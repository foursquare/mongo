// @file killfilewatcher.cpp
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
#include "../../util/log.h"
#include "../commands.h"
#include "killfilewatcher.h"

namespace po = boost::program_options;

namespace mongo {

    KillFileWatcher::KillFileWatcher()
      : Module( "killfilewatcher" ) , _path( "" ), _fileExists(false) {

      add_options()
         ( "kill-file-path" , po::value<string>() , "absolute path of kill-file to watch for health checking. if unset, no kill-file is monitored" )
         ;
    }

   KillFileWatcher::~KillFileWatcher() {}

   bool KillFileWatcher::fileExists() const { return _fileExists; }

   bool KillFileWatcher::config( program_options::variables_map& params ) {
      if (params.count("kill-file-path") > 0) {
        _path = params["kill-file-path"].as<string>();

        // in newer versions of boost, is_complete is renamed to is_absolute, but we don't have that yet.
        if (!boost::filesystem::path(_path).is_complete()) {
          log(LL_ERROR) << "kill-file-path must be absolute! bailing since we got " << _path << endl;
          return false;
        }
      }

      return true;
   }

    string KillFileWatcher::name() const { return "killfilewatcher"; }

    void KillFileWatcher::run() {
        if ( _path.size() == 0 ) {
            log() << "KillFileWatcher not configured" << endl;
            return;
        }

        log() << "kill-file monitor starting and monitoring path " << _path << endl;

        while ( ! inShutdown() ) {
            sleepsecs( 1 );

            try {
                bool previousValue = _fileExists;
                _fileExists = boost::filesystem::exists(_path);
                if (_fileExists != previousValue) {
                  log() << "kill file status changed! "
                        << "before: " << (previousValue ? "KILLED. " : "OK. ")
                        << "now: " << (_fileExists ? "KILLED. " : "OK. ")
                        << endl;
                }
            }
            catch ( std::exception& e ) {
                log(LL_ERROR) << "killfilewatcher exception: " << e.what() << endl;
            }
        }
    }

    void KillFileWatcher::init() {
      go();
    }

    void KillFileWatcher::shutdown() {
        // TODO
    }

    KillFileWatcher killFileWatcher;
}



