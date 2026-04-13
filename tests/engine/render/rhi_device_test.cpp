#include "catch/catch.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "engine/render/rhi/rhi_device.h"

TEST_CASE("Given_OpenGLRhiDevice_When_QueryingUnknownRenderTargetTextures_Then_ReturnsZero", "[engine][render][rhi]") {
    OpenGLRhiDevice device;

    REQUIRE(device.GetRenderTargetColorTexture(0) == 0);
    REQUIRE(device.GetRenderTargetColorTexture(123456u) == 0);
    REQUIRE(device.GetRenderTargetDepthTexture(0) == 0);
    REQUIRE(device.GetRenderTargetDepthTexture(123456u) == 0);
}

TEST_CASE("Given_OpenGLRhiDevice_When_CreatingPipelineStates_Then_HandlesAreUniqueAndLastFrameStatsStayStableWithoutFrames", "[engine][render][rhi]") {
    OpenGLRhiDevice device;

    PipelineStateDesc alpha_blend{};
    alpha_blend.blend_enabled = true;
    alpha_blend.depth_test_enabled = false;

    PipelineStateDesc opaque{};
    opaque.blend_enabled = false;
    opaque.depth_test_enabled = true;
    opaque.depth_write_enabled = true;

    const unsigned int first = device.CreatePipelineState(alpha_blend);
    const unsigned int second = device.CreatePipelineState(opaque);

    REQUIRE(first != 0u);
    REQUIRE(second != 0u);
    REQUIRE(second != first);
    REQUIRE(second > first);

    const RenderStats& stats = device.LastFrameStats();
    REQUIRE(stats.draw_calls == 0);
    REQUIRE(stats.material_switches == 0);
    REQUIRE(stats.sprite_count == 0);
    REQUIRE(stats.mesh_count == 0);
    REQUIRE(stats.max_batch_sprites == 0);
    REQUIRE(stats.render_passes == 0);
    REQUIRE(stats.shadow_passes == 0);
}

TEST_CASE("Given_OpenGLRhiDevice_When_CreatingCommandBuffers_Then_ReturnsDistinctOpenGLCommandBufferInstances", "[engine][render][rhi]") {
    OpenGLRhiDevice device;

    std::shared_ptr<CommandBuffer> first = device.CreateCommandBuffer();
    std::shared_ptr<CommandBuffer> second = device.CreateCommandBuffer();

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(first != second);
    REQUIRE(std::dynamic_pointer_cast<OpenGLCommandBuffer>(first) != nullptr);
    REQUIRE(std::dynamic_pointer_cast<OpenGLCommandBuffer>(second) != nullptr);
}

TEST_CASE("Given_OpenGLRhiDeviceHeader_When_MultiSpotShadowSupportIsAdded_Then_PublicApiAndShaderStateExist", "[engine][render][rhi][static]") {
    const std::string header = []() {
        std::ifstream input("engine/render/rhi/rhi_device.h", std::ios::in | std::ios::binary);
        REQUIRE(input.is_open());
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }();

    REQUIRE(header.find("SetGlobalSpotShadowMap(unsigned int index, unsigned int handle)") != std::string::npos);
    REQUIRE(header.find("SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat)") != std::string::npos);
    REQUIRE(header.find("global_spot_shadow_map_[4]") != std::string::npos);
    REQUIRE(header.find("global_spot_light_space_matrix_[4]") != std::string::npos);
    REQUIRE(header.find("int shadow_index = -1;") != std::string::npos);
}

TEST_CASE("Given_OpenGLRhiDeviceShader_When_PbrTextureMapsAreAligned_Then_ShaderAndDrawItemContainMetallicRoughnessEmissiveOcclusionPaths", "[engine][render][rhi][static]") {
    const std::string header = []() {
        std::ifstream input("engine/render/rhi/rhi_device.h", std::ios::in | std::ios::binary);
        REQUIRE(input.is_open());
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }();

    const std::string source = []() {
        std::ifstream input("engine/render/rhi/rhi_device.cpp", std::ios::in | std::ios::binary);
        REQUIRE(input.is_open());
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }();

    REQUIRE(header.find("unsigned int metallic_roughness_map_handle = 0;") != std::string::npos);
    REQUIRE(header.find("unsigned int emissive_map_handle = 0;") != std::string::npos);
    REQUIRE(header.find("unsigned int occlusion_map_handle = 0;") != std::string::npos);
    REQUIRE(header.find("int uniform_metallic_roughness_map_loc_ = -1;") != std::string::npos);
    REQUIRE(header.find("int uniform_emissive_map_loc_ = -1;") != std::string::npos);
    REQUIRE(header.find("int uniform_occlusion_map_loc_ = -1;") != std::string::npos);

    REQUIRE(source.find("uniform sampler2D u_metallic_roughness_map;") != std::string::npos);
    REQUIRE(source.find("uniform bool u_has_metallic_roughness_map;") != std::string::npos);
    REQUIRE(source.find("uniform sampler2D u_emissive_map;") != std::string::npos);
    REQUIRE(source.find("uniform bool u_has_emissive_map;") != std::string::npos);
    REQUIRE(source.find("uniform sampler2D u_occlusion_map;") != std::string::npos);
    REQUIRE(source.find("uniform bool u_has_occlusion_map;") != std::string::npos);
    REQUIRE(source.find("roughness = clamp(mrSample.g * u_material_roughness, 0.04, 1.0);") != std::string::npos);
    REQUIRE(source.find("metallic = clamp(mrSample.b * u_material_metallic, 0.0, 1.0);") != std::string::npos);
    REQUIRE(source.find("ao *= texture(u_occlusion_map, TexCoord).r;") != std::string::npos);
    REQUIRE(source.find("emissive *= texture(u_emissive_map, TexCoord).rgb;") != std::string::npos);
}
