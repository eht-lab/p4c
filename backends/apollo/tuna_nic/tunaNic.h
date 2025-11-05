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

#ifndef BACKENDS_APOLLO_TUNA_NIC_TUNANIC_H_
#define BACKENDS_APOLLO_TUNA_NIC_TUNANIC_H_

#include "backends/apollo/common/helpers.h"
#include "backends/apollo/portable_common/portable.h"
#include "backends/apollo/tuna_nic/tunaProgramStructure.h"
#include "tuna.h"

namespace P4::Apollo {

class TunaNicExpressionConverter : public ExpressionConverter {
 public:
    TunaNicExpressionConverter(P4::ReferenceMap *refMap, P4::TypeMap *typeMap,
                               P4::ProgramStructure *structure, cstring scalarsName)
        : Apollo::ExpressionConverter(refMap, typeMap, structure, scalarsName) {}

    void modelError(const char *format, const cstring field) {
        ::P4::error(ErrorType::ERR_MODEL,
                    (cstring(format) + "\nInvalid metadata parameter value for Tuna").c_str(),
                    field);
    }

    Util::IJson *convertParam(UNUSED const IR::Parameter *param, cstring fieldName) override {
        cstring ptName = param->type->toString();
        if (P4::TunaProgramStructure::isCounterMetadata(ptName)) {  // check if its counter metadata
            auto jsn = new Util::JsonObject();
            jsn->emplace("name", param->toString());
            jsn->emplace("type", "hexstr");
            // FIXME -- how big is a TUNA_CounterType_t?  Being an enum type, we can't
            // sensibly call param->width_bits() here.
            auto bitwidth = 0;

            // encode the counter type from enum -> int
            if (fieldName == "BYTES") {
                cstring repr = Apollo::stringRepr(0, ROUNDUP(bitwidth, 32));
                jsn->emplace("value", repr);
            } else if (fieldName == "PACKETS") {
                cstring repr = Apollo::stringRepr(1, ROUNDUP(bitwidth, 32));
                jsn->emplace("value", repr);
            } else if (fieldName == "PACKETS_AND_BYTES") {
                cstring repr = Apollo::stringRepr(2, ROUNDUP(bitwidth, 32));
                jsn->emplace("value", repr);
            } else {
                modelError("%1%: Expected a TUNA_CounterType_t", fieldName);
                return nullptr;
            }
            return jsn;
        } else if (P4::TunaProgramStructure::isStandardMetadata(
                       ptName)) {  // check if its tuna metadata
            auto jsn = new Util::JsonObject();

            // encode the metadata type and field in json
            jsn->emplace("type", "field");
            auto a = mkArrayField(jsn, "value"_cs);
            a->append(ptName.exceptLast(2));
            a->append(fieldName);
            return jsn;
        } else {
            // not a special type
            return nullptr;
        }
        return nullptr;
    }
};

class TunaCodeGenerator : public PortableCodeGenerator {
 public:
    TunaParserConverterWrapper *ingressParserConverter = nullptr;
    TunaParserConverterWrapper *egressParserConverter = nullptr;
    TunaDeparserConverterWrapper *ingressDeparserConverter = nullptr;
    TunaDeparserConverterWrapper *egressDeparserConverter = nullptr;
    TunaActionConverter *actionConverter = nullptr;
    TunaControlConverterWrapper *ingressControlConverter = nullptr;
    TunaControlConverterWrapper *egressControlConverter = nullptr;

    void create(ConversionContext *ctxt, P4::PortableProgramStructure *structure);
    void createParsers(ConversionContext *ctxt, P4::PortableProgramStructure *structure);
    void createControls(ConversionContext *ctxt, P4::PortableProgramStructure *structure);
    void createDeparsers(ConversionContext *ctxt, P4::PortableProgramStructure *structure);
    void createActions(ConversionContext *ctxt, P4::PortableProgramStructure *structure);
    void createChecksumControls(ConversionContext *ctxt, P4::PortableProgramStructure *structure);
    cstring convertHashAlgorithm(cstring algo);

    // Checksum support functions
    cstring createCalculation(ConversionContext *ctxt, cstring algo, const IR::Expression *fields,
                              Util::JsonArray *calculations, bool withPayload,
                              const IR::Node *sourcePositionNode);
    void convertChecksum(ConversionContext *ctxt, const IR::BlockStatement *block,
                         Util::JsonArray *checksums, Util::JsonArray *calculations, bool verify,
                         cstring gress);
};

class ConvertTunaToJson : public Inspector {
 public:
    P4::ReferenceMap *refMap;
    P4::TypeMap *typeMap;
    const IR::ToplevelBlock *toplevel;
    JsonObjects *json;
    P4::TunaProgramStructure *structure;
    TunaCodeGenerator *codeGenerator;
    bool generateBins;
    ConversionContext *context;

    ConvertTunaToJson(P4::ReferenceMap *refMap, P4::TypeMap *typeMap,
                      const IR::ToplevelBlock *toplevel, JsonObjects *json,
                      P4::TunaProgramStructure *structure, bool generateBins)
        : refMap(refMap),
          typeMap(typeMap),
          toplevel(toplevel),
          json(json),
          structure(structure),
          generateBins(generateBins),
          context(nullptr) {
        CHECK_NULL(refMap);
        CHECK_NULL(typeMap);
        CHECK_NULL(toplevel);
        CHECK_NULL(json);
        CHECK_NULL(structure);
        codeGenerator = new TunaCodeGenerator();
        CHECK_NULL(codeGenerator);
    }

    void postorder(UNUSED const IR::P4Program *program) override {
        cstring scalarsName = "scalars"_cs;
        // This visitor is used in multiple passes to convert expression to json
        auto conv = new TunaNicExpressionConverter(refMap, typeMap, structure, scalarsName);
        context = new ConversionContext(refMap, typeMap, toplevel, structure, conv, json);
        context->generateBins = generateBins;
        codeGenerator->create(context, structure);
    }
};

class TunaNicBackend : public Backend {
 protected:
    ApolloOptions &options;

 public:
    void convert(const IR::ToplevelBlock *tlb) override;
    void serialize(std::ostream &out) const { Backend::serialize(out); }
    TunaNicBackend(ApolloOptions &options, P4::ReferenceMap *refMap, P4::TypeMap *typeMap,
                   P4::ConvertEnums::EnumMapping *enumMap)
        : Backend(options, refMap, typeMap, enumMap), options(options) {}

    bool generateFirmware(TunaCodeGenerator *codeGen) const;
    void checkUnsupportedFeatures(const IR::ToplevelBlock *tlb);
};

EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE(Hash)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE(InternetChecksum)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE(Register)
// EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE(Counter)
// EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE(Checksum)

}  // namespace P4::Apollo

#endif /* BACKENDS_APOLLO_TUNA_NIC_TUNANIC_H_ */
