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

#pragma once

#include "pch.h"
#include "../db.h"
#include "../instance.h"
#include "../module.h"
#include "../../util/background.h"
#include "../../util/concurrency/rwlock.h"
#include "../commands.h"

#include <ctime>

namespace mongo {

    /** Kill file watcher.
        If enabled, this checks for existence of a kill file.
    */
    class KillFileWatcher : public BackgroundJob , Module {
    public:

        KillFileWatcher();
        ~KillFileWatcher();

        bool isKilled() const;
        bool isForcedToNotBePrimary() const;
        string contentsOfKillFile() const;
        void appendHealthStatus( BSONObjBuilder& result);

        bool config( boost::program_options::variables_map& params );

        string name() const;

        void run();

        void init();
        void shutdown();

    private:
        // Called on any transition.
        void handleChange_inWriteLock(bool oldValue, bool newValue);
        // Called every time we check and the kill file exists, even when it
        // continues to exist.
        void handleKilled_inWriteLock();

        void tryStepDownIfApplicable_inWriteLock();

        // These two fields are command-line configured parameters.
        string _path;
        bool _killFileShouldTriggerStepDown;

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

    extern KillFileWatcher killFileWatcher;
}
