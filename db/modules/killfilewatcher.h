// @file killfilewatcher.h
/*
 *    Copyright (C) 2010 10gen Inc.
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
    class KillFileWatcher : public BackgroundJob , Module {
    public:

       KillFileWatcher();
       ~KillFileWatcher();

       bool fileExists() const;

       void config( program_options::variables_map& params );

       string name() const;

       void run();

       void init();

       void shutdown();

    private:
       string _path;
       bool _fileExists;

    };

    extern KillFileWatcher killFileWatcher;
}
