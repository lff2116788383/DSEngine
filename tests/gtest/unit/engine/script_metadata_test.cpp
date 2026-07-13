#include <gtest/gtest.h>

#include "engine/scripting/script_metadata.h"

using namespace dse::scripting;

namespace {

ScriptMetadata MakeSample() {
    ScriptMetadata m;
    m.class_name = "PlayerController";
    m.full_name = "Game.PlayerController";
    m.base_type = "DseScript";
    m.assembly = "DSEngine.Game";
    m.source_path = "GameScripts/DSEngine.Game/PlayerController.cs";

    ScriptFieldMeta speed;
    speed.name = "MoveSpeed";
    speed.type = ScriptFieldType::Float;
    speed.tooltip = "Units per second";
    speed.number_default[0] = 5.0f;
    m.fields.push_back(speed);

    ScriptFieldMeta start;
    start.name = "StartPos";
    start.type = ScriptFieldType::Vec3;
    start.number_default[0] = 1.0f;
    start.number_default[1] = 2.0f;
    start.number_default[2] = 3.0f;
    m.fields.push_back(start);

    ScriptFieldMeta title;
    title.name = "Title";
    title.type = ScriptFieldType::String;
    title.string_default = "Hero";
    m.fields.push_back(title);

    return m;
}

}  // namespace

TEST(ScriptMetadata, RoundTripPreservesMetadata) {
    ScriptMetadata src = MakeSample();
    std::string json = SerializeScriptMetadata(src);

    ScriptMetadata dst;
    ScriptMetaDiagnostics diag;
    ASSERT_TRUE(DeserializeScriptMetadata(json, dst, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_FALSE(diag.migrated);
    EXPECT_EQ(diag.source_version, kScriptMetaSchemaVersion);

    EXPECT_EQ(dst.class_name, "PlayerController");
    EXPECT_EQ(dst.full_name, "Game.PlayerController");
    EXPECT_EQ(dst.assembly, "DSEngine.Game");
    ASSERT_EQ(dst.fields.size(), 3u);
    EXPECT_EQ(dst.fields[0].name, "MoveSpeed");
    EXPECT_EQ(dst.fields[0].type, ScriptFieldType::Float);
    EXPECT_FLOAT_EQ(dst.fields[0].number_default[0], 5.0f);
    EXPECT_EQ(dst.fields[1].type, ScriptFieldType::Vec3);
    EXPECT_FLOAT_EQ(dst.fields[1].number_default[2], 3.0f);
    EXPECT_EQ(dst.fields[2].type, ScriptFieldType::String);
    EXPECT_EQ(dst.fields[2].string_default, "Hero");
}

TEST(ScriptMetadata, SerializedFormHasVersionEnvelope) {
    std::string json = SerializeScriptMetadata(MakeSample());
    EXPECT_NE(json.find("\"version\""), std::string::npos);
    EXPECT_NE(json.find("\"script\""), std::string::npos);
}

TEST(ScriptMetadata, LegacyUnversionedMigrates) {
    const char* legacy =
        "{\"class_name\":\"Enemy\",\"full_name\":\"Game.Enemy\","
        "\"base_type\":\"DseScript\",\"assembly\":\"DSEngine.Game\","
        "\"fields\":[{\"name\":\"Health\",\"type\":\"Int\","
        "\"number_default\":[100,0,0,0]}]}";

    ScriptMetadata dst;
    ScriptMetaDiagnostics diag;
    ASSERT_TRUE(DeserializeScriptMetadata(legacy, dst, diag));
    EXPECT_TRUE(diag.migrated);
    EXPECT_EQ(diag.source_version, 0);
    EXPECT_EQ(dst.class_name, "Enemy");
    ASSERT_EQ(dst.fields.size(), 1u);
    EXPECT_EQ(dst.fields[0].type, ScriptFieldType::Int);
    EXPECT_FLOAT_EQ(dst.fields[0].number_default[0], 100.0f);
}

TEST(ScriptMetadata, NewerVersionWarnsButReadsBestEffort) {
    std::string json = SerializeScriptMetadata(MakeSample());
    auto pos = json.find("\"version\":1");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::string("\"version\":1").size(), "\"version\":999");

    ScriptMetadata dst;
    ScriptMetaDiagnostics diag;
    ASSERT_TRUE(DeserializeScriptMetadata(json, dst, diag));
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.warnings.empty());
    EXPECT_EQ(dst.class_name, "PlayerController");
}

TEST(ScriptMetadata, ValidateRejectsEmptyClassName) {
    ScriptMetadata m = MakeSample();
    m.class_name.clear();
    std::string error;
    EXPECT_FALSE(ValidateScriptMetadata(m, error));
    EXPECT_FALSE(error.empty());
}

TEST(ScriptMetadata, ValidateRejectsDuplicateFieldNames) {
    ScriptMetadata m = MakeSample();
    m.fields[1].name = "MoveSpeed";  // collide with fields[0]
    std::string error;
    EXPECT_FALSE(ValidateScriptMetadata(m, error));
}

TEST(ScriptMetadata, ValidateAcceptsWellFormed) {
    std::string error;
    EXPECT_TRUE(ValidateScriptMetadata(MakeSample(), error)) << error;
}
