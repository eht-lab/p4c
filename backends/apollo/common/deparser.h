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

#ifndef BACKENDS_APOLLO_COMMON_DEPARSER_H_
#define BACKENDS_APOLLO_COMMON_DEPARSER_H_

#include "backend.h"
#include "expression.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeMap.h"
#include "ir/ir.h"
#include "lib/json.h"

namespace P4::Apollo {

class DeparserConverter : public Inspector {
    ConversionContext *ctxt;
    cstring name;
    P4::P4CoreLibrary &corelib;

 protected:
    Util::IJson *convertDeparser(const IR::P4Control *ctrl);
    void convertDeparserBody(const IR::Vector<IR::StatOrDecl> *body, Util::JsonArray *order,
                             Util::JsonArray *primitives);

 public:
    bool preorder(const IR::P4Control *ctrl) override;
    explicit DeparserConverter(ConversionContext *ctxt, cstring name = "deparser"_cs)
        : ctxt(ctxt), name(name), corelib(P4::P4CoreLibrary::instance()) {
        setName("DeparserConverter");
    }
};

}  // namespace P4::Apollo

#endif /* BACKENDS_APOLLO_COMMON_DEPARSER_H_ */
