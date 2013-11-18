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
#include <fstream>
#include "boost/filesystem/fstream.hpp"
#include <boost/filesystem/operations.hpp>

namespace po = boost::program_options;

static string statusString(bool killed, const string& contents) {
    std::stringstream ss;
    if (killed) {
        ss << "KILLED";
        if (contents.size() != 0) {
            ss << " ('" << contents << "')";
        }
    } else {
        ss << "OK";
    }
    return ss.str();
}

static string readInKillFileContents(const string& path) {
    string out;
    std::ifstream stream(path.c_str());
    if (!stream.good()) {
        mongo::log() << "kill file exists, but unable to read its contents" << std::endl;
    } else {
        std::stringstream ss;
        ss << stream.rdbuf();
        out = ss.str();
        // STL's weird way of removing all \n's from a string.
        out.erase(remove(out.begin(), out.end(), '\n'), out.end());
    }
    stream.close();
    return out;
}

namespace mongo {

    KillFileWatcher::KillFileWatcher()
        : Module("KillFileWatcher"), _path(), _killFileShouldTriggerStepDown(false),
        _isKilled(false), _killFileContents(), _numChecksSinceChange(0),
        _timeOfLastChange(0), _hasSteppedDown(false), _lock("KillFileWatcher") {

            add_options()
                ( "kill-file-path" ,
                  po::value<string>() ,
                  "absolute path of kill-file to watch for health checking. if unset, kill.<port> under dbpath will be monitored" )
                ( "kill-file-should-trigger-step-down" ,
                  "if specified, then the presence of a kill-file will tell the mongod to step down, if it's the master.")
                ;
        }

    KillFileWatcher::~KillFileWatcher() {}

    bool KillFileWatcher::isKilled() const {
        rwlock lk(_lock, false);
        return _isKilled;
    }
    bool KillFileWatcher::isForcedToNotBePrimary() const {
        rwlock lk(_lock, false);
        if (_isKilled && _killFileShouldTriggerStepDown) {
            return true;
        } else {
            return false;
        }
    }
    string KillFileWatcher::contentsOfKillFile() const {
        rwlock lk(_lock, false);
        return _killFileContents;
    }

    void KillFileWatcher::appendHealthStatus( BSONObjBuilder& result ) {
        BSONObjBuilder health;

        string msg = "healthy";
        bool healthy = true;

        bool killFileExists = isKilled();
        if (killFileExists) {
            healthy = false;
            string killFileContents = contentsOfKillFile();
            if (killFileContents.size() == 0) {
                msg = "kill file is present";
            } else {
                msg = "kill file is present: " + killFileContents;
            }
        }

        health.append("ok", healthy);
        health.append("msg", msg);
        health.append("killFile", killFileExists);

        result.append("healthStatus", health.obj());
    }

    bool KillFileWatcher::config( boost::program_options::variables_map& params ) {
        rwlock lk(_lock, true);

        if (params.count("kill-file-path") > 0) {
            _path = params["kill-file-path"].as<string>();

            // in newer versions odf boost, is_complete is renamed to is_absolute, but we don't have that yet.
            if (!boost::filesystem::path(_path).is_complete()) {
                LOG(LL_ERROR) << "kill-file-path must be absolute! bailing since we got " << _path << endl;
                return false;
            }
        } else {
            stringstream ss;
            ss << "kill." << cmdLine.port;
            _path = (boost::filesystem::path(dbpath) / ss.str()).c_str();
        }

        if (params.count("kill-file-should-trigger-step-down") > 0) {
            _killFileShouldTriggerStepDown = true;
        }

        return true;
    }

    string KillFileWatcher::name() const { return "KillFileWatcher"; }

    void KillFileWatcher::tryStepDownIfApplicable_inWriteLock() {
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

    void KillFileWatcher::handleChange_inWriteLock(bool oldValue, bool newValue) {
        _hasSteppedDown = false;
        _numChecksSinceChange = 0;
        _timeOfLastChange = Listener::getElapsedTimeMillis();
        _isKilled = newValue;

        string oldContents = _killFileContents;
        _killFileContents = "";

        if (newValue) {
            _killFileContents = readInKillFileContents(_path);
        }

        log() << "kill file status changed! "
            << "before: " << statusString(oldValue, oldContents) << ". "
            << "now: " << statusString(newValue, _killFileContents)
            << endl;

        if (newValue) {
            tryStepDownIfApplicable_inWriteLock();
        }
    }

    void KillFileWatcher::handleKilled_inWriteLock() {
        ++_numChecksSinceChange;
        if ((_numChecksSinceChange % 60) == 0) {
            tryStepDownIfApplicable_inWriteLock();

            log() << "kill file has existed for "
                << ((Listener::getElapsedTimeMillis() - _timeOfLastChange) / 1000)
                << " seconds. "
                << statusString(_isKilled, _killFileContents)
                << endl;

        }

        // Update the contents of the string in case the reason changes over
        // time but the file stays around.
        _killFileContents = readInKillFileContents(_path);
    }

    void KillFileWatcher::run() {
        if (_path.size() == 0) {
            log() << "KillFileWatcher not configured" << endl;
            return;
        }

        log() << "KillFileWatcher starting and monitoring path " << _path << endl;

        Client::initThread("KillFileWatcher");
        Client& c = cc();

        while ( ! inShutdown() ) {
            sleepsecs( 1 );

            try {
                bool previousValue = isKilled();
                bool newValue = boost::filesystem::exists(_path);
                if (newValue || (newValue != previousValue)) {
                    rwlock lk(_lock, true);

                    if (newValue != previousValue) {
                        handleChange_inWriteLock(previousValue, newValue);
                    }

                    if (newValue) {
                        handleKilled_inWriteLock();
                    }
                }
            }
            catch ( std::exception& e ) {
                LOG(LL_ERROR) << "KillFileWatcher exception: " << e.what() << endl;
            }
        }

        c.shutdown();
    }

    void KillFileWatcher::init() {
        go();
    }

    void KillFileWatcher::shutdown() {
        // TODO
    }

    KillFileWatcher killFileWatcher;
}



