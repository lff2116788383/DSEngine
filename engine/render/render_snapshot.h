/**
 * @file render_snapshot.h
 * @brief 渲染线程所需的 ECS 数据薄快照（< 2KB），
 *        由主线程在 CaptureThinSnapshot() 中一次性填充，渲染线程只读。
 *        消除 38 次/帧的 registry() 重复查询，为 Phase 1 流水线并行做数据隔离。
 */

#ifndef DSE_RENDER_SNAPSHOT_H
#define DSE_RENDER_SNAPSHOT_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse {
namespace render {

struct RenderThinSnapshot {

    // ── 3D 相机（最高优先级 + 最小 entity id 选出的唯一主相机）──
    struct Camera3D {
        bool valid = false;
        float fov = 60.0f;
        float near_clip = 0.1f;
        float far_clip = 1000.0f;
        glm::vec3 position{0.0f};
        glm::vec3 forward{0.0f, 0.0f, -1.0f};
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        glm::vec3 right{1.0f, 0.0f, 0.0f};
        glm::mat4 view{1.0f};              ///< lookAt(vec3(0), forward, up) — camera-relative
        glm::vec3 shadow_center{0.0f};     ///< position + forward * 50 (CSM 用)
    } camera_3d;

    /// Camera-Relative Rendering: 所有 model matrix 提交 GPU 前减去此偏移
    glm::vec3 camera_offset{0.0f};

    // ── 2D 相机 fallback ──
    struct Camera2D {
        bool valid = false;
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
    } camera_2d;

    // ── 天空盒 ──
    struct Skybox {
        bool valid = false;
        unsigned int cubemap_handle = 0;
        bool has_transform = false;
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    } skybox;

    // ── 方向光（CSMShadow / DeferredLighting / ForwardScene / ContactShadow / LightShaft / Fog / Water / RSM / DDGI）──
    struct DirectionalLight {
        bool valid = false;
        bool cast_shadow = false;
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
        float ambient_intensity = 0.1f;
        float shadow_strength = 1.0f;
        float cascade_splits[3] = {20.0f, 60.0f, 200.0f};
    } directional_light;

    // ── 聚光灯（SpotShadowPass，最多 4 盏投影阴影）──
    static constexpr int kMaxSpotShadowLights = 4;
    struct SpotLight {
        glm::vec3 position{0.0f};
        glm::vec3 forward{0.0f, -1.0f, 0.0f}; ///< world-space normalized direction
        glm::vec3 up{0.0f, 1.0f, 0.0f};        ///< world-space up (for lookAt)
        float outer_cone_angle = 17.5f;         ///< degrees
        float radius = 20.0f;
    };
    SpotLight spot_lights[kMaxSpotShadowLights];
    int spot_shadow_count = 0;

    // ── 点光源（PointShadowPass，最多 4 盏投影阴影）──
    static constexpr int kMaxPointShadowLights = 4;
    struct PointLight {
        glm::vec3 position{0.0f};
        float radius = 10.0f;
    };
    PointLight point_lights[kMaxPointShadowLights];
    int point_shadow_count = 0;

    // ── 后处理（合并 13 个 Pass 的重复 PostProcessComponent 查询）──
    struct PostProcess {
        bool valid = false;
        bool enabled = true;

        // Bloom
        bool bloom_enabled = true;
        float bloom_threshold = 1.0f;
        float bloom_intensity = 0.5f;

        // Color Grading
        bool color_grading_enabled = true;
        float exposure = 1.0f;
        float gamma = 2.2f;

        // SSAO
        bool ssao_enabled = false;
        float ssao_radius = 0.5f;
        float ssao_bias = 0.025f;

        // Auto Exposure
        bool auto_exposure_enabled = false;
        float exposure_min = 0.1f;
        float exposure_max = 10.0f;
        float adaptation_speed_up = 2.0f;
        float adaptation_speed_down = 1.0f;
        float exposure_compensation = 0.0f;

        // Color LUT
        unsigned int color_lut_handle = 0;
        float color_lut_intensity = 1.0f;

        // Vignette
        bool vignette_enabled = false;
        float vignette_intensity = 0.35f;
        float vignette_radius = 0.75f;
        float vignette_softness = 0.35f;

        // Film Grain
        bool film_grain_enabled = false;
        float film_grain_intensity = 0.08f;
        float film_grain_time_scale = 1.0f;

        // FXAA
        bool fxaa_enabled = true;

        // TAA
        bool taa_enabled = false;
        float taa_blend_factor = 0.1f;

        // Contact Shadow
        bool contact_shadow_enabled = false;
        float contact_shadow_strength = 0.5f;
        int contact_shadow_steps = 16;
        float contact_shadow_step_size = 0.5f;

        // DOF
        bool dof_enabled = false;
        float dof_focus_distance = 100.0f;
        float dof_focus_range = 50.0f;
        float dof_bokeh_radius = 4.0f;

        // Motion Blur
        bool motion_blur_enabled = false;
        float motion_blur_intensity = 1.0f;
        int motion_blur_samples = 8;

        // SSR
        bool ssr_enabled = false;
        float ssr_max_distance = 100.0f;
        float ssr_thickness = 0.5f;
        float ssr_step_size = 1.0f;
        int ssr_max_steps = 64;

        // Outline
        bool outline_enabled = false;
        glm::vec3 outline_color{0.0f};
        float outline_thickness = 1.0f;
        float outline_depth_threshold = 0.1f;
        float outline_normal_threshold = 0.4f;

        // Light Shaft
        bool light_shaft_enabled = false;
        glm::vec3 light_shaft_color{1.0f, 0.95f, 0.8f};
        float light_shaft_density = 0.84f;
        float light_shaft_weight = 0.04f;
        float light_shaft_decay = 0.97f;
        float light_shaft_exposure = 0.4f;
        float light_shaft_intensity = 1.0f;
        int light_shaft_samples = 64;

        // Volumetric Fog
        bool fog_enabled = false;
        glm::vec3 fog_color{0.70f, 0.75f, 0.85f};
        float fog_density = 0.02f;
        float fog_height_falloff = 0.3f;
        float fog_height_offset = 0.0f;
        float fog_start = 0.0f;
        float fog_end = 1000.0f;
        int fog_steps = 16;
        float fog_sun_scatter = 0.6f;
    } post_process;

    // ── 水面（WaterPass 遍历所有 enabled 实体）──
    static constexpr int kMaxWaterSurfaces = 8;
    struct WaterSurface {
        float water_level = 0.0f;
        glm::vec3 deep_color{0.0f, 0.05f, 0.15f};
        glm::vec3 shallow_color{0.0f, 0.4f, 0.55f};
        float max_depth = 30.0f;
        float transparency = 0.6f;
        float wave_amplitude = 0.15f;
        float wave_frequency = 1.5f;
        float wave_speed = 1.0f;
        glm::vec2 wave_direction{1.0f, 0.3f};
        float refraction_strength = 0.03f;
        float reflection_strength = 0.5f;
        float specular_power = 128.0f;
        float caustic_intensity = 0.3f;
        float caustic_scale = 8.0f;
        float foam_intensity = 0.5f;
        float foam_depth_threshold = 2.0f;
        float underwater_fog_density = 0.15f;
        glm::vec3 underwater_fog_color{0.0f, 0.1f, 0.2f};
    };
    WaterSurface waters[kMaxWaterSurfaces];
    int water_count = 0;

    // ── 贴花（DecalPass 遍历所有 enabled 实体）──
    static constexpr int kMaxDecals = 32;
    struct Decal {
        unsigned int albedo_texture = 0;
        glm::vec4 color{1.0f};
        float angle_fade = 0.5f;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
    };
    Decal decals[kMaxDecals];
    int decal_count = 0;

    // ── Light Probe SH（最近 probe 距离加权混合后的 9 阶 SH 系数）──
    struct LightProbeSH {
        bool valid = false;
        glm::vec4 coefficients[9] = {};
    } light_probe_sh;

    // ── DDGI 运行时配置（从 GIProbeVolumeComponent 读取）──
    struct DDGIConfig {
        bool enabled = false;
        bool needs_reinit = false;
        glm::vec3 origin{0.0f};
        glm::vec3 extent{10.0f};
        int resolution_x = 8;
        int resolution_y = 4;
        int resolution_z = 8;
        int irradiance_texels = 8;
        int visibility_texels = 16;
        int rays_per_probe = 128;
        float hysteresis = 0.97f;
        float gi_intensity = 1.0f;
        float normal_bias = 0.1f;
    } ddgi_config;

    void Reset() {
        camera_3d = Camera3D{};
        camera_offset = glm::vec3(0.0f);
        camera_2d = Camera2D{};
        skybox = Skybox{};
        directional_light = DirectionalLight{};
        spot_shadow_count = 0;
        point_shadow_count = 0;
        post_process = PostProcess{};
        water_count = 0;
        decal_count = 0;
        light_probe_sh = LightProbeSH{};
        ddgi_config = DDGIConfig{};
    }
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SNAPSHOT_H
