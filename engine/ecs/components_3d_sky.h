#ifndef DSE_COMPONENTS_3D_SKY_H
#define DSE_COMPONENTS_3D_SKY_H

#include <string>
#include <glm/glm.hpp>

namespace dse {

/// 大气散射组件 — 物理天空参数（Bruneton 2017 简化）
struct AtmosphereComponent {
    bool enabled = true;

    float planet_radius     = 6371000.0f;  ///< 地球半径 (m)
    float atmosphere_height = 100000.0f;   ///< 大气层厚度 (m)

    // Rayleigh 散射
    glm::vec3 rayleigh_coeff = {5.8e-6f, 13.5e-6f, 33.1e-6f};
    float rayleigh_scale_height = 8500.0f; ///< 标高 (m)

    // Mie 散射
    float mie_coeff        = 21e-6f;       ///< Mie 散射系数
    float mie_scale_height = 1200.0f;      ///< Mie 标高 (m)
    float mie_g            = 0.76f;        ///< Henyey-Greenstein 各向异性因子 [-1,1]
    glm::vec3 mie_albedo   = glm::vec3(0.9f); ///< Mie 单次散射反照率

    // Ozone 吸收（可选，增加落日红/蓝色准确度）
    glm::vec3 ozone_coeff  = {0.65e-6f, 1.88e-6f, 0.085e-6f};
    float ozone_center_h   = 25000.0f;    ///< 臭氧层中心高度 (m)
    float ozone_width       = 15000.0f;    ///< 臭氧层宽度 (m)

    // 太阳
    glm::vec3 sun_intensity = glm::vec3(20.0f); ///< 太阳辐照度 (用于 HDR)
    float sun_disk_angle    = 0.53f;       ///< 太阳视角直径 (度)

    // 渲染质量
    int transmittance_lut_width  = 256;
    int transmittance_lut_height = 64;
    int sky_view_steps   = 32;             ///< 天空渲染积分步数
    bool aerial_perspective_enabled = true;
};

/// 体积云组件 — Guerrilla-style raymarching
struct VolumetricCloudComponent {
    bool enabled = true;

    // 云层范围
    float cloud_bottom = 1500.0f;          ///< 云层底部高度 (m)
    float cloud_top    = 4000.0f;          ///< 云层顶部高度 (m)

    // 形状
    float coverage       = 0.5f;           ///< 全局覆盖率 [0,1]
    float density        = 0.04f;          ///< 密度系数
    float shape_scale    = 0.0003f;        ///< 主 noise 缩放 (world units)
    float detail_scale   = 0.0015f;        ///< 细节 noise 缩放
    float detail_strength = 0.35f;         ///< 细节强度 [0,1]
    float erosion        = 0.4f;           ///< 边缘侵蚀 [0,1]

    // 风
    glm::vec2 wind_direction = {1.0f, 0.0f};
    float wind_speed = 20.0f;              ///< m/s

    // 光照
    float silver_intensity  = 0.7f;        ///< Silver lining 强度
    float silver_spread     = 0.1f;        ///< Silver lining 展宽
    float powder_strength   = 2.0f;        ///< Powder/多散射近似强度
    float ambient_strength  = 0.3f;        ///< 环境光贡献

    // Raymarching 质量
    int march_steps       = 64;            ///< 主光线步数
    int light_march_steps = 6;             ///< 光照采样步数
    bool half_resolution  = true;          ///< 半分辨率渲染
    bool temporal_reprojection = true;     ///< 时序复投影

    // 天气图（2D 空间覆盖率控制）
    std::string weather_map_path;          ///< 天气贴图路径 (R=coverage, G=type)
    float weather_map_scale = 0.00001f;    ///< 天气图世界缩放

    // 云阴影
    bool cloud_shadow_enabled = true;
    int cloud_shadow_resolution = 512;     ///< 云阴影图分辨率
};

/// 昼夜循环组件 — 驱动太阳方向和天空色温
struct DayNightCycleComponent {
    bool enabled = true;

    float time_of_day    = 12.0f;          ///< 当前时刻 [0, 24)
    float time_speed     = 1.0f;           ///< 时间流速（1.0 = 实时）
    bool auto_advance    = false;          ///< 是否自动推进时间

    // 地理位置（影响太阳轨迹）
    float latitude       = 30.0f;          ///< 纬度 (度, 北半球正)
    float longitude      = 120.0f;         ///< 经度 (度)
    int day_of_year      = 172;            ///< 一年中的第几天（影响太阳高度角）

    // 输出（由 DayNightCycleSystem 每帧写入）
    glm::vec3 sun_direction_ = glm::vec3(0, -1, 0); ///< 归一化太阳方向 (指向太阳)
    float sun_elevation_     = 90.0f;      ///< 太阳仰角 (度)
    glm::vec3 sun_color_     = glm::vec3(1.0f); ///< 基于大气厚度衰减的太阳颜色
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_SKY_H
