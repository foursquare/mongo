// bsonsampler.cpp

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
#include <time.h>
#include <stdlib.h>

#include <fcntl.h>

using namespace mongo;

namespace po = boost::program_options;

class BSONSampler : public BSONTool {
    double _samplingRate;
    FILE* _outputFile;
public:

    BSONSampler() : BSONTool( "bsonsampler", NONE ) {
        add_options()
        ("r" , po::value<string>()->default_value("0.5") , "sampling rate." )
        ;
        add_hidden_options()
        ("file" , po::value<string>() , ".bson file" )
        ;
        addPositionArg( "file" , 1 );
        _noconnection = true;
    }

    virtual void printExtraHelp(ostream& out) {
        out << "Sample BSON file.\n" << endl;
        out << "usage: " << _name << " [-r sampling_rate] <bson filename>" << endl;
    }

    long long processFile(FILE* file) {
        unsigned long long read = 0;
        unsigned long long num = 0;
        
        const int BUF_SIZE = BSONObjMaxUserSize + ( 1024 * 1024 );
        boost::scoped_array<char> buf_holder(new char[BUF_SIZE]);
        char * buf = buf_holder.get();

        while (!feof(file) ) {
            size_t amt = fread(buf, 1, 4, file);
            if( amt == 0) {
                if(feof(file)){
                    break;
                }
            }
            assert( amt == 4 );
            
            int size = ((int*)buf)[0];
            uassert( 15922 , str::stream() << "invalid object size: " << size , size < BUF_SIZE );
            
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
        srand ( time(NULL) );
        {
            string r = getParam( "r" );
            if(r == "") {
                cerr << "Missing sampling rate parameter." << endl;
                return 1;
            } else {
                _samplingRate = atof(r.c_str());
                if(_samplingRate < 0.0f || _samplingRate > 1.0f) {
                    cerr  << "Not between 0 and 1: "<< r << endl;
                    return 1;
                }
            }
            string o = getParam( "o" );
            if(o == "") {
                _outputFile = stdout;
            } else {
                _outputFile = fopen(o.c_str(), "wb");
                uassert(15923, errnoWithPrefix("couldn't open file"), _outputFile);
            }
        }

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
        if(_outputFile != NULL ) {
            fclose(_outputFile);
            _outputFile = NULL;
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
        int random = rand();
        if((random*1.0f/RAND_MAX) < _samplingRate) {
            size_t toWrite = o.objsize();
            size_t written = 0;
            while( toWrite ) {
                size_t ret = fwrite( o.objdata()+written, 1, toWrite, _outputFile );
                uassert(15924, errnoWithPrefix("couldn't write to file"), ret);
                toWrite -= ret;
                written += ret;
            }
        }
    }
};

int main( int argc , char ** argv ) {
    BSONSampler sampler;
    return sampler.main( argc , argv );
}

