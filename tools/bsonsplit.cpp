// bsonsplit.cpp

/**
*    Copyright (C) 2008 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../pch.h"
#include "../client/dbclient.h"
#include "../util/mmap.h"
#include "../util/text.h"
#include "tool.h"

#include <boost/program_options.hpp>

#include <fcntl.h>

#include "lzo_compressor.h"

using namespace mongo;

namespace po = boost::program_options;

class BSONSplit : public BSONTool {
    string _prefix;
    unsigned long _bytes;
    static const long DefaultBytes = 1000000000;
    unsigned long _byteCount;
    unsigned int _fileCount;
    FILE* _outputFile;
    bool _compress;
    LZOCompressor compressor;
    
public:

    BSONSplit() : BSONTool( "bsonsplit", NONE ) {
        add_options()
        ("b" , po::value<string>()->default_value("1000000000") , "size of file." )
        ;
        add_options()
        ("p" , po::value<string>()->default_value("x") , "prefix of output files." )
        ;
        add_options()
        ("c" , po::value<string>()->default_value("") , "compression level." )
        ;
        add_hidden_options()
        ("file" , po::value<string>() , ".bson file" )
        ;
        addPositionArg( "file" , 1 );
        _noconnection = true;
        _compress = true;
    }

    virtual void printExtraHelp(ostream& out) {
        out << "Split BSON file into smaller ones.\n" << endl;
        out << "usage: " << _name << " [-b bytes][-p prefix][-c [1|9]] <bson filename>" << endl;
    }

    long long processFile(FILE* file) {
        unsigned long long read = 0;
        unsigned long long num = 0;
        
        const int BUF_SIZE = BSONObjMaxUserSize + ( 1024 * 1024 );
        boost::scoped_array<char> buf_holder(new char[BUF_SIZE]);
        char * buf = buf_holder.get();

        while ( !feof(file) ) {
            size_t amt = fread(buf, 1, 4, file);
            if( amt != 4) {
                if(feof(file)){
                    break;
                }
            }
            assert( amt == 4 );
            
            int size = ((int*)buf)[0];
            uassert( 15921 , str::stream() << "invalid object size: " << size , size < BUF_SIZE );
            
            amt = fread(buf+4, 1, size-4, file);
            assert( amt == (size_t)( size - 4 ) );
            
            BSONObj o( buf );

            gotObject( o );
            read += o.objsize();
            num++;
        }
        
        (_usesstdout ? cout : cerr ) << num << " objects." << endl;
        return num++;
    }
    
    
    virtual int doRun() {
        {
            string p = getParam( "p" );
            if( p == "" ) {
                _prefix = "x";
            } else {
                _prefix = p;
            }
            string c = getParam( "c" );
            if( c == "" ) {
                _compress = false;
            } else {
                _compress = true;
                int level = atoi(c.c_str());
                if(level != 1 && level != 9) {
                    cerr << "-c compression level: " << level << " is not 1 or 9" << endl;
                    return 1;
                }
                compressor.setCompressionLevel(level);
            }
            string b = getParam( "b" );
            if(b == "") {
                _bytes = DefaultBytes;
            } else {
                _bytes = atol(b.c_str());
                if(_bytes <= 0 ) {
                    cerr  << "Not a positive number: "<< b << endl;
                    return 1;
                }
            }
        }

        _byteCount  = 0;
        _fileCount = 0;
        _outputFile = NULL;
        path root = getParam( "file" );
        if ( root == "" ) {
            processFile(stdin);
        } else {
            string inputFileName = root.string();
            
            unsigned long long fileLength = file_size( root );
            
            if ( fileLength == 0 ) {
                out() << "file " << inputFileName << " empty, skipping" << endl;
                return 0;
            }
            
            
            FILE* file = fopen( inputFileName.c_str() , "rb" );
            if ( ! file ) {
                log() << "error opening file: " << inputFileName << " " << errnoWithDescription() << endl;
                return 0;
            }
            processFile( file );
            fclose( file );
        }
        if(_compress) {
            compressor.end();
        }
        if(_outputFile != NULL ) {
            fclose(_outputFile);
        }
        return 0;
    }

    bool debug( const BSONObj& o , int depth=0) {
        string prefix = "";
        for ( int i=0; i<depth; i++ ) {
            prefix += "\t\t\t";
        }

        int read = 4;

        try {
            cout << prefix << "--- new object ---\n";
            cout << prefix << "\t size : " << o.objsize() << "\n";
            BSONObjIterator i(o);
            while ( i.more() ) {
                BSONElement e = i.next();
                cout << prefix << "\t\t " << e.fieldName() << "\n" << prefix << "\t\t\t type:" << setw(3) << e.type() << " size: " << e.size() << endl;
                if ( ( read + e.size() ) > o.objsize() ) {
                    cout << prefix << " SIZE DOES NOT WORK" << endl;
                    return false;
                }
                read += e.size();
                try {
                    e.validate();
                    if ( e.isABSONObj() ) {
                        if ( ! debug( e.Obj() , depth + 1 ) )
                            return false;
                    }
                    else if ( e.type() == String && ! isValidUTF8( e.valuestr() ) ) {
                        cout << prefix << "\t\t\t" << "bad utf8 String!" << endl;
                    }
                    else if ( logLevel > 0 ) {
                        cout << prefix << "\t\t\t" << e << endl;
                    }

                }
                catch ( std::exception& e ) {
                    cout << prefix << "\t\t\t bad value: " << e.what() << endl;
                }
            }
        }
        catch ( std::exception& e ) {
            cout << prefix << "\t" << e.what() << endl;
        }
        return true;
    }

    virtual void gotObject( const BSONObj& o ) {
        if( _byteCount ==0 || _byteCount > _bytes ) {
            if(_byteCount != 0 && _compress) {
                compressor.end();
            }
            if(_outputFile != NULL) {
                fclose(_outputFile);
            }
            char buffer [10];
            sprintf(buffer, "%05u", _fileCount);
            string outputFileName = _prefix + buffer + ".bson" + (_compress ? ".lzo" : "");
            if(!_compress) {
                _outputFile = fopen(outputFileName.c_str(), "wb");
                uassert(15919, errnoWithPrefix("couldn't open file"), _outputFile);
            } else {
                compressor.start(outputFileName.c_str());
            }
            _fileCount += 1;
            _byteCount = 0;
            (_usesstdout ? cout : cerr ) << "Outputting to file: " << outputFileName << endl;
        }
        _byteCount += o.objsize();
        size_t toWrite = o.objsize();
        size_t written = 0;
        while( toWrite ) {
            size_t ret = 0;
            if(!_compress) {
                ret = fwrite( o.objdata()+written, 1, toWrite, _outputFile );
                uassert(15920, errnoWithPrefix("couldn't write to file"), ret);
            } else {
                compressor.put(o.objdata()+written, toWrite);
                ret = toWrite;
            }
            toWrite -= ret;
            written += ret;
        }
    }
};

int main( int argc , char ** argv ) {
    BSONSplit split;
    return split.main( argc , argv );
}

