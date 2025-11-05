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

#ifndef BACKENDS_APOLLO_TUNA_NIC_TUNA_H_
#define BACKENDS_APOLLO_TUNA_NIC_TUNA_H_

#include "backends/apollo/common/backend.h"
#include "backends/apollo/common/expression.h"
#include "ir/ir.h"
#include "lib/json.h"

namespace P4::Apollo {

class TunaDeparserConverterImpl;
class TunaDeparserConverterWrapper : public Inspector {
    ConversionContext *ctxt;
    cstring name;
    P4::P4CoreLibrary &corelib;

 public:
    std::unique_ptr<TunaDeparserConverterImpl> pImpl_;

 public:
    bool preorder(const IR::P4Control *ctrl) override;
    explicit TunaDeparserConverterWrapper(ConversionContext *ctxt, cstring name);
    ~TunaDeparserConverterWrapper() override;
};

class TunaParserConverterImpl;
class TunaParserConverterWrapper : public Inspector {
    ConversionContext *ctxt;
    cstring name;
    P4::P4CoreLibrary &corelib;

 public:
    std::unique_ptr<TunaParserConverterImpl> pImpl_;

 public:
    bool preorder(const IR::P4Parser *parser) override;
    explicit TunaParserConverterWrapper(ConversionContext *ctxt, cstring name);
    ~TunaParserConverterWrapper() override;
};

template <Standard::Arch arch>
class TunaControlConverterImpl;
class TunaControlConverterWrapper : public Inspector {
    ConversionContext *ctxt;
    cstring name;
    P4::P4CoreLibrary &corelib;
    const bool emitExterns;

 public:
    std::unique_ptr<TunaControlConverterImpl<Standard::Arch::TUNA>> pImpl_;

 public:
    bool preorder(const IR::P4Control *cont) override;
    explicit TunaControlConverterWrapper(ConversionContext *ctxt, cstring name,
                                         const bool &emitExterns_);
    ~TunaControlConverterWrapper() override;
};

class TunaActionConverter : public Inspector {
    ConversionContext *ctxt;
    void postorder(const IR::P4Action *action) override;

 public:
    const bool emitExterns;
    explicit TunaActionConverter(ConversionContext *ctxt, const bool &emitExterns_)
        : ctxt(ctxt), emitExterns(emitExterns_) {
        setName("ConvertActions");
    }
};

struct FirmwareWriterGen {
    TunaParserConverterWrapper *ingressParserConverter;
    TunaParserConverterWrapper *egressParserConverter;
    TunaControlConverterWrapper *ingressControlConverter;
    TunaControlConverterWrapper *egressControlConverter;
    TunaDeparserConverterWrapper *ingressDeparserConverter;
    TunaDeparserConverterWrapper *egressDeparserConverter;
};

bool generateFirmwareAll(FirmwareWriterGen &gen, std::filesystem::path &outputFile);

}  // namespace P4::Apollo

#endif