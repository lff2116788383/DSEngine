#include "catch/catch.hpp"

#include "engine/assets/asset_manager.h"
#include "engine/render/rhi/rhi_device.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

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

void WriteSolidBmp1x1(const std::filesystem::path& path, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const std::vector<std::uint8_t> bytes = {
        0x42, 0x4D, 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
        40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24, 0,
        0, 0, 0, 0, 4, 0, 0, 0, 19, 11, 0, 0, 19, 11, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        b, g, r, 0
    };
    WriteBinaryFile(path, bytes);
}

class MockRhiDevice final : public RhiDevice {
public:
    void Shutdown() override {}
    void BeginFrame() override {}
    unsigned int CreateRenderTarget(const RenderTargetDesc&) override { return 1; }
    unsigned int GetRenderTargetColorTexture(unsigned int) const override { return 1; }
    unsigned int GetRenderTargetDepthTexture(unsigned int) const override { return 1; }
    unsigned int GetRenderTargetDepthTextureFace(unsigned int, unsigned int) const override { return 1; }
    unsigned int CreateTexture2D(int, int, const unsigned char*, bool) override {
        ++create_texture2d_calls;
        return next_texture_handle++;
    }
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override {
        ++create_texture_cube_calls;
        last_cube_width = width;
        last_cube_height = height;
        last_cube_linear = linear_filter;
        last_cube_face0 = rgba8_faces[0];
        return next_texture_handle++;
    }
    void DeleteTexture(unsigned int handle) override {
        ++delete_texture_calls;
        last_deleted_texture = handle;
    }
    unsigned int CreateShaderProgram(const std::string&, const std::string&) override { return 1; }
    void DeleteShaderProgram(unsigned int) override {}
    unsigned int CreatePipelineState(const PipelineStateDesc&) override { return 1; }
    unsigned int CreateBuffer(size_t, const void*, bool, bool) override { return 1; }
    void UpdateBuffer(unsigned int, size_t, size_t, const void*, bool) override {}
    void DeleteBuffer(unsigned int) override {}
    unsigned int CreateVertexArray() override { return 1; }
    void DeleteVertexArray(unsigned int) override {}
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return nullptr; }
    void Submit(std::shared_ptr<CommandBuffer>) override {}
    void EndFrame() override {}
    const RenderStats& LastFrameStats() const override { return stats; }

    unsigned int next_texture_handle = 100;
    int create_texture2d_calls = 0;
    int create_texture_cube_calls = 0;
    int delete_texture_calls = 0;
    int last_cube_width = 0;
    int last_cube_height = 0;
    bool last_cube_linear = false;
    const unsigned char* last_cube_face0 = nullptr;
    unsigned int last_deleted_texture = 0;
    RenderStats stats{};
};

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

TEST_CASE("Given_CubemapDirectory_When_LoadedTwice_Then_CubeTextureIsCreatedAndCached", "[engine][assets][skybox]") {
    AssetManager asset_manager;
    MockRhiDevice rhi;

    const auto root = MakeTempDir("cubemap_success");
    const auto data_root = root / "data_root";
    const auto sky_dir = data_root / "skyboxes" / "default_sky";
    WriteSolidBmp1x1(sky_dir / "px.bmp", 128, 160, 200);
    WriteSolidBmp1x1(sky_dir / "nx.bmp", 110, 140, 180);
    WriteSolidBmp1x1(sky_dir / "py.bmp", 180, 205, 240);
    WriteSolidBmp1x1(sky_dir / "ny.bmp", 30, 45, 80);
    WriteSolidBmp1x1(sky_dir / "pz.bmp", 120, 155, 210);
    WriteSolidBmp1x1(sky_dir / "nz.bmp", 70, 95, 150);

    asset_manager.ConfigureDataRoot(data_root.string());
    asset_manager.SetRhiDevice(&rhi);

    auto cubemap = asset_manager.LoadCubemapDirectory("skyboxes/default_sky");
    REQUIRE(cubemap != nullptr);
    REQUIRE(cubemap->GetHandle() == 100);
    REQUIRE(cubemap->GetWidth() == 1);
    REQUIRE(cubemap->GetHeight() == 1);
    REQUIRE(rhi.create_texture_cube_calls == 1);
    REQUIRE(rhi.last_cube_width == 1);
    REQUIRE(rhi.last_cube_height == 1);
    REQUIRE(rhi.last_cube_linear);
    REQUIRE(rhi.last_cube_face0 != nullptr);

    auto cached = asset_manager.LoadCubemapDirectory("data/skyboxes/default_sky");
    REQUIRE(cached == cubemap);
    REQUIRE(rhi.create_texture_cube_calls == 1);

    asset_manager.ReleaseGpuResources();
    REQUIRE(rhi.delete_texture_calls == 1);
    REQUIRE(rhi.last_deleted_texture == 100);

    asset_manager.SetRhiDevice(nullptr);
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("Given_CubemapDirectoryMissingFace_When_Loaded_Then_ReturnsNull", "[engine][assets][skybox]") {
    AssetManager asset_manager;
    MockRhiDevice rhi;

    const auto root = MakeTempDir("cubemap_missing_face");
    const auto data_root = root / "data_root";
    const auto sky_dir = data_root / "skyboxes" / "broken_sky";
    WriteSolidBmp1x1(sky_dir / "px.bmp", 64, 96, 128);
    WriteSolidBmp1x1(sky_dir / "nx.bmp", 64, 96, 128);
    WriteSolidBmp1x1(sky_dir / "py.bmp", 64, 96, 128);
    WriteSolidBmp1x1(sky_dir / "ny.bmp", 64, 96, 128);
    WriteSolidBmp1x1(sky_dir / "pz.bmp", 64, 96, 128);

    asset_manager.ConfigureDataRoot(data_root.string());
    asset_manager.SetRhiDevice(&rhi);

    auto cubemap = asset_manager.LoadCubemapDirectory("skyboxes/broken_sky");
    REQUIRE(cubemap == nullptr);
    REQUIRE(rhi.create_texture_cube_calls == 0);

    asset_manager.SetRhiDevice(nullptr);
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

TEST_CASE("Given_MeshPbrMaterial_When_Configured_Then_PbrOverridesAreStored", "[engine][assets][material]") {
    AssetManager asset_manager;
    auto material = asset_manager.CreateMaterialInstance("mesh_pbr_master");

    REQUIRE(material != nullptr);
    REQUIRE(material->GetShaderVariant() == "MESH_PBR");
    REQUIRE(material->GetBlendMode() == MaterialBlendMode::Opaque);

    MaterialAsset::TextureSlots slots;
    slots.albedo = 11;
    slots.normal = 12;
    slots.metallic_roughness = 13;
    slots.emissive = 14;
    slots.occlusion = 15;
    material->SetTextureSlots(slots);

    MaterialAsset::ScalarOverrides scalars;
    scalars.metallic = 0.85f;
    scalars.roughness = 0.2f;
    scalars.ao = 0.9f;
    scalars.normal_strength = 1.5f;
    scalars.alpha_cutoff = 0.33f;
    material->SetScalarOverrides(scalars);
    material->SetBaseColor(glm::vec4(0.8f, 0.7f, 0.6f, 1.0f));
    material->SetEmissiveColor(glm::vec3(0.1f, 0.2f, 0.3f));

    REQUIRE(material->GetTextureSlots().albedo == 11);
    REQUIRE(material->GetTextureSlots().normal == 12);
    REQUIRE(material->GetTextureSlots().metallic_roughness == 13);
    REQUIRE(material->GetTextureSlots().emissive == 14);
    REQUIRE(material->GetTextureSlots().occlusion == 15);
    REQUIRE(material->GetScalarOverrides().metallic == Approx(0.85f));
    REQUIRE(material->GetScalarOverrides().roughness == Approx(0.2f));
    REQUIRE(material->GetScalarOverrides().ao == Approx(0.9f));
    REQUIRE(material->GetScalarOverrides().normal_strength == Approx(1.5f));
    REQUIRE(material->GetScalarOverrides().alpha_cutoff == Approx(0.33f));
    REQUIRE(material->GetBaseColor().x == Approx(0.8f));
    REQUIRE(material->GetBaseColor().y == Approx(0.7f));
    REQUIRE(material->GetBaseColor().z == Approx(0.6f));
    REQUIRE(material->GetEmissiveColor().x == Approx(0.1f));
    REQUIRE(material->GetEmissiveColor().y == Approx(0.2f));
    REQUIRE(material->GetEmissiveColor().z == Approx(0.3f));
}

TEST_CASE("Given_ReferenceDemo159StyleMaterials_When_RecreatedByIdAndUpdated_Then_MaterialInstanceHandlesRemainAddressable", "[engine][assets][material][reference_demo]") {
    AssetManager asset_manager;

    auto material0 = asset_manager.CreateMaterialInstance("reference_demo_15_9_mesh_0");
    auto material1 = asset_manager.CreateMaterialInstance("reference_demo_15_9_mesh_1");

    REQUIRE(material0 != nullptr);
    REQUIRE(material1 != nullptr);
    REQUIRE(material0->GetId() != material1->GetId());
    REQUIRE(material0->GetShaderVariant() == "MESH_PBR");
    REQUIRE(material1->GetShaderVariant() == "MESH_PBR");
    REQUIRE(material0->GetBlendMode() == MaterialBlendMode::Opaque);
    REQUIRE(material1->GetBlendMode() == MaterialBlendMode::Opaque);

    MaterialAsset::ScalarOverrides scalars0 = material0->GetScalarOverrides();
    scalars0.metallic = 0.08f;
    scalars0.roughness = 0.56f;
    material0->SetScalarOverrides(scalars0);
    material0->SetEmissiveColor(glm::vec3(0.0f, 0.02f, 0.12f));

    MaterialAsset::ScalarOverrides scalars1 = material1->GetScalarOverrides();
    scalars1.metallic = 0.12f;
    scalars1.roughness = 0.78f;
    material1->SetScalarOverrides(scalars1);
    material1->SetEmissiveColor(glm::vec3(0.1f, 0.01f, 0.0f));

    auto fetched0 = asset_manager.GetMaterialInstance(material0->GetId());
    auto fetched1 = asset_manager.GetMaterialInstance(material1->GetId());
    REQUIRE(fetched0 == material0);
    REQUIRE(fetched1 == material1);
    REQUIRE(fetched0->GetScalarOverrides().metallic == Approx(0.08f));
    REQUIRE(fetched0->GetScalarOverrides().roughness == Approx(0.56f));
    REQUIRE(fetched0->GetEmissiveColor().z == Approx(0.12f));
    REQUIRE(fetched1->GetScalarOverrides().metallic == Approx(0.12f));
    REQUIRE(fetched1->GetScalarOverrides().roughness == Approx(0.78f));
    REQUIRE(fetched1->GetEmissiveColor().x == Approx(0.1f));
}

TEST_CASE("Given_DmatMaterialFile_When_LoadMaterialInstanceFromDmat_Then_RuntimeMaterialFieldsAreMapped", "[engine][assets][material][dmat]") {
    AssetManager asset_manager;

    const auto root = MakeTempDir("dmat_load");
    const auto data_root = root / "data_root";
    std::error_code ec;
    std::filesystem::create_directories(data_root / "textures", ec);
    WriteBinaryFile(data_root / "textures" / "albedo.bin", {1, 2, 3, 4});
    WriteBinaryFile(data_root / "textures" / "normal.bin", {5, 6, 7, 8});
    WriteBinaryFile(data_root / "textures" / "mr.bin", {9, 10, 11, 12});
    WriteBinaryFile(data_root / "textures" / "emissive.bin", {13, 14, 15, 16});
    WriteBinaryFile(data_root / "textures" / "occlusion.bin", {17, 18, 19, 20});
    asset_manager.ConfigureDataRoot(data_root.string());

    const auto dmat_path = data_root / "materials.dmat";
    {
        std::ofstream out(dmat_path);
        REQUIRE(out.is_open());
        out << "{\n"
               "  \"materials\": [\n"
               "    {\n"
               "      \"name\": \"runtime_mat\",\n"
               "      \"base_color\": [0.2, 0.4, 0.6, 1.0],\n"
               "      \"metallic\": 0.9,\n"
               "      \"roughness\": 0.15,\n"
               "      \"emissive\": [0.1, 0.3, 0.5],\n"
               "      \"normal_scale\": 1.7,\n"
               "      \"occlusion_strength\": 0.55,\n"
               "      \"alpha_cutoff\": 0.25,\n"
               "      \"double_sided\": true,\n"
               "      \"alpha_test\": true,\n"
               "      \"base_color_texture\": \"textures/albedo.bin\",\n"
               "      \"normal_texture\": \"textures/normal.bin\",\n"
               "      \"metallic_roughness_texture\": \"textures/mr.bin\",\n"
               "      \"emissive_texture\": \"textures/emissive.bin\",\n"
               "      \"occlusion_texture\": \"textures/occlusion.bin\"\n"
               "    }\n"
               "  ]\n"
               "}\n";
    }

    auto material = asset_manager.LoadMaterialInstanceFromDmat("materials.dmat");
    REQUIRE(material != nullptr);
    REQUIRE(material->GetName() == "runtime_mat");
    REQUIRE(material->GetBaseColor().x == Approx(0.2f));
    REQUIRE(material->GetBaseColor().y == Approx(0.4f));
    REQUIRE(material->GetBaseColor().z == Approx(0.6f));
    REQUIRE(material->GetEmissiveColor().x == Approx(0.1f));
    REQUIRE(material->GetEmissiveColor().y == Approx(0.3f));
    REQUIRE(material->GetEmissiveColor().z == Approx(0.5f));
    REQUIRE(material->GetScalarOverrides().metallic == Approx(0.9f));
    REQUIRE(material->GetScalarOverrides().roughness == Approx(0.15f));
    REQUIRE(material->GetScalarOverrides().ao == Approx(0.55f));
    REQUIRE(material->GetScalarOverrides().normal_strength == Approx(1.7f));
    REQUIRE(material->GetScalarOverrides().alpha_cutoff == Approx(0.25f));
    REQUIRE(material->GetScalarOverrides().alpha_test);
    REQUIRE(material->GetRasterOverrides().double_sided);
    REQUIRE(material->GetTextureSlots().albedo != 0);
    REQUIRE(material->GetTextureSlots().normal != 0);
    REQUIRE(material->GetTextureSlots().metallic_roughness != 0);
    REQUIRE(material->GetTextureSlots().emissive != 0);
    REQUIRE(material->GetTextureSlots().occlusion != 0);
    REQUIRE(material->GetTextureHandle() == material->GetTextureSlots().albedo);

    std::filesystem::remove_all(root, ec);
}
