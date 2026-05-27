#ifndef DSE_COMPONENTS_3D_SNOW_H
#define DSE_COMPONENTS_3D_SNOW_H

#include <glm/glm.hpp>
#include <string>

namespace dse {

/// 雪地覆盖组件 — 挂在地形/Mesh 实体上，控制雪覆盖渲染
struct SnowCoverComponent {
    bool enabled = true;

    // ── 积雪状态 ──
    float coverage = 0.0f;            ///< 当前雪覆盖率 [0,1]，由系统自动驱动
    float target_coverage = 0.0f;     ///< 目标覆盖率（由天气系统写入）

    // ── 积雪速率 ──
    float accumulation_rate = 0.08f;  ///< 积雪速率 (coverage/秒, 下雪时)
    float melt_rate = 0.02f;          ///< 融雪速率 (coverage/秒, 停雪后)

    // ── 外观参数 ──
    glm::vec3 snow_albedo = {0.92f, 0.93f, 0.96f};   ///< 雪面反照率
    float snow_roughness = 0.75f;                      ///< 雪面粗糙度
    float snow_metallic = 0.0f;                        ///< 雪面金属度

    float normal_threshold = 0.4f;    ///< 法线 Y 阈值: N.y > threshold 的面才积雪
    float edge_sharpness = 3.0f;      ///< 积雪边缘锐利度 (pow 指数)

    // ── 雪面纹理 ──
    std::string snow_texture_path;    ///< 雪面细节纹理（可选）
    unsigned int snow_texture_handle = 0;
    float snow_tiling = 8.0f;         ///< 雪面纹理 UV 缩放

    // ── 雪面高度偏移（顶点位移）──
    float displacement_height = 0.05f; ///< 积雪最大厚度位移 (m)

    // ── 脚印/痕迹（预留） ──
    float deformation_strength = 0.0f; ///< 变形强度 [0,1] (未来用于脚印系统)
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_SNOW_H
