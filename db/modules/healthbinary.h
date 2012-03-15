// @file healthbinary.h
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


#include "pch.h"
#include "../db.h"
#include "../instance.h"
#include "../module.h"
#include "../../util/background.h"
#include "../commands.h"

namespace mongo {

    /** Kill file watcher.
      If enabled, this checks for existence of a kill file.
      */
    class HealthBinary : public BackgroundJob , Module {
        public:

            HealthBinary();
            ~HealthBinary();

            bool ok() const;
            string message() const;
            time_t lastRunMs() const;
            time_t lastRunTimestamp() const;

            bool config( program_options::variables_map& params );

            string name() const;

            void run();

            void init();

            void shutdown();

        private:
            string _path;
            bool _ok;
            string _message;
            time_t _lastRunMs;
            time_t _lastRunTimestamp;
    };

    extern HealthBinary healthBinary;
}
