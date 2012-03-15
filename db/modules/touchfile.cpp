// @file touchfile.cpp
/*
 *    Copyright (C) 2012 10gen Inc.
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


#include "../db.h"
#include "../module.h"
#include "../../util/background.h"
#include "../../util/file.h"
#include "../../util/log.h"
#include "../../util/timer.h"
#include "touchfile.h"

namespace po = boost::program_options;

namespace mongo {

    TouchFile::TouchFile()
        : Module( "touchfile" ), _lastTouchElapsedMs(0), _lastTouchTimestamp(0)  {
        }

    TouchFile::~TouchFile() {}

    string TouchFile::name() const { return "touchfile"; }

    time_t TouchFile::lastTouchElapsedMs() const { return _lastTouchElapsedMs; }
    time_t TouchFile::lastTouchTimestamp() const { return _lastTouchTimestamp; }

    bool TouchFile::config( program_options::variables_map& params ) {
        return true;
    }

    void TouchFile::run() {
        string path = dbpath + "/touchfile";
        log() << "touchfile monitor starting and monitoring path " << path << endl;

        while ( ! inShutdown() ) {
            sleepsecs( 1 );

            try {
                Timer t;
                File f;
                // 4 chunks of 256kb.  should hit every stripe in a 4 drive raid0
                const unsigned BLKSZ = 256 * 1024;
                const unsigned CHUNKS = 4;
                f.open( path.c_str() , /*read-only*/ false , /*direct-io*/ false );
                assert( f.is_open() );

                if ( f.len() < CHUNKS * BLKSZ ) {
                    // file is new, write out the whole thing
                    fileofs loc = 0;
                    char mybuffer[ BLKSZ ];
                    memset(mybuffer, 0, sizeof(mybuffer));
                    while ( loc / BLKSZ < CHUNKS ) {
                        f.write( loc , mybuffer , BLKSZ );
                        loc += BLKSZ;
                    }
                } else {
                    // file already exists, so do sparse writes
                    unsigned i = 0;
                    const unsigned SMALL_BLKSZ = 4;
                    char mybuffer[ SMALL_BLKSZ ];
                    memset(mybuffer, 1, sizeof(mybuffer));
                    while ( i < CHUNKS ) {
                        f.write( i * BLKSZ , mybuffer , SMALL_BLKSZ );
                        i++;
                    }
                }

                f.fsync();
                _lastTouchElapsedMs = t.millis();
                _lastTouchTimestamp = time(0);
            }
            catch ( std::exception& e ) {
                log(LL_ERROR) << "touchfile exception: " << e.what() << endl;
            }
        }
    }

    void TouchFile::init() {
        go();
    }

    void TouchFile::shutdown() {
        // TODO
    }

    TouchFile touchFile;
}



