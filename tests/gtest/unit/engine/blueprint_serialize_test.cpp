/**
 * @file blueprint_serialize_test.cpp
 * @brief P0-2 共享资产契约：.dbp round-trip / 迁移 / 损坏 / 前向兼容测试。
 */

#include <gtest/gtest.h>

#include "engine/scripting/blueprint/blueprint_serialize.h"
#include "engine/scripting/blueprint/blueprint_compiler.h"

namespace {

using namespace dse::bp;

BlueprintAsset MakeSampleAsset() {
    BlueprintAsset a;
    a.name = "SampleBP";
    a.description = "round-trip 测试蓝图";  // 含非 ASCII，验证 UTF-8 保真

    BpVariable var;
    var.name = "speed";
    var.type = BpVarType::Float;
    var.default_float = 2.5f;
    var.is_exposed = true;
    a.variables.push_back(var);

    BpVariable v2;
    v2.name = "origin";
    v2.type = BpVarType::Vec3;
    v2.default_vec[0] = 1.0f; v2.default_vec[1] = 2.0f; v2.default_vec[2] = 3.0f;
    a.variables.push_back(v2);

    BpFunctionGraph g;
    g.name = "EventGraph";
    g.next_id = 100;
    g.is_pure = false;

    BpNode n;
    n.id = 10;
    n.name = "On Update";
    n.category = "Event";
    n.comment = "tick";
    BpPin out;
    out.id = 11;
    out.name = "Then";
    out.type = BpPinType::Flow;
    out.kind = BpPinKind::Output;
    n.outputs.push_back(out);
    BpPin in;
    in.id = 12;
    in.name = "Value";
    in.type = BpPinType::Float;
    in.kind = BpPinKind::Input;
    in.default_float = 42.0f;
    n.inputs.push_back(in);
    g.nodes.push_back(n);

    BpLink link;
    link.id = 20;
    link.from_pin = 11;
    link.to_pin = 12;
    g.links.push_back(link);

    a.graphs.push_back(g);
    return a;
}

void ExpectAssetEq(const BlueprintAsset& a, const BlueprintAsset& b) {
    EXPECT_EQ(a.name, b.name);
    EXPECT_EQ(a.description, b.description);
    ASSERT_EQ(a.variables.size(), b.variables.size());
    for (size_t i = 0; i < a.variables.size(); ++i) {
        EXPECT_EQ(a.variables[i].name, b.variables[i].name);
        EXPECT_EQ(static_cast<int>(a.variables[i].type), static_cast<int>(b.variables[i].type));
        EXPECT_FLOAT_EQ(a.variables[i].default_float, b.variables[i].default_float);
        EXPECT_EQ(a.variables[i].is_exposed, b.variables[i].is_exposed);
        for (int k = 0; k < 4; ++k)
            EXPECT_FLOAT_EQ(a.variables[i].default_vec[k], b.variables[i].default_vec[k]);
    }
    ASSERT_EQ(a.graphs.size(), b.graphs.size());
    for (size_t i = 0; i < a.graphs.size(); ++i) {
        const auto& ga = a.graphs[i];
        const auto& gb = b.graphs[i];
        EXPECT_EQ(ga.name, gb.name);
        EXPECT_EQ(ga.next_id, gb.next_id);
        EXPECT_EQ(ga.is_pure, gb.is_pure);
        ASSERT_EQ(ga.nodes.size(), gb.nodes.size());
        for (size_t j = 0; j < ga.nodes.size(); ++j) {
            EXPECT_EQ(ga.nodes[j].id, gb.nodes[j].id);
            EXPECT_EQ(ga.nodes[j].name, gb.nodes[j].name);
            EXPECT_EQ(ga.nodes[j].category, gb.nodes[j].category);
            EXPECT_EQ(ga.nodes[j].comment, gb.nodes[j].comment);
            ASSERT_EQ(ga.nodes[j].inputs.size(), gb.nodes[j].inputs.size());
            ASSERT_EQ(ga.nodes[j].outputs.size(), gb.nodes[j].outputs.size());
        }
        ASSERT_EQ(ga.links.size(), gb.links.size());
        for (size_t j = 0; j < ga.links.size(); ++j) {
            EXPECT_EQ(ga.links[j].id, gb.links[j].id);
            EXPECT_EQ(ga.links[j].from_pin, gb.links[j].from_pin);
            EXPECT_EQ(ga.links[j].to_pin, gb.links[j].to_pin);
        }
    }
}

TEST(BlueprintSerialize, RoundTripPreservesSemantics) {
    BlueprintAsset original = MakeSampleAsset();
    std::string json = SerializeBlueprintAsset(original);
    ASSERT_FALSE(json.empty());

    BlueprintAsset loaded;
    BlueprintDiagnostics diag;
    ASSERT_TRUE(DeserializeBlueprintAsset(loaded, json, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_TRUE(diag.errors.empty());
    EXPECT_EQ(diag.source_version, kBlueprintSchemaVersion);
    EXPECT_FALSE(diag.migrated);
    ExpectAssetEq(original, loaded);
}

TEST(BlueprintSerialize, RejectsCorruptJson) {
    BlueprintAsset asset;
    BlueprintDiagnostics diag;
    EXPECT_FALSE(DeserializeBlueprintAsset(asset, "{ this is not valid json ", diag));
    EXPECT_FALSE(diag.ok);
    EXPECT_FALSE(diag.errors.empty());
}

TEST(BlueprintSerialize, RejectsNonObjectRoot) {
    BlueprintAsset asset;
    BlueprintDiagnostics diag;
    EXPECT_FALSE(DeserializeBlueprintAsset(asset, "[1,2,3]", diag));
    EXPECT_FALSE(diag.ok);
    EXPECT_FALSE(diag.errors.empty());
}

TEST(BlueprintSerialize, ForwardCompatRecordsUnknownFieldsAndNewerVersion) {
    const char* json =
        "{ \"name\": \"Fut\", \"version\": 999, \"future_only_field\": 7,"
        "  \"variables\": [], \"graphs\": [] }";
    BlueprintAsset asset;
    BlueprintDiagnostics diag;
    ASSERT_TRUE(DeserializeBlueprintAsset(asset, json, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(asset.name, "Fut");
    EXPECT_EQ(diag.source_version, 999);
    // 未知字段 + 前向版本 都应记录为 warning，而非静默丢弃或失败。
    bool saw_unknown = false, saw_version = false;
    for (const auto& wmsg : diag.warnings) {
        if (wmsg.find("future_only_field") != std::string::npos) saw_unknown = true;
        if (wmsg.find("newer than supported") != std::string::npos) saw_version = true;
    }
    EXPECT_TRUE(saw_unknown);
    EXPECT_TRUE(saw_version);
}

TEST(BlueprintSerialize, MigratesLegacyMissingVersionBackfillsNextId) {
    // legacy 文件：无 version 字段，graph.next_id 缺失(=默认1)，但节点/引脚 id 高达 30。
    const char* json =
        "{ \"name\": \"Legacy\","
        "  \"variables\": [],"
        "  \"graphs\": [ { \"name\": \"EventGraph\","
        "     \"nodes\": [ { \"id\": 5, \"name\": \"On Init\","
        "        \"outputs\": [ { \"id\": 30, \"name\": \"Then\", \"type\": \"Flow\" } ] } ],"
        "     \"links\": [] } ] }";
    BlueprintAsset asset;
    BlueprintDiagnostics diag;
    ASSERT_TRUE(DeserializeBlueprintAsset(asset, json, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, 0);
    EXPECT_TRUE(diag.migrated);
    EXPECT_EQ(asset.version, kBlueprintSchemaVersion);
    ASSERT_EQ(asset.graphs.size(), 1u);
    // next_id 必须被回填到 max(id)+1 = 31，避免 id 复用。
    EXPECT_EQ(asset.graphs[0].next_id, 31);
}

TEST(BlueprintSerialize, SaveLoadFileRoundTrip) {
    BlueprintAsset original = MakeSampleAsset();
    std::string path = std::string(::testing::TempDir()) + "/dse_bp_roundtrip.dbp";

    BlueprintDiagnostics save_diag;
    ASSERT_TRUE(SaveBlueprintAsset(original, path, save_diag)) ;
    EXPECT_TRUE(save_diag.ok);

    BlueprintAsset loaded;
    BlueprintDiagnostics load_diag;
    ASSERT_TRUE(LoadBlueprintAssetChecked(loaded, path, load_diag));
    EXPECT_TRUE(load_diag.ok);
    EXPECT_EQ(loaded.file_path, path);
    ExpectAssetEq(original, loaded);
}

}  // namespace
