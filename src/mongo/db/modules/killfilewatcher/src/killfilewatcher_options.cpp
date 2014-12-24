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

#include "mongo/platform/basic.h"

#include "killfilewatcher_options.h"

#include <string>
#include <vector>

#include "mongo/base/status.h"
#include "mongo/db/server_options.h"
#include "mongo/util/options_parser/option_description.h"
#include "mongo/util/options_parser/option_section.h"
#include "mongo/util/options_parser/options_parser.h"
#include "mongo/util/options_parser/startup_option_init.h"
#include "mongo/util/options_parser/startup_options.h"

namespace mongo {

    KillFileWatcherParams killfileParams;

    Status addKillFileWatcherOptions(moe::OptionSection* options) {

        moe::OptionSection killfile_options("KillFileWatcher Module Options");

        killfile_options.addOptionChaining("killfile.stepdown", "kill-file-should-trigger-step-down", moe::Switch,
                                        "Step down master on presense of killfile")
                                        .setDefault(moe::Value(false));
        killfile_options.addOptionChaining("killfile.filepath", "kill-file-path", moe::String,
                                        "File path to kill file. Required to turn on this module");

        Status ret = options->addSection(killfile_options);
        if (!ret.isOK()) {
            return ret;
        }

        return Status::OK();
    }

    Status storeKillFileWatcherOptions(const moe::Environment& params,
                             const std::vector<std::string>& args) {

        if (params.count("killfile.filepath")) {
            killfileParams.enabled = true;
            killfileParams.filePath = params["killfile.filepath"].as<string>();
            killfileParams.triggerStepDown = params["killfile.stepdown"].as<bool>();
        }

        return Status::OK();
    }

    MONGO_MODULE_STARTUP_OPTIONS_REGISTER(KillFileWatcherOptions)(InitializerContext* context) {
        return addKillFileWatcherOptions(&moe::startupOptions);
    }

    MONGO_STARTUP_OPTIONS_STORE(KillFileWatcherOptions)(InitializerContext* context) {
        return storeKillFileWatcherOptions(moe::startupOptionsParsed, context->args());
    }

} // namespace mongo
