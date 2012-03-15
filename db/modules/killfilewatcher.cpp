// @file KillFileWatcher.cpp
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
#include "killfilewatcher.h"
#include "../db.h"
#include "../instance.h"
#include "../module.h"
#include "../repl/rs.h"
#include "../commands.h"
#include "../../util/background.h"
#include "../../util/log.h"
#include "../../util/net/listen.h"

namespace po = boost::program_options;

namespace mongo {

    KillFileWatcher::KillFileWatcher()
      : Module( "KillFileWatcher" ), _path( "" ), _killFileShouldTriggerStepDown(false),
        _isKilled(false), _numChecksSinceChange(0), _timeOfLastChange(0),
        _hasSteppedDown(false) {

      add_options()
         ( "kill-file-path" ,
           po::value<string>() ,
           "absolute path of kill-file to watch for health checking. if unset, kill.<port> under dbpath will be monitored" )
         ( "kill-file-should-trigger-step-down" ,
           "if specified, then the presence of a kill-file will tell the mongod to step down, if it's the master.")
         ;
    }

   KillFileWatcher::~KillFileWatcher() {}

   bool KillFileWatcher::isKilled() const { return _isKilled; }
   bool KillFileWatcher::isForcedToNotBePrimary() const {
       if (_isKilled && _killFileShouldTriggerStepDown) {
           return true;
       } else {
           return false;
       }
   }

   bool KillFileWatcher::config( program_options::variables_map& params ) {
      if (params.count("kill-file-path") > 0) {
        _path = params["kill-file-path"].as<string>();

        // in newer versions odf boost, is_complete is renamed to is_absolute, but we don't have that yet.
        if (!boost::filesystem::path(_path).is_complete()) {
          log(LL_ERROR) << "kill-file-path must be absolute! bailing since we got " << _path << endl;
          return false;
        }
      } else {
        stringstream ss;
        ss << "kill." << cmdLine.port;
        _path = (boost::filesystem::path(dbpath) / ss.str()).native_file_string();
      }

      if (params.count("kill-file-should-trigger-step-down") > 0) {
        _killFileShouldTriggerStepDown = true;
      }

      return true;
   }

    string KillFileWatcher::name() const { return "KillFileWatcher"; }

    void KillFileWatcher::tryStepDownIfApplicable() {
        if (_isKilled && replSet && theReplSet && theReplSet->isPrimary() &&
            _killFileShouldTriggerStepDown && !_hasSteppedDown) {
            // step down
            string errmsg;
            BSONObjBuilder unusedBuilder;
            if (!theReplSet->isSafeToStepDown(errmsg, unusedBuilder)) {
                log() << "kill file is present but we can't step down because it's unsafe: "
                      << errmsg << ". will try again in a minute."
                      << endl;
            } else {
                log() << "stepping down as master for 60s due to presence of kill file!" << endl;
                _hasSteppedDown = theReplSet->stepDown(60);
                if (!_hasSteppedDown) {
                  log() << "failed to step down as master. will try again in a minute." << endl;
                }
            }
        }
    }

    void KillFileWatcher::run() {
        if ( _path.size() == 0 ) {
            log() << "KillFileWatcher not configured" << endl;
            return;
        }

        log() << "kill-file monitor starting and monitoring path " << _path << endl;

        Client::initThread( "KillFileWatcher" );

        while ( ! inShutdown() ) {
            sleepsecs( 1 );

            try {
                bool previousValue = _isKilled;
                _isKilled = boost::filesystem::exists(_path);
                if (_isKilled != previousValue) {
                    _hasSteppedDown = false;
                    _numChecksSinceChange = 0;
                    _timeOfLastChange = Listener::getElapsedTimeMillis();

                    log() << "kill file status changed! "
                          << "before: " << (previousValue ? "KILLED. " : "OK. ")
                          << "now: " << (_isKilled ? "KILLED. " : "OK. ")
                          << endl;

                    if (_isKilled) {
                      tryStepDownIfApplicable();
                    }
                } else if (_isKilled) {
                    tryStepDownIfApplicable();

                    ++_numChecksSinceChange;
                    if ((_numChecksSinceChange % 60) == 0) {
                        log() << "still in KILLED state. kill file has existed for "
                              << ((Listener::getElapsedTimeMillis() - _timeOfLastChange) / 1000)
                              << " seconds" << endl;
                    }
                }
            }
            catch ( std::exception& e ) {
                log(LL_ERROR) << "KillFileWatcher exception: " << e.what() << endl;
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



