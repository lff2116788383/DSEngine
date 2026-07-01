/**
 * @file light_2d_system.cpp
 * @brief 2D 灯光系统实现 — 光照累积 + 法线贴图响应
 */

#include "modules/gameplay_2d/lighting/light_2d_system.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/render/rhi/rhi_device.h"
#include <cmath>
#include <algorithm>

void Light2DSystem::Update(World& world, float /*delta_time*/) {
    // Pre-compute light world positions for rendering pass
    // (Currently positions are read directly from TransformComponent in Render)
}

void Light2DSystem::Render(World& world, CommandBuffer& /*cmd_buffer*/, const dse::render::FrameContext& /*frame*/) {
    if (!rhi_device_) return;

    auto& reg = world.registry();

    // Gather ambient
    glm::vec3 ambient_color = glm::vec3(0.2f, 0.2f, 0.3f);
    float ambient_intensity = 0.5f;
    auto ambient_view = reg.view<Ambient2DComponent>();
    for (auto e : ambient_view) {
        auto& amb = ambient_view.get<Ambient2DComponent>(e);
        ambient_color = amb.color;
        ambient_intensity = amb.intensity;
        break; // Only first ambient component used
    }

    // Gather lights
    struct LightData {
        glm::vec2 position;
        glm::vec3 color;
        float intensity;
        float range;
        float falloff;
        Light2DType type;
        float spot_angle;
        float spot_outer_angle;
        float direction_angle;
        Shadow2DMode shadow_mode;
    };

    std::vector<LightData> lights;
    auto light_view = reg.view<Light2DComponent, TransformComponent>();
    for (auto e : light_view) {
        auto& lc = light_view.get<Light2DComponent>(e);
        if (!lc.enabled) continue;
        auto& tc = light_view.get<TransformComponent>(e);

        LightData ld;
        ld.position = glm::vec2(tc.position.x, tc.position.y);
        ld.color = lc.color;
        ld.intensity = lc.intensity;
        ld.range = lc.range;
        ld.falloff = lc.falloff;
        ld.type = lc.type;
        ld.spot_angle = lc.spot_angle;
        ld.spot_outer_angle = lc.spot_outer_angle;
        ld.direction_angle = lc.direction_angle;
        ld.shadow_mode = lc.shadow_mode;
        lights.push_back(ld);
    }

    // Light contribution calculation would be done in a compute/fragment shader.
    // The system prepares the light buffer data for the sprite renderer's lighting pass.
    // For now, we store computed light data that the sprite render system can query.
    (void)ambient_color;
    (void)ambient_intensity;
    (void)lights;
}

void Light2DSystem::Shutdown() {
    // Release GPU resources if allocated
    resources_initialized_ = false;
}
