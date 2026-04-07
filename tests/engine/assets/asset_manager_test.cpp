#include "catch/catch.hpp"

#include "engine/assets/asset_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path MakeTempDir(const std::string& suffix) {
    const auto base = std::filesystem::temp_directory_path() / ("dse_asset_manager_test_" + suffix);
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base, ec);
    return base;
}

void WriteBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file.good());
}

} // namespace

TEST_CASE("Given_DataRootAndMixedPathForms_When_NormalizeAndResolve_Then_ReturnConsistentLogicalAndDiskPaths", "[engine][assets]") {
    AssetManager asset_manager;

    const auto root = MakeTempDir("paths");
    const auto data_root = root / "runtime_data";
    const auto nested_file = data_root / "textures" / "hero.bin";
    WriteBinaryFile(nested_file, {1, 2, 3, 4});

    asset_manager.ConfigureDataRoot(data_root.string());

    const std::string logical_from_prefixed = asset_manager.NormalizeAssetPath("data/textures/hero.bin");
    const std::string logical_from_bin_prefixed = asset_manager.NormalizeAssetPath("bin/data/textures/hero.bin");
    const std::string logical_from_absolute = asset_manager.NormalizeAssetPath(nested_file.string());

    REQUIRE(logical_from_prefixed == "textures/hero.bin");
    REQUIRE(logical_from_bin_prefixed == "textures/hero.bin");
    REQUIRE(logical_from_absolute == "textures/hero.bin");

    const std::string resolved_from_logical = asset_manager.ResolveAssetPath("textures/hero.bin");
    const std::string resolved_from_prefixed = asset_manager.ResolveAssetPath("data/textures/hero.bin");
    const std::string resolved_from_absolute = asset_manager.ResolveAssetPath(nested_file.string());

    REQUIRE(resolved_from_logical == nested_file.lexically_normal().string());
    REQUIRE(resolved_from_prefixed == nested_file.lexically_normal().string());
    REQUIRE(resolved_from_absolute == nested_file.lexically_normal().string());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("Given_DataRootDiskFiles_When_LoadFileToMemory_Then_LogicalAndPrefixedPathsReadSameBytes", "[engine][assets]") {
    AssetManager asset_manager;

    const auto root = MakeTempDir("disk_read");
    const auto data_root = root / "data_root";

    WriteBinaryFile(data_root / "audio" / "click.dat", {1, 2, 3, 4});

    asset_manager.ConfigureDataRoot(data_root.string());

    std::vector<std::uint8_t> loaded;
    REQUIRE(asset_manager.LoadFileToMemory("audio/click.dat", loaded));
    REQUIRE(loaded == std::vector<std::uint8_t>({1, 2, 3, 4}));

    loaded.clear();
    REQUIRE(asset_manager.LoadFileToMemory("data/audio/click.dat", loaded));
    REQUIRE(loaded == std::vector<std::uint8_t>({1, 2, 3, 4}));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("Given_MaterialInstances_When_ListGetAndUnloadUnused_Then_OnlyLiveEntriesRemain", "[engine][assets]") {
    AssetManager asset_manager;

    auto first = asset_manager.CreateMaterialInstance("first");
    auto second = asset_manager.CreateMaterialInstance("second");

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(first->GetId() != second->GetId());
    REQUIRE(asset_manager.GetMaterialInstance(first->GetId()) == first);
    REQUIRE(asset_manager.GetMaterialInstance(second->GetId()) == second);

    auto ids = asset_manager.ListMaterialInstanceIds();
    std::sort(ids.begin(), ids.end());
    REQUIRE(ids == std::vector<unsigned int>{first->GetId(), second->GetId()});

    const unsigned int expired_id = first->GetId();
    const unsigned int live_id = second->GetId();
    first.reset();
    asset_manager.UnloadUnused();

    REQUIRE(asset_manager.GetMaterialInstance(expired_id) == nullptr);
    REQUIRE(asset_manager.GetMaterialInstance(live_id) == second);

    ids = asset_manager.ListMaterialInstanceIds();
    REQUIRE(ids == std::vector<unsigned int>{live_id});
}
