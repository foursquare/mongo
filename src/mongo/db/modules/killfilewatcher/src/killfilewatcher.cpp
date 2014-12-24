/*    Copyright 2014 Foursquare Labs, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kControl

#include "mongo/platform/basic.h"

#include "killfilewatcher.h"

#include <string>
#include <fstream>
#include "boost/filesystem/fstream.hpp"
#include <boost/filesystem/operations.hpp>

#include "mongo/base/init.h"
#include "mongo/db/db.h"            // see HORRIBLE HACK below
#include "mongo/db/repl/rs.h"
#include "mongo/util/log.h"
#include "mongo/util/options_parser/option_description.h"

#include "killfilewatcher_options.h"

static std::string statusString(bool killed, const std::string& contents) {
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

static std::string readInKillFileContents(const std::string& path) {
    std::string out;
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

    namespace moe = mongo::optionenvironment;

    KillFileWatcherAgent killfileAgent;

    KillFileWatcherAgent::KillFileWatcherAgent() : _isKilled(false), _killFileContents(), _numChecksSinceChange(0),
        _timeOfLastChange(0), _hasSteppedDown(false), _lock("KillFileWatcher") {
    }

    KillFileWatcherAgent::~KillFileWatcherAgent() {
    }

    std::string KillFileWatcherAgent::name() const {
        return "KillFileWatcherAgent";
    }

    void KillFileWatcherAgent::shutdown() {
        killfileParams.enabled = 0;
    }

    void KillFileWatcherAgent::_init() {
        Client::initThread("KillFileWatcherAgent");
    }

    void KillFileWatcherAgent::init() {
        killfileAgent.go();
    }

    bool KillFileWatcherAgent::isKilled() const {
        rwlock lk(_lock, false);
        return _isKilled;
    }
    bool KillFileWatcherAgent::isForcedToNotBePrimary() const {
        rwlock lk(_lock, false);
        if (_isKilled && killfileParams.triggerStepDown) {
            return true;
        } else {
            return false;
        }
    }

    std::string KillFileWatcherAgent::contentsOfKillFile() const {
        rwlock lk(_lock, false);
        return _killFileContents;
    }

    void KillFileWatcherAgent::appendHealthStatus( BSONObjBuilder& result ) {
        BSONObjBuilder health;

        std::string msg = "healthy";
        bool healthy = true;

        log() << "appendHealthStatus called "
                    << _isKilled << " " << _killFileContents
                    << endl;

        bool killFileExists = isKilled();
        if (killFileExists) {
            healthy = false;
            std::string killFileContents = contentsOfKillFile();
            if (killFileContents.size() == 0) {
                msg = "kill file is present";
            } else {
                msg = "kill file is present: " + killFileContents;
            }
        }

        health.append("ok", healthy);
        health.append("msg", msg);

        result.append("healthStatus", health.obj());
    }

    void KillFileWatcherAgent::tryStepDownIfApplicable_inWriteLock() {
        if (_isKilled && replSet && theReplSet && theReplSet->isPrimary() &&
                killfileParams.triggerStepDown && !_hasSteppedDown) {
            // step down
            std::string errmsg;
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

    void KillFileWatcherAgent::handleChange_inWriteLock(bool oldValue, bool newValue) {
        _hasSteppedDown = false;
        _numChecksSinceChange = 0;
        _timeOfLastChange = Listener::getElapsedTimeMillis();
        _isKilled = newValue;

        std::string oldContents = _killFileContents;
        _killFileContents = "";

        if (newValue) {
            _killFileContents = readInKillFileContents(killfileParams.filePath);
        }

        log() << "kill file status changed! "
            << "before: " << statusString(oldValue, oldContents) << ". "
            << "now: " << statusString(newValue, _killFileContents)
            << endl;

        if (newValue) {
            tryStepDownIfApplicable_inWriteLock();
        }
    }

    void KillFileWatcherAgent::handleKilled_inWriteLock() {
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
        _killFileContents = readInKillFileContents(killfileParams.filePath);
    }

    void KillFileWatcherAgent::run() {

        if (!killfileParams.enabled) {
            LOG(1) << "KillFileWatcherAgent not enabled";
            return;
        }

        log() << "KillFileWatcherAgent started" << endl;
        _init();

        while(killfileParams.enabled && !inShutdown()) {
            try {
                bool previousValue = isKilled();
                bool newValue = boost::filesystem::exists(killfileParams.filePath);
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
                LOG(1) << "KillFileWatcher exception: " << e.what() << endl;
            }
            sleepsecs(1);
        }

        log() << "KillFileWatcherAgent shutting down" << endl;
    }

    MONGO_INITIALIZER(InitializeKillFileWatcher)(InitializerContext* context) {
        // HORRIBLE HACK to start thread later
        // Threads cannot be started in initializers
        snmpInit = &KillFileWatcherAgent::init;
        return Status::OK();
    }
}
