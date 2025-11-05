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

#include "tunaNic.h"

#include "frontends/common/model.h"
#include "frontends/p4/cloner.h"

namespace P4::Apollo {

using namespace P4::literals;
// called by backends/apollo/tuna_nic/tunaNic.h:ConvertTunaToJson
//    --> codeGenerator->create(ctxt, structure);
void TunaCodeGenerator::create(ConversionContext *ctxt, P4::PortableProgramStructure *structure) {
    createTypes(ctxt, structure);
    createHeaders(ctxt, structure);
    createScalars(ctxt, structure);
    createExterns();
    createChecksumControls(ctxt, structure);
    createParsers(ctxt, structure);
    createActions(ctxt, structure);
    createControls(ctxt, structure);
    createDeparsers(ctxt, structure);
    createGlobals();
}

void TunaCodeGenerator::createActions(ConversionContext *ctxt,
                                      P4::PortableProgramStructure *structure) {
    auto cvt = new TunaActionConverter(ctxt, true);
    for (auto it : structure->actions) {
        auto action = it.first;
        action->apply(*cvt);
    }
}

void TunaCodeGenerator::createParsers(ConversionContext *ctxt,
                                      P4::PortableProgramStructure *structure) {
    {
        ingressParserConverter = new TunaParserConverterWrapper(ctxt, "ingress"_cs);
        auto ingress = structure->parsers.at("ingress"_cs);
        ingress->apply(*ingressParserConverter);
    }
    {
        egressParserConverter = new TunaParserConverterWrapper(ctxt, "egress"_cs);
        auto egress = structure->parsers.at("egress"_cs);
        egress->apply(*egressParserConverter);
    }
}

void TunaCodeGenerator::createControls(ConversionContext *ctxt,
                                       P4::PortableProgramStructure *structure) {
    ingressControlConverter = new Apollo::TunaControlConverterWrapper(ctxt, "ingress"_cs, true);
    auto ingress = structure->pipelines.at("ingress"_cs);
    ingress->apply(*ingressControlConverter);  // visitor

    egressControlConverter = new Apollo::TunaControlConverterWrapper(ctxt, "egress"_cs, true);
    auto egress = structure->pipelines.at("egress"_cs);
    egress->apply(*egressControlConverter);  // visitor
}

void TunaCodeGenerator::createDeparsers(ConversionContext *ctxt,
                                        P4::PortableProgramStructure *structure) {
    {
        ingressDeparserConverter = new TunaDeparserConverterWrapper(ctxt, "ingress"_cs);
        auto ingress = structure->deparsers.at("ingress"_cs);
        ingress->apply(*ingressDeparserConverter);
    }
    {
        egressDeparserConverter = new TunaDeparserConverterWrapper(ctxt, "egress"_cs);
        auto egress = structure->deparsers.at("egress"_cs);
        egress->apply(*egressDeparserConverter);
    }
}

cstring TunaCodeGenerator::createCalculation(ConversionContext *ctxt, cstring algo,
                                             const IR::Expression *fields,
                                             Util::JsonArray *calculations, bool withPayload,
                                             const IR::Node *sourcePositionNode) {
    cstring calcName = ctxt->refMap->newName("calc_");
    auto calc = new Util::JsonObject();
    calc->emplace("name", calcName);
    calc->emplace("id", nextId("calculations"_cs));
    if (sourcePositionNode != nullptr)
        calc->emplace_non_null("source_info"_cs, sourcePositionNode->sourceInfoJsonObj());
    calc->emplace("algo", algo);

    // Convert fields expression to JSON
    // Handle both StructExpression and ListExpression
    Util::IJson *jright = nullptr;
    if (auto structExpr = fields->to<IR::StructExpression>()) {
        // For StructExpression, convert each component
        auto array = new Util::JsonArray();
        for (auto namedExpr : structExpr->components) {
            auto jfield = ctxt->conv->convert(namedExpr->expression, false, true, false);
            array->append(jfield);
        }
        jright = array;
    } else if (fields->is<IR::ListExpression>()) {
        // For ListExpression, convert directly
        auto listExpr = fields->to<IR::ListExpression>();
        auto array = new Util::JsonArray();
        for (auto component : listExpr->components) {
            auto jfield = ctxt->conv->convert(component, false, true, false);
            array->append(jfield);
        }
        jright = array;
    } else {
        // Single field or other expression
        jright = ctxt->conv->convertWithConstantWidths(fields);
    }

    if (withPayload) {
        auto array = jright->to<Util::JsonArray>();
        BUG_CHECK(array, "expected a JSON array");
        auto payload = new Util::JsonObject();
        payload->emplace("type", "payload");
        payload->emplace("value", (Util::IJson *)nullptr);
        array->append(payload);
    }
    calc->emplace("input"_cs, jright);
    calculations->append(calc);
    return calcName;
}

void TunaCodeGenerator::convertChecksum(ConversionContext *ctxt, const IR::BlockStatement *block,
                                        Util::JsonArray *checksums, Util::JsonArray *calculations,
                                        bool verify, cstring gress) {
    if (::P4::errorCount() > 0) return;

    for (auto stat : block->components) {
        if (auto blk = stat->to<IR::BlockStatement>()) {
            // Recursively process nested blocks
            convertChecksum(ctxt, blk, checksums, calculations, verify, gress);
            continue;
        } else if (auto mc = stat->to<IR::MethodCallStatement>()) {
            auto mi = P4::MethodInstance::resolve(mc, ctxt->refMap, ctxt->typeMap);
            if (auto em = mi->to<P4::ExternFunction>()) {
                cstring functionName = em->method->name.name;

                // Check if this is a verify_checksum or update_checksum call
                bool isVerify = (functionName == "verify_checksum" ||
                                 functionName == "verify_checksum_with_payload");
                bool isUpdate = (functionName == "update_checksum" ||
                                 functionName == "update_checksum_with_payload");

                if ((verify && isVerify) || (!verify && isUpdate)) {
                    bool usePayload = functionName.endsWith("_with_payload");

                    if (mi->expr->arguments->size() != 4) {
                        ::P4::error(ErrorType::ERR_MODEL, "%1%: Expected 4 arguments", mc);
                        return;
                    }

                    auto cksum = new Util::JsonObject();

                    // Extract algorithm (4th argument)
                    auto ei = P4::EnumInstance::resolve(mi->expr->arguments->at(3)->expression,
                                                        ctxt->typeMap);
                    cstring algo = convertHashAlgorithm(ei->name);

                    // Extract field list (2nd argument)
                    auto calcExpr = mi->expr->arguments->at(1)->expression;

                    // Create calculation
                    cstring calcName =
                        createCalculation(ctxt, algo, calcExpr, calculations, usePayload, mc);

                    // Build checksum JSON object with gress prefix for unique name
                    cstring namePrefix = gress + "_cksum_";
                    cksum->emplace("name", ctxt->refMap->newName(namePrefix));
                    cksum->emplace("id", nextId("checksums"_cs));
                    cksum->emplace_non_null("source_info"_cs, stat->sourceInfoJsonObj());

                    // Extract target field (3rd argument)
                    auto jleft = ctxt->conv->convert(mi->expr->arguments->at(2)->expression);
                    cksum->emplace("target", jleft->to<Util::JsonObject>()->get("value"));

                    cksum->emplace("type", "generic");
                    cksum->emplace("calculation", calcName);

                    // Extract condition (1st argument)
                    auto ifcond =
                        ctxt->conv->convert(mi->expr->arguments->at(0)->expression, true, false);
                    cksum->emplace("if_cond"_cs, ifcond);

                    // Add with_payload flag
                    if (usePayload) {
                        cksum->emplace("with_payload", true);
                    }

                    checksums->append(cksum);

                    LOG3((verify ? "verify_checksum" : "update_checksum")
                         << " in " << gress << ": " << calcName);
                    continue;
                }
            }
        }

        // If we get here, it's an unsupported statement
        if (!stat->is<IR::EmptyStatement>()) {
            ::P4::error(ErrorType::ERR_UNSUPPORTED,
                        "%1%: Only calls to verify_checksum, update_checksum, "
                        "verify_checksum_with_payload, or update_checksum_with_payload allowed",
                        stat);
        }
    }
}

void TunaCodeGenerator::createChecksumControls(ConversionContext *ctxt,
                                               P4::PortableProgramStructure *structure) {
    // Use static_cast since we know this is a TunaProgramStructure
    auto tunaStructure = static_cast<P4::TunaProgramStructure *>(structure);

    // Process ingress verify checksum - should be converted to parser machine code
    if (tunaStructure->verify_checksums.count("ingress"_cs)) {
        auto verifyControl = tunaStructure->verify_checksums.at("ingress"_cs);
        LOG1("Processing ingress verify_checksum control");
        convertChecksum(ctxt, verifyControl->body, ctxt->json->verify_checksums,
                        ctxt->json->calculations, true, "ingress"_cs);
    }

    // Process ingress compute checksum - should be converted to deparser machine code
    if (tunaStructure->compute_checksums.count("ingress"_cs)) {
        auto computeControl = tunaStructure->compute_checksums.at("ingress"_cs);
        LOG1("Processing ingress compute_checksum control");
        convertChecksum(ctxt, computeControl->body, ctxt->json->compute_checksums,
                        ctxt->json->calculations, false, "ingress"_cs);
    }

    // Process egress verify checksum - should be converted to parser machine code
    if (tunaStructure->verify_checksums.count("egress"_cs)) {
        auto verifyControl = tunaStructure->verify_checksums.at("egress"_cs);
        LOG1("Processing egress verify_checksum control");
        convertChecksum(ctxt, verifyControl->body, ctxt->json->verify_checksums,
                        ctxt->json->calculations, true, "egress"_cs);
    }

    // Process egress compute checksum - should be converted to deparser machine code
    if (tunaStructure->compute_checksums.count("egress"_cs)) {
        auto computeControl = tunaStructure->compute_checksums.at("egress"_cs);
        LOG1("Processing egress compute_checksum control");
        convertChecksum(ctxt, computeControl->body, ctxt->json->compute_checksums,
                        ctxt->json->calculations, false, "egress"_cs);
    }

    LOG1("Checksum controls processing completed:");
    LOG1("  verify_checksums count: " << ctxt->json->verify_checksums->size());
    LOG1("  compute_checksums count: " << ctxt->json->compute_checksums->size());
}

cstring TunaCodeGenerator::convertHashAlgorithm(cstring algo) {
    if (algo == "crc32") return "crc32"_cs;                    // Standard CRC32 (0x04c11db7)
    if (algo == "crc32_1edc6f41") return "crc32_1edc6f41"_cs;  // Tuna custom CRC32 (0x1edc6f41)
    if (algo == "xor4") return "xor4"_cs;
    if (algo == "xor8") return "xor8"_cs;
    if (algo == "xor16") return "xor16"_cs;
    if (algo == "xor32") return "xor32"_cs;
    if (algo == "toeplitz") return "toeplitz"_cs;
    if (algo == "csum") return "csum"_cs;
    if (algo == "crc16") return "crc16"_cs;

    ::P4::error(ErrorType::ERR_UNSUPPORTED, "Unsupported hash algorithm %1% for Tuna", algo);
    return nullptr;
}

void TunaNicBackend::checkUnsupportedFeatures(const IR::ToplevelBlock *tlb) {
    // check if DirectCounter is used
    for (auto decl : tlb->getProgram()->objects) {
        if (auto inst = decl->to<IR::Declaration_Instance>()) {
            if (inst->type->is<IR::Type_Specialized>()) {
                auto spec = inst->type->to<IR::Type_Specialized>();
                if (spec->baseType->toString() == "DirectCounter") {
                    ::P4::error(ErrorType::ERR_UNSUPPORTED,
                                "%1%: DirectCounter is not supported in Tuna architecture. "
                                "Use Counter with explicit indexing instead.",
                                inst);
                }
            } else if (auto name = inst->type->to<IR::Type_Name>()) {
                if (name->path->name == "DirectCounter") {
                    ::P4::error(ErrorType::ERR_UNSUPPORTED,
                                "%1%: DirectCounter is not supported in Tuna architecture. "
                                "Use Counter with explicit indexing instead.",
                                inst);
                }
            }
        }
    }
}

void TunaNicBackend::convert(const IR::ToplevelBlock *tlb) {
    CHECK_NULL(tlb);
    // check unsupported features
    checkUnsupportedFeatures(tlb);

    P4::TunaProgramStructure structure(refMap, typeMap);
    auto parseTunaArch = new P4::ParseTunaArchitecture(&structure);
    auto main = tlb->getMain();
    if (!main) return;

    if (main->type->name != "TunaNic")
        ::P4::warning(ErrorType::WARN_INVALID,
                      "%1%: the main package should be called TunaNic"
                      "; are you using the wrong architecture?",
                      main->type->name);

    main->apply(*parseTunaArch);

    auto evaluator = new P4::EvaluatorPass(refMap, typeMap);
    auto program = tlb->getProgram();
    PassManager simplify = {
        /* TODO */
        // new RenameUserMetadata(refMap, userMetaType, userMetaName),
        new P4::ClearTypeMap(typeMap),
        new P4::SynthesizeActions(refMap, typeMap,
                                  new SkipControls(&structure.non_pipeline_controls)),
        new P4::MoveActionsToTables(refMap, typeMap),
        new P4::SimplifyControlFlow(typeMap, true),
        new LowerExpressions(typeMap),
        new PassRepeated({
            new P4::ConstantFolding(typeMap),
            new P4::StrengthReduction(typeMap),
        }),
        new P4::TypeChecking(refMap, typeMap),
        new P4::RemoveComplexExpressions(typeMap,
                                         new ProcessControls(&structure.pipeline_controls)),
        new P4::SimplifyControlFlow(typeMap, true),
        new P4::RemoveAllUnusedDeclarations(P4::RemoveUnusedPolicy()),
        // Converts the DAG into a TREE (at least for expressions)
        // This is important later for conversion to JSON.
        new P4::CloneExpressions(),
        new P4::ClearTypeMap(typeMap),
        evaluator,
        [this, evaluator, structure]() { toplevel = evaluator->getToplevelBlock(); },
    };
    auto hook = options.getDebugHook();
    simplify.addDebugHook(hook);
    program->apply(simplify);

    // map IR node to compile-time allocated resource blocks.
    toplevel->apply(*new P4::BuildResourceMap(&structure.resourceMap));

    main = toplevel->getMain();
    if (!main) return;  // no main
    main->apply(*parseTunaArch);
    if (::P4::errorCount() > 0) return;
    program = toplevel->getProgram();

    auto convertToJson =
        new ConvertTunaToJson(refMap, typeMap, toplevel, json, &structure, options.generateBins);

    PassManager toJson = {new P4::DiscoverStructure(&structure),
                          new P4::InspectTunaProgram(refMap, typeMap, &structure), convertToJson};
    for (const auto &pEnum : *enumMap) {
        auto name = pEnum.first->getName();
        for (const auto &pEntry : *pEnum.second) {
            json->add_enum(name, pEntry.first, pEntry.second);
        }
    }

    program->apply(toJson);
    json->add_program_info(cstring(options.file));
    json->add_meta_info();

    if (!options.outputFile.empty()) {
        if (convertToJson && convertToJson->codeGenerator && options.generateBins) {
            LOG1("Generating firmware files...");
            if (!generateFirmware(convertToJson->codeGenerator)) {
                ::P4::error(ErrorType::ERR_IO, "Failed to generate firmware");
                return;
            }
        }
    }
}

ExternConverter_Hash ExternConverter_Hash::singleton;
ExternConverter_InternetChecksum ExternConverter_InternetChecksum::singleton;
ExternConverter_Register ExternConverter_Register::singleton;

Util::IJson *ExternConverter_Hash::convertExternObject(UNUSED ConversionContext *ctxt,
                                                       const P4::ExternMethod *em,
                                                       const IR::MethodCallExpression *mc,
                                                       UNUSED const IR::StatOrDecl *s,
                                                       UNUSED const bool &emitExterns) {
    Util::JsonObject *primitive = nullptr;
    if (mc->arguments->size() == 2)
        primitive = mkPrimitive("_" + em->originalExternType->name + "_" + em->method->name);
    else if (mc->arguments->size() == 4)
        primitive = mkPrimitive("_" + em->originalExternType->name + "_" + "get_hash_mod");
    else {
        modelError("Expected 1 or 3 arguments for %1%", mc);
        return nullptr;
    }
    auto parameters = mkParameters(primitive);
    primitive->emplace_non_null("source_info"_cs, s->sourceInfoJsonObj());
    auto hash = new Util::JsonObject();
    hash->emplace("type"_cs, "extern");
    hash->emplace("value"_cs, em->object->controlPlaneName());
    parameters->append(hash);
    if (mc->arguments->size() == 2) {  // get_hash
        auto dst = ctxt->conv->convertLeftValue(mc->arguments->at(0)->expression);
        auto fieldList = new Util::JsonObject();
        fieldList->emplace("type"_cs, "field_list");
        auto fieldsJson = ctxt->conv->convert(mc->arguments->at(1)->expression, true, false);
        fieldList->emplace("value"_cs, fieldsJson);
        parameters->append(dst);
        parameters->append(fieldList);
    } else {  // get_hash with base and mod
        auto dst = ctxt->conv->convertLeftValue(mc->arguments->at(0)->expression);
        auto base = ctxt->conv->convert(mc->arguments->at(1)->expression);
        auto fieldList = new Util::JsonObject();
        fieldList->emplace("type"_cs, "field_list");
        auto fieldsJson = ctxt->conv->convert(mc->arguments->at(2)->expression, true, false);
        fieldList->emplace("value"_cs, fieldsJson);
        auto max = ctxt->conv->convert(mc->arguments->at(3)->expression);
        parameters->append(dst);
        parameters->append(base);
        parameters->append(fieldList);
        parameters->append(max);
    }

    return primitive;
}

Util::IJson *ExternConverter_InternetChecksum::convertExternObject(
    UNUSED ConversionContext *ctxt, UNUSED const P4::ExternMethod *em,
    UNUSED const IR::MethodCallExpression *mc, UNUSED const IR::StatOrDecl *s,
    UNUSED const bool &emitExterns) {
    Util::JsonObject *primitive = nullptr;
    if (em->method->name == "add" || em->method->name == "subtract" ||
        em->method->name == "get_state" || em->method->name == "set_state") {
        if (mc->arguments->size() != 1) {
            modelError("Expected 1 argument for %1%", mc);
            return nullptr;
        } else
            primitive = mkPrimitive("_" + em->originalExternType->name + "_" + em->method->name);
    } else if (em->method->name == "get") {
        if (mc->arguments->size() == 1)
            primitive = mkPrimitive("_" + em->originalExternType->name + "_" + em->method->name);
        else if (mc->arguments->size() == 2)
            primitive = mkPrimitive("_" + em->originalExternType->name + "_" + "get_verify");
        else {
            modelError("Unexpected number of arguments for %1%", mc);
            return nullptr;
        }
    } else if (em->method->name == "clear") {
        if (mc->arguments->size() != 0) {
            modelError("Expected 0 argument for %1%", mc);
            return nullptr;
        } else
            primitive = mkPrimitive("_" + em->originalExternType->name + "_" + em->method->name);
    }
    auto parameters = mkParameters(primitive);
    primitive->emplace_non_null("source_info"_cs, s->sourceInfoJsonObj());
    auto cksum = new Util::JsonObject();
    cksum->emplace("type", "extern");
    cksum->emplace("value", em->object->controlPlaneName());
    parameters->append(cksum);
    if (em->method->name == "add" || em->method->name == "subtract") {
        auto fieldList = new Util::JsonObject();
        fieldList->emplace("type", "field_list");
        auto fieldsJson = ctxt->conv->convert(mc->arguments->at(0)->expression, true, false);
        fieldList->emplace("value", fieldsJson);
        parameters->append(fieldList);
    } else if (em->method->name != "clear") {
        if (mc->arguments->size() == 2) {  // get_verify
            auto dst = ctxt->conv->convertLeftValue(mc->arguments->at(0)->expression);
            auto equOp = ctxt->conv->convert(mc->arguments->at(1)->expression);
            parameters->append(dst);
            parameters->append(equOp);
        } else if (mc->arguments->size() == 1) {  // get or get_state or set_state
            auto dst = ctxt->conv->convert(mc->arguments->at(0)->expression);
            parameters->append(dst);
        }
    }
    return primitive;
}

Util::IJson *ExternConverter_Register::convertExternObject(
    UNUSED ConversionContext *ctxt, UNUSED const P4::ExternMethod *em,
    UNUSED const IR::MethodCallExpression *mc, UNUSED const IR::StatOrDecl *s,
    UNUSED const bool &emitExterns) {
    if (em->method->name != "write" && em->method->name != "read") {
        modelError("Unsupported register method %1%", mc);
        return nullptr;
    } else if (em->method->name == "write" && mc->arguments->size() != 2) {
        modelError("Expected 2 arguments for %1%", mc);
        return nullptr;
    } else if (em->method->name == "read" && mc->arguments->size() != 2) {
        modelError("p4c-tuna internally requires 2 arguments for %1%", mc);
        return nullptr;
    }
    auto reg = new Util::JsonObject();
    reg->emplace("type", "register_array");
    cstring name = em->object->controlPlaneName();
    reg->emplace("value", name);
    if (em->method->name == "read") {
        auto primitive = mkPrimitive("register_read"_cs);
        auto parameters = mkParameters(primitive);
        primitive->emplace_non_null("source_info"_cs, s->sourceInfoJsonObj());
        auto dest = ctxt->conv->convert(mc->arguments->at(0)->expression);
        parameters->append(dest);
        parameters->append(reg);
        auto index = ctxt->conv->convert(mc->arguments->at(1)->expression);
        parameters->append(index);
        return primitive;
    } else if (em->method->name == "write") {
        auto primitive = mkPrimitive("register_write"_cs);
        auto parameters = mkParameters(primitive);
        primitive->emplace_non_null("source_info"_cs, s->sourceInfoJsonObj());
        parameters->append(reg);
        auto index = ctxt->conv->convert(mc->arguments->at(0)->expression);
        parameters->append(index);
        auto value = ctxt->conv->convert(mc->arguments->at(1)->expression);
        parameters->append(value);
        return primitive;
    }
    return nullptr;
}

void ExternConverter_Hash::convertExternInstance(ConversionContext *ctxt, const IR::Declaration *c,
                                                 const IR::ExternBlock *eb,
                                                 UNUSED const bool &emitExterns) {
    auto inst = c->to<IR::Declaration_Instance>();
    cstring name = inst->controlPlaneName();

    // Get constructor parameter (algorithm type)
    auto algoParam = eb->findParameterValue("algo"_cs);
    if (!algoParam || !algoParam->is<IR::Declaration_ID>()) {
        ::P4::error(ErrorType::ERR_EXPECTED, "%1%: Hash expects an algorithm parameter", eb);
        return;
    }

    // Parse algorithm enum value
    auto algoDecl = algoParam->to<IR::Declaration_ID>();
    cstring algoName = algoDecl->name;

    // Create TunaCodeGenerator instance to use convertHashAlgorithm
    auto tunaStructure = new TunaCodeGenerator();
    cstring tunaAlgo = tunaStructure->convertHashAlgorithm(algoName);
    if (tunaAlgo == nullptr) {
        return;  // Error already reported in convertHashAlgorithm
    }

    // Generate JSON configuration
    auto hashExtern = new Util::JsonObject();
    hashExtern->emplace("name", name);
    hashExtern->emplace("id", nextId("extern_instances"_cs));
    hashExtern->emplace("type", eb->getName());
    hashExtern->emplace_non_null("source_info"_cs, inst->sourceInfoJsonObj());

    // Add attributes
    auto attrs = ctxt->json->insert_array_field(hashExtern, "attribute_values"_cs);

    auto algoAttr = new Util::JsonObject();
    algoAttr->emplace("name", "algorithm");
    algoAttr->emplace("type", "string");
    algoAttr->emplace("value", tunaAlgo);
    attrs->append(algoAttr);

    // Add polynomial for CRC algorithms
    if (algoName == "crc32_1edc6f41") {
        auto polyAttr = new Util::JsonObject();
        polyAttr->emplace("name", "polynomial");
        polyAttr->emplace("type", "hexstr");
        polyAttr->emplace("value", "0x1edc6f41");
        attrs->append(polyAttr);
    } else if (algoName == "crc32") {
        auto polyAttr = new Util::JsonObject();
        polyAttr->emplace("name", "polynomial");
        polyAttr->emplace("type", "hexstr");
        polyAttr->emplace("value", "0x04c11db7");
        attrs->append(polyAttr);
    }

    // Add to JSON output
    ctxt->json->externs->append(hashExtern);
    LOG3("Created Hash extern instance: " << name << " with algorithm: " << tunaAlgo);
}

void ExternConverter_InternetChecksum::convertExternInstance(UNUSED ConversionContext *ctxt,
                                                             UNUSED const IR::Declaration *c,
                                                             UNUSED const IR::ExternBlock *eb,
                                                             UNUSED const bool &emitExterns) {
    auto inst = c->to<IR::Declaration_Instance>();
    cstring name = inst->controlPlaneName();
    auto trim = inst->controlPlaneName().find(".");
    auto block = inst->controlPlaneName().before(trim);
    auto tunaStructure = static_cast<P4::TunaProgramStructure *>(ctxt->structure);
    auto ingressParser = tunaStructure->parsers.at("ingress"_cs)->controlPlaneName();
    auto ingressDeparser = tunaStructure->deparsers.at("ingress"_cs)->controlPlaneName();
    auto egressParser = tunaStructure->parsers.at("egress"_cs)->controlPlaneName();
    auto egressDeparser = tunaStructure->deparsers.at("egress"_cs)->controlPlaneName();
    if (block != ingressParser && block != ingressDeparser && block != egressParser &&
        block != egressDeparser) {
        ::P4::error(ErrorType::ERR_UNSUPPORTED, "%1%: not supported in pipeline on this target",
                    eb);
    }
    // add checksum instance
    auto jcksum = new Util::JsonObject();
    jcksum->emplace("name", name);
    jcksum->emplace("id", nextId("extern_instances"_cs));
    jcksum->emplace("type", eb->getName());
    jcksum->emplace_non_null("source_info"_cs, inst->sourceInfoJsonObj());
    ctxt->json->externs->append(jcksum);
}

void ExternConverter_Register::convertExternInstance(UNUSED ConversionContext *ctxt,
                                                     UNUSED const IR::Declaration *c,
                                                     UNUSED const IR::ExternBlock *eb,
                                                     UNUSED const bool &emitExterns) {
    size_t paramSize = eb->getConstructorParameters()->size();
    if (paramSize == 2) {
        modelError("%1%: Expecting 1 parameter. Initial value not supported", eb->constructor);
    }
    auto inst = c->to<IR::Declaration_Instance>();
    cstring name = inst->controlPlaneName();
    auto jreg = new Util::JsonObject();
    jreg->emplace("name", name);
    jreg->emplace("id", nextId("register_arrays"_cs));
    jreg->emplace_non_null("source_info"_cs, eb->sourceInfoJsonObj());
    auto sz = eb->findParameterValue("size"_cs);
    CHECK_NULL(sz);
    if (!sz->is<IR::Constant>()) {
        modelError("%1%: expected a constant", sz->getNode());
        return;
    }
    if (sz->to<IR::Constant>()->value == 0)
        error(ErrorType::ERR_UNSUPPORTED, "%1%: direct registers are not supported", inst);
    jreg->emplace("size", sz->to<IR::Constant>()->value);
    if (!eb->instanceType->is<IR::Type_SpecializedCanonical>()) {
        modelError("%1%: Expected a generic specialized type", eb->instanceType);
        return;
    }
    auto st = eb->instanceType->to<IR::Type_SpecializedCanonical>();
    if (st->arguments->size() != 2) {
        modelError("%1%: expected 2 type argument", st);
        return;
    }
    auto regType = st->arguments->at(0);
    if (!regType->is<IR::Type_Bits>()) {
        ::P4::error(ErrorType::ERR_UNSUPPORTED,
                    "%1%: registers with types other than bit<> or int<> are not suppoted", eb);
        return;
    }
    unsigned width = regType->width_bits();
    if (width == 0) {
        ::P4::error(ErrorType::ERR_UNKNOWN, "%1%: unknown width", st->arguments->at(0));
        return;
    }
    jreg->emplace("bitwidth", width);
    ctxt->json->register_arrays->append(jreg);
}

bool TunaNicBackend::generateFirmware(TunaCodeGenerator *codeGen) const {
    FirmwareWriterGen gen;
    gen.ingressParserConverter = codeGen->ingressParserConverter;
    gen.egressParserConverter = codeGen->egressParserConverter;
    gen.ingressControlConverter = codeGen->ingressControlConverter;
    gen.egressControlConverter = codeGen->egressControlConverter;
    gen.ingressDeparserConverter = codeGen->ingressDeparserConverter;
    gen.egressDeparserConverter = codeGen->egressDeparserConverter;

    return generateFirmwareAll(gen, options.outputFile);
}

}  // namespace P4::Apollo