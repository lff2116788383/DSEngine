#include <gtest/gtest.h>

#include "engine/core/asset_diagnostics.h"

using dse::assets::AssetDiagnostics;
using dse::assets::NoteForwardCompat;

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
