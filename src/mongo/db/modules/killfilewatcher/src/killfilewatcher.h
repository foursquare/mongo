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

#pragma once

#include <string>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/util/background.h"
#include "mongo/util/concurrency/rwlock.h"

namespace mongo {

    class KillFileWatcherAgent : public BackgroundJob {
    public:

        KillFileWatcherAgent();

        ~KillFileWatcherAgent();

        virtual std::string name() const;

        static void init();

        void shutdown();

        void run();

        bool isKilled() const;
        bool isForcedToNotBePrimary() const;
        std::string contentsOfKillFile() const;
        void appendHealthStatus(BSONObjBuilder& result);

    private:
        void _init();

        // Called on any transition.
        void handleChange_inWriteLock(bool oldValue, bool newValue);
        // Called every time we check and the kill file exists, even when it
        // continues to exist.
        void handleKilled_inWriteLock();

        void tryStepDownIfApplicable_inWriteLock();

        // These fields represent the current state of the watcher.
        // Whether the kill file currently exists.
        bool _isKilled;
        // If we're killed, the contents of the kill file, so we can add it to
        // logs and such.
        string _killFileContents;
        // How many times we've checked and the kill file has still existed (so
        // we can log reminders, and try to take action periodically if our
        // first attempt failed).
        int _numChecksSinceChange;
        // When the last transition occurred.
        long long _timeOfLastChange;
        // If this node was a master and the kill file is present and the
        // command-line param told us too, we may have stepped down. Indicates
        // whether we successfully stepped down so, if we didn't, we can try
        // again.
        bool _hasSteppedDown;

        // Read-write lock to control access to member variables. The only
        // exceptions are reading _path and _killFileShouldTriggerStepDown
        // since those are initialized once and never mutated after.
        RWLock _lock;

    };
    extern KillFileWatcherAgent killfileAgent;
}
