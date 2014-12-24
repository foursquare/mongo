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
#include <vector>

#include "mongo/base/status.h"

namespace mongo {

    namespace optionenvironment {
        class Environment;
        class OptionSection;
    } // namespace optionenvironment

    namespace moe = mongo::optionenvironment;

    struct KillFileWatcherParams {

        KillFileWatcherParams() : enabled(false) {}

        bool enabled;
        std::string filePath;
        bool triggerStepDown;
    };

    extern KillFileWatcherParams killfileParams;

    Status addKillFileWatcherOptions(moe::OptionSection* options);

    Status storeKillFileWatcherOptions(const moe::Environment& params,
                             const std::vector<std::string>& args);
}
