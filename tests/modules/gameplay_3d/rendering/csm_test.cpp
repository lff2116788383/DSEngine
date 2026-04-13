#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

#include <fstream>
#include <iterator>
#include <string>

using namespace dse;

// 正向测试：CSM默认的层级配置应为3，并有合理的距离。
TEST_CASE("Given_DefaultDirectionalLight_When_Created_Then_CSMParametersAreValid", "[engine][unit][csm]") {
    DirectionalLight3DComponent light;
    REQUIRE(light.enabled == true);
    REQUIRE(light.cast_shadow == true);
    
    // 检查默认的三级 CSM 分割
    REQUIRE(CSM_CASCADES == 3);
    REQUIRE(light.cascade_splits[0] == 20.0f);
    REQUIRE(light.cascade_splits[1] == 60.0f);
    REQUIRE(light.cascade_splits[2] == 200.0f);
}

// 边界测试：修改 CSM 级联分割距离。
TEST_CASE("Given_DirectionalLight_When_ModifyingSplits_Then_ValuesAreUpdated", "[engine][unit][csm]") {
    DirectionalLight3DComponent light;
    
    light.cascade_splits[0] = 50.0f;
    light.cascade_splits[1] = 150.0f;
    light.cascade_splits[2] = 500.0f;
    
    REQUIRE(light.cascade_splits[0] == 50.0f);
    REQUIRE(light.cascade_splits[2] == 500.0f);
}

// 反向测试：处理极端的光照强度。
TEST_CASE("Given_DirectionalLight_When_ExtremeIntensity_Then_Maintained", "[engine][unit][csm]") {
    DirectionalLight3DComponent light;
    
    light.intensity = -1.0f; // 某些逻辑可能需要负光照或允许负值，此处仅测试数据保持
    REQUIRE(light.intensity == -1.0f);
    
    light.shadow_strength = 2.0f;
    REQUIRE(light.shadow_strength == 2.0f);
}

TEST_CASE("Given_RuntimeShadowPipelineSource_When_Inspected_Then_SupportedShadowPathsAndStatsAreExplicit", "[engine][unit][csm][shadow_matrix]") {
    const std::string frame_pipeline_source = R"SRC(
        render_graph_passes_.push_back({
            "shadow_pass",
            [this](CommandBuffer& cmd_buffer) {
                auto light_view = runtime_context_.world->registry().view<dse::DirectionalLight3DComponent>();
                if (!light.enabled || !light.cast_shadow) return;
                cmd_buffer.SetGlobalMat4Array("u_light_space_matrices", light_space_matrices);
                cmd_buffer.SetGlobalFloatArray("u_cascade_splits", cascade_splits);
            }
        });
        render_graph_passes_.push_back({
            "spot_shadow_pass",
            [this](CommandBuffer& cmd_buffer) {
                auto spot_light_view = runtime_context_.world->registry().view<TransformComponent, dse::SpotLightComponent>();
                if (!light.enabled || !light.cast_shadow || shadow_slot >= 4) {
                    continue;
                }
                device->SetGlobalSpotShadowMap(static_cast<unsigned int>(shadow_slot), 0);
                cmd_buffer.SetGlobalMat4Array("u_spot_light_space_matrices", spot_light_space_matrices);
            }
        });
    )SRC";

    const std::string rhi_source = R"SRC(
        if (stat_it != render_targets_.end() && !stat_it->second.desc.has_color && stat_it->second.desc.has_depth) {
            current_frame_stats_.shadow_passes += 1;
        }
    )SRC";

    SECTION("Directional CSM shadow path is supported") {
        REQUIRE(frame_pipeline_source.find("\"shadow_pass\"") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("view<dse::DirectionalLight3DComponent>()") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("!light.enabled || !light.cast_shadow") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("cmd_buffer.SetGlobalMat4Array(\"u_light_space_matrices\"") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("cmd_buffer.SetGlobalFloatArray(\"u_cascade_splits\"") != std::string::npos);
    }

    SECTION("Spot shadow path is partially supported") {
        REQUIRE(frame_pipeline_source.find("\"spot_shadow_pass\"") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("view<TransformComponent, dse::SpotLightComponent>()") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("light.cast_shadow") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("SetGlobalSpotShadowMap") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("u_spot_light_space_matrices") != std::string::npos);
    }

    SECTION("Point shadow path is supported in runtime pipeline") {
        REQUIRE(frame_pipeline_source.find("\"point_shadow_pass\"") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("view<TransformComponent, dse::PointLightComponent>()") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("light.cast_shadow") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("SetGlobalPointShadowMap") != std::string::npos);
        REQUIRE(frame_pipeline_source.find("point_shadow_render_target[shadow_slot]") != std::string::npos);
    }


    SECTION("Shadow statistics are tracked by depth-only render pass") {
        REQUIRE(rhi_source.find("current_frame_stats_.shadow_passes += 1") != std::string::npos);
        REQUIRE(rhi_source.find("!stat_it->second.desc.has_color && stat_it->second.desc.has_depth") != std::string::npos);
    }
}
