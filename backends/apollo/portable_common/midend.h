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

#ifndef BACKENDS_APOLLO_PORTABLE_COMMON_MIDEND_H_
#define BACKENDS_APOLLO_PORTABLE_COMMON_MIDEND_H_

#include "backends/apollo/common/midend.h"
#include "backends/apollo/common/options.h"
#include "frontends/common/options.h"
#include "ir/ir.h"
#include "midend/convertEnums.h"

namespace P4::Apollo {

class PortableMidEnd : public MidEnd {
 public:
    explicit PortableMidEnd(CompilerOptions &options) : MidEnd(options) {}
};

}  // namespace P4::Apollo

#endif /* BACKENDS_APOLLO_PORTABLE_COMMON_MIDEND_H_ */
