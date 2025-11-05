/*
Copyright (C) 2025-2026 EHTech(Beijing)Co.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef BACKENDS_APOLLO_TUNA_NIC_OPTIONS_H_
#define BACKENDS_APOLLO_TUNA_NIC_OPTIONS_H_

#include "backends/apollo/portable_common/options.h"
#include "backends/apollo/tuna_nic/midend.h"

namespace P4::Apollo {

class TunaNicOptions : public PortableOptions {
 public:
    TunaNicOptions() {
        registerOption(
            "--listMidendPasses", nullptr,
            [this](const char *) {
                listMidendPasses = true;
                loadIRFromJson = false;
                TunaNicMidEnd midEnd(*this, outStream);
                exit(0);
                return false;
            },
            "[TunaNic back-end] Lists exact name of all midend passes.\n");
    }

    std::vector<const char *> *process(int argc, char *const argv[]) override;
};

using TunaNicContext = P4CContextWithOptions<TunaNicOptions>;

}  // namespace P4::Apollo

#endif /* BACKENDS_APOLLO_TUNA_NIC_OPTIONS_H_ */
