#include <gtest/gtest.h>

#include <rapidjson/document.h>

#include "engine/core/asset_diagnostics.h"
#include "engine/core/asset_version_envelope.h"

using dse::assets::AssetDiagnostics;
using dse::assets::NoteForwardCompat;
using dse::assets::ReadVersionEnvelope;
using dse::assets::WriteVersionEnvelope;

TEST(AssetDiagnostics, DefaultsAreLegacyAndNotOk) {
    AssetDiagnostics diag;
    EXPECT_FALSE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_FALSE(diag.migrated);
    EXPECT_TRUE(diag.errors.empty());
    EXPECT_TRUE(diag.warnings.empty());
}

TEST(AssetDiagnostics, NoWarningWhenSourceVersionSupported) {
    AssetDiagnostics diag;
    NoteForwardCompat(1, 1, ".dbp", diag);
    EXPECT_TRUE(diag.warnings.empty());

    NoteForwardCompat(0, 1, ".dbp", diag);
    EXPECT_TRUE(diag.warnings.empty());
}

TEST(AssetDiagnostics, WarnsWhenSourceVersionNewer) {
    AssetDiagnostics diag;
    NoteForwardCompat(3, 1, ".dcutscene", diag);
    ASSERT_EQ(diag.warnings.size(), 1u);
    const std::string& w = diag.warnings.front();
    EXPECT_NE(w.find("newer than supported"), std::string::npos);
    EXPECT_NE(w.find(".dcutscene"), std::string::npos);
    EXPECT_NE(w.find('3'), std::string::npos);
}

TEST(AssetDiagnostics, NullFormatNameDoesNotCrash) {
    AssetDiagnostics diag;
    NoteForwardCompat(2, 1, nullptr, diag);
    ASSERT_EQ(diag.warnings.size(), 1u);
    EXPECT_NE(diag.warnings.front().find("newer than supported"), std::string::npos);
}

// ── Shared version-envelope helper (rapidjson) ──────────────────────────────

TEST(AssetVersionEnvelope, RoundTripWritesAndReadsVersion) {
    rapidjson::Document doc;
    doc.SetObject();
    WriteVersionEnvelope(doc, 3, doc.GetAllocator());
    ASSERT_TRUE(doc.HasMember("version"));
    EXPECT_EQ(doc["version"].GetInt(), 3);

    AssetDiagnostics diag;
    int v = ReadVersionEnvelope(doc, 3, ".dbp", diag);
    EXPECT_EQ(v, 3);
    EXPECT_EQ(diag.source_version, 3);
    EXPECT_TRUE(diag.warnings.empty());
}

TEST(AssetVersionEnvelope, MissingVersionIsLegacy) {
    rapidjson::Document doc;
    doc.SetObject();  // no "version" field
    AssetDiagnostics diag;
    int v = ReadVersionEnvelope(doc, 1, ".dasm", diag);
    EXPECT_EQ(v, dse::assets::kSchemaVersionLegacy);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.warnings.empty());
}

TEST(AssetVersionEnvelope, NewerVersionAppliesForwardCompatWarning) {
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("version", 9, doc.GetAllocator());
    AssetDiagnostics diag;
    int v = ReadVersionEnvelope(doc, 2, ".dshadergraph", diag);
    EXPECT_EQ(v, 9);
    ASSERT_EQ(diag.warnings.size(), 1u);
    EXPECT_NE(diag.warnings.front().find(".dshadergraph"), std::string::npos);
    EXPECT_NE(diag.warnings.front().find("newer than supported"), std::string::npos);
}
