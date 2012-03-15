// @file healthbinary.cpp
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


#include "../db.h"
#include "../module.h"
#include "../../util/background.h"
#include "../../util/log.h"
#include "healthbinary.h"

namespace po = boost::program_options;

namespace mongo {

    HealthBinary::HealthBinary()
        : Module( "healthbinary" ) , _path( "" ), _ok( true ), _message( "" ), _lastRunMs(0), _lastRunTimestamp(0) {

            add_options()
                ( "health-binary" , po::value<string>() , "absolute path of health binary to run for health checking. if unset, no health binary is monitored" )
                ;
        }

    HealthBinary::~HealthBinary() {}

    bool HealthBinary::ok() const { return _ok; }
    string HealthBinary::message() const { return _message; }
    time_t HealthBinary::lastRunMs() const { return _lastRunMs; }
    time_t HealthBinary::lastRunTimestamp() const { return _lastRunTimestamp; }


    bool HealthBinary::config( program_options::variables_map& params ) {
        if (params.count("health-binary") > 0) {
            _path = params["health-binary"].as<string>();

            // in newer versions of boost, is_complete is renamed to is_absolute, but we don't have that yet.
            if (!boost::filesystem::path(_path).is_complete()) {
                log(LL_ERROR) << "health-binary must be absolute! bailing since we got " << _path << endl;
                return false;
            } else if ( !boost::filesystem::exists(_path) ) {
                log(LL_ERROR) << "health-binary must exist at " << _path << endl;
                return false;
            }
        }

        return true;
    }

    string HealthBinary::name() const { return "healthbinary"; }

    void HealthBinary::run() {
        if ( _path.size() == 0 ) {
            log() << "HealthBinary not configured" << endl;
            return;
        }

        log() << "health-binary monitor starting and monitoring path " << _path << endl;

        while ( ! inShutdown() ) {
            sleepsecs( 1 );

            Timer t;
            try {
                bool previousValue = _ok;

                FILE* fp = popen( (_path + " 2>&1").c_str() , "r");

                if (fp == NULL) {
                    log(LL_ERROR) << "failed to run binary " << _path << endl;
                }

                char buffer[128];
                string message = "";
                while( !feof(fp) ) {
                    if(fgets( buffer, sizeof(buffer) , fp) != NULL)
                        message += buffer;
                }

                _message = message;

                int status = pclose(fp);
                // TODO(jon) handle more exit conditions
                // see examples here: http://pubs.opengroup.org/onlinepubs/009604499/functions/wait.html
                if ( status != -1 && WIFEXITED(status) ) {
                    _ok = WEXITSTATUS(status) == 0;
                } else {
                    log(LL_ERROR) << "healthbinary did not exit correctly " << _path << endl;
                }


                if (_ok != previousValue) {
                    log() << "healthbinary status changed! "
                        << "before: " << (previousValue ? "OK. " : "UNHEALTHY. ")
                        << "now: " << ( _ok ? "OK. " : "UNHEALTHY. ")
                        << endl;
                }
            }
            catch ( std::exception& e ) {
                log(LL_ERROR) << "healthbinary exception: " << e.what() << endl;
            }
            _lastRunMs = t.millis();
            _lastRunTimestamp = time(0);
        }
    }

    void HealthBinary::init() {
        go();
    }

    void HealthBinary::shutdown() {
        // TODO
    }

    HealthBinary healthBinary;
}



