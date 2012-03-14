// @file touchfile.h
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



#include "../module.h"
#include "../../util/background.h"

namespace mongo {

    /** touchfile writer.
        will periodically write to a <dbpath>/touchfile to measure time
        as a proxy for disk health
    */
    class TouchFile : public BackgroundJob , Module {
    public:

       TouchFile();
       ~TouchFile();

       time_t lastTouchElapsedMs() const;
       time_t lastTouchTimestamp() const;

       bool config( program_options::variables_map& params );

       string name() const;

       void run();

       void init();

       void shutdown();

    private:
       time_t _lastTouchElapsedMs;
       time_t _lastTouchTimestamp;
    };

    extern TouchFile touchFile;
}
