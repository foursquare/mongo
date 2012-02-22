// lzo_compressor.h
#include <stdio.h>
#include <iostream>
#include <time.h>
#include "lzo/lzoconf.h"
#include "lzo/lzo1x.h"

/* portability layer */
static const char *progname = NULL;
#define WANT_LZO_MALLOC 1
#define WANT_LZO_FREAD 1
#define WANT_LZO_WILDARGV 1
#define WANT_XMALLOC 1
#include "portab.h"

using namespace std;

class LZOCompressor {
public:
    LZOCompressor():
        fo(NULL),
        r(0),
        in(NULL),
        out(NULL),
        wrkmem(NULL),
        in_len(0),
        out_len(0),
        wrk_len(0),
        flags(1), 
        method(1)
    { }

    void start(const char *out_name) {
        fo = NULL,
        r = 0,
        in = NULL,
        out = NULL,
        wrkmem = NULL,
        in_len = 0,
        out_len = 0,
        wrk_len = 0,
        flags = 1, 
        method = 1;
        
        fo = xopen_fo(out_name);
        /*
         * Step 1: initialize the LZO library
         */
        if (lzo_init() != LZO_E_OK)
        {
            cerr << "internal error - lzo_init() failed !!!\n" << endl;
            cerr << "(this usually indicates a compiler bug - try recompiling\nwithout optimizations, and enable '-DLZO_DEBUG' for diagnostics)\n" << endl;
            exit(1);
        }
        
        /*
         * Step 2: write magic header, flags & block size, init checksum
         */
        block_size = 64 * 1024L;
        vector<unsigned char> header;
        
        xwrite(fo, magic, sizeof(magic));
        
        int lzop_version = 0x1010;
        xwrite16(fo, lzop_version);
        xwrite16buffer(header, lzop_version);
        
        int lzo_version = 0x2060;
        xwrite16(fo, lzo_version);
        xwrite16buffer(header, lzo_version);
        
        int lzop_compat_version = 0x0940;
        xwrite16(fo, lzop_compat_version);
        xwrite16buffer(header, lzop_compat_version);
        
        
        // lzo_strategy: LZO1X_1 
        xwrite8(fo, 1);
        xwrite8buffer(header, 1);
        
        xwrite8(fo, 5);
        xwrite8buffer(header, 5);
        
        xwrite32(fo, 0); // all flags 0
        xwrite32buffer(header, 0);
        
        xwrite32(fo, 0x81A4); // mode
        xwrite32buffer(header, 0x81A4);
        
        int ct = (int)time(NULL);
        xwrite32(fo, ct); // mtime
        xwrite32buffer(header, ct);
        
        xwrite32(fo, 0); // gmtdiff ignored
        xwrite32buffer(header, 0);
        
        xwrite8(fo, 0);     // no filename
        xwrite8buffer(header, 0);
        
        lzo_uint32 header_checksum = lzo_adler32(0, NULL, 0);
        header_checksum = lzo_adler32(header_checksum, &header.front(), header.size());  
        
        xwrite32(fo, header_checksum);

        checksum = lzo_adler32(0, NULL, 0);
        
        
        total_in = total_out = 0;
        
        /*
         * Step 2: allocate compression buffers and work-memory
         */
        in = (lzo_bytep) xmalloc(block_size);
        out = (lzo_bytep) xmalloc(block_size + block_size / 16 + 64 + 3);
        if (compression_level == 9)
            wrk_len = LZO1X_999_MEM_COMPRESS;
        else
            wrk_len = LZO1X_1_MEM_COMPRESS;
        wrkmem = (lzo_voidp) xmalloc(wrk_len);
        if (in == NULL || out == NULL || wrkmem == NULL)
        {
            cerr << progname << ": out of memory\n";
            goto err;
        }
        return;
        
    err:
        xclose(fo); 
        fo = NULL;
        lzo_free(wrkmem);
        lzo_free(out);
        lzo_free(in);
        wrkmem = NULL;
        out = NULL;
        in = NULL;
        exit(1);
    }

    void end() {
        if(in_len > 0) {
            compress();
            in_len = 0;
        }
        if (fo != NULL) {
            /* write EOF marker */
            xwrite32(fo, 0);
            
            /* write checksum */
//            if (flags & 1)
//                xwrite32(fo, checksum);
            xclose(fo);
            fo = NULL;
        }
        lzo_free(wrkmem);
        lzo_free(out);
        lzo_free(in);
        wrkmem = NULL;
        out = NULL;
        in = NULL;
        printf("total in: %ld, total_out: %ld\n", total_in, total_out);
    }
    
    void put(const char* buffer, size_t len) {
        size_t toPut = len;
        while((in_len+toPut) >= block_size) {
            size_t toFill = block_size - in_len;
            memcpy(in+in_len, buffer, toFill);
            buffer += (toFill);
            in_len = block_size;
            compress();
            in_len = 0;
            toPut -= toFill;
        }
        if(toPut > 0) {
            memcpy(in+in_len, buffer, toPut);
            buffer += toPut;
            in_len += toPut;
        }
    }
    
    void setCompressionLevel(int level) {
        compression_level = level;
    }

private:
    FILE* fo;

    int r;
    lzo_bytep in;
    lzo_bytep out;
    lzo_voidp wrkmem;
    lzo_uint in_len;
    lzo_uint out_len;
    lzo_uint32 wrk_len;
    lzo_uint32 flags;       /* = 1 do compute a checksum */
    int method;             /* = 1 compression method: LZO1X */
    lzo_uint32 checksum;
    lzo_uint block_size;
    int compression_level;
    
    unsigned long total_in;
    unsigned long total_out;
    lzo_bool opt_debug;
    
    /* magic file header for lzop files */
    static const unsigned char magic[9];
    
    void compress()  {
        total_in += in_len;
        /* update checksum */
        if (flags & 1)
            checksum = lzo_adler32(checksum, in, in_len);
        
        /* clear wrkmem (not needed, only for debug/benchmark purposes) */
        if (opt_debug)
            lzo_memset(wrkmem, 0xff, wrk_len);
        
        /* compress block */
        if (compression_level == 9)
            r = lzo1x_999_compress(in, in_len, out, &out_len, wrkmem);
        else
            r = lzo1x_1_compress(in, in_len, out, &out_len, wrkmem);
        if (r != LZO_E_OK || out_len > in_len + in_len / 16 + 64 + 3)
        {
            /* this should NEVER happen */
            cerr << "internal error - compression failed: " << r << endl;
            r = 2;
            goto err;
        }
        
        /* write uncompressed block size */
        xwrite32(fo, in_len);
        total_out += out_len;
        if (out_len < in_len)
        {
            /* write compressed block */
            xwrite32(fo, out_len);
            xwrite(fo, out, out_len);
        }
        else
        {
            /* not compressible - write uncompressed block */
            xwrite32(fo, in_len);
            xwrite(fo, in, in_len);
        }
        return;
        
    err:
        xclose(fo); 
        fo = NULL;
        lzo_free(wrkmem);
        lzo_free(out);
        lzo_free(in);
        wrkmem = NULL;
        out = NULL;
        in = NULL;
        exit(1);
    }
    
    /*************************************************************************
     // file IO
     **************************************************************************/
    
    lzo_uint xread(FILE *fp, lzo_voidp buf, lzo_uint len, lzo_bool allow_eof) {
        lzo_uint l;
        
        l = (lzo_uint) lzo_fread(fp, buf, len);
        if (l > len)
        {
            fprintf(stderr, "\nsomething's wrong with your C library !!!\n");
            exit(1);
        }
        if (l != len && !allow_eof)
        {
            fprintf(stderr, "\nread error - premature end of file\n");
            exit(1);
        }
        total_in += (unsigned long) l;
        return l;
    }

    lzo_uint xwrite(FILE *fp, const lzo_voidp buf, lzo_uint len) {
        if (fp != NULL && lzo_fwrite(fp, buf, len) != len)
        {
            fprintf(stderr, "\nwrite error  (disk full ?)\n");
            exit(1);
        }
        total_out += (unsigned long) len;
        return len;
    }

    int xgetc(FILE *fp) {
        unsigned char c;
        xread(fp, (lzo_voidp) &c, 1, 0);
        return c;
    }

    void xputc(FILE *fp, int c) {
        unsigned char cc = (unsigned char) (c & 0xff);
        xwrite(fp, (const lzo_voidp) &cc, 1);
    }

    /* read and write portable 32-bit integers */
    
    lzo_uint32 xread32(FILE *fp) {
        unsigned char b[4];
        lzo_uint32 v;
        
        xread(fp, b, 4, 0);
        v  = (lzo_uint32) b[3] <<  0;
        v |= (lzo_uint32) b[2] <<  8;
        v |= (lzo_uint32) b[1] << 16;
        v |= (lzo_uint32) b[0] << 24;
        return v;
    }

    
    void xwrite32(FILE *fp, lzo_xint v) {
        unsigned char b[4];
        
        b[3] = (unsigned char) ((v >>  0) & 0xff);
        b[2] = (unsigned char) ((v >>  8) & 0xff);
        b[1] = (unsigned char) ((v >> 16) & 0xff);
        b[0] = (unsigned char) ((v >> 24) & 0xff);
        xwrite(fp, b, 4);
    }
    
    void xwrite16(FILE *fp, lzo_xint v) {
        unsigned char b[2];
        
        b[1] = (unsigned char) ((v >>  0) & 0xff);
        b[0] = (unsigned char) ((v >>  8) & 0xff);
        xwrite(fp, b, 2);
    }
    
    void xwrite8(FILE *fp, lzo_xint v) {
        unsigned char b[1];
        
        b[0] = (unsigned char) ((v >>  0) & 0xff);
        xwrite(fp, b, 1);
    }
    
    void xwrite32buffer(vector<unsigned char> &header, lzo_xint v) {
        unsigned char b[4];
        
        b[3] = (unsigned char) ((v >>  0) & 0xff);
        b[2] = (unsigned char) ((v >>  8) & 0xff);
        b[1] = (unsigned char) ((v >> 16) & 0xff);
        b[0] = (unsigned char) ((v >> 24) & 0xff);
        header.push_back(b[0]);
        header.push_back(b[1]);
        header.push_back(b[2]);
        header.push_back(b[3]);
    }
    
    void xwrite16buffer(vector<unsigned char> &header, lzo_xint v) {
        unsigned char b[2];
        
        b[1] = (unsigned char) ((v >>  0) & 0xff);
        b[0] = (unsigned char) ((v >>  8) & 0xff);
        header.push_back(b[0]);
        header.push_back(b[1]);
    }
    
    void xwrite8buffer(vector<unsigned char> &header, lzo_xint v) {
        unsigned char b[1];
        
        b[0] = (unsigned char) ((v >>  0) & 0xff);
        header.push_back(b[0]);
    }
    
    

    /* open output file */
    static FILE *xopen_fo(const char *name) {
        FILE *fp;
#if 0        
        /*make sure we don't overwrite a file */
        fp = fopen(name, "rb");
        if (fp != NULL)
        {
            cerr << progname << ": file " << name << " already exists -- not overwritten\n";
            fclose(fp); fp = NULL;
            exit(1);
        }
#endif        
        fp = fopen(name, "wb");
        if (fp == NULL)
        {
            cerr << progname << ": cannot open output file " << name << endl;
            exit(1);
        }
        return fp;
    }
    
    /* close file */
    static void xclose(FILE *fp) {
        if (fp)
        {
            int err;
            err = ferror(fp);
            if (fclose(fp) != 0)
                err = 1;
            if (err)
            {
                cerr << progname << ": error while closing file\n";
                exit(1);
            }
        }
    }
};

const unsigned char LZOCompressor::magic[] = { -119, 'L', 'Z', 'O', 0, '\r', '\n', '\032', '\n'};


