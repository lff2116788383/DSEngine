/**
 * @file lightmap_baker.h
 * @brief GI 烘焙 / 光照贴图系统
 *
 * 离线 CPU 光线追踪烘焙器，生成光照贴图（irradiance map）用于静态场景 GI：
 * - 对场景中标记为 static 的 mesh 生成 lightmap UV（使用已有 UV2 或自动展开）
 * - 逐 texel 发射半球采样光线，收集直接光 + 多 bounce 间接光
 * - 产出 .dlightmap 纹理图集（R11G11B10F 或 RGBE8）
 * - 运行时由 LightmapComponent 引用，fragment shader 采样叠加到间接漫反射
 *
 * 特性：
 * - 多线程并行烘焙（按 texel tile 分 job）
 * - Progressive 模式（逐步采样细化）
 * - 直接光 + N-bounce 间接光（可配 1-4 bounce）
 * - AO 通道可选同时烘焙
 * - 降噪后处理（简易 edge-aware bilateral filter）
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace dse {
namespace render {

/// 光照贴图烘焙配置
struct LightmapBakeConfig {
    uint32_t resolution = 512;             ///< 光照贴图分辨率（正方形）
    uint32_t samples_per_texel = 64;       ///< 每 texel 的采样数（越多越平滑）
    uint32_t bounces = 2;                  ///< 间接光反弹次数 (1=直接+一次间接)
    float bias = 0.001f;                   ///< 射线偏移（避免自交）
    bool bake_ao = true;                   ///< 是否同时烘焙 AO
    float ao_radius = 2.0f;               ///< AO 采样半径
    bool denoise = true;                   ///< 是否降噪
    int denoise_iterations = 2;            ///< 降噪迭代次数
    float denoise_sigma_spatial = 2.0f;    ///< 空间权重 sigma
    float denoise_sigma_color = 0.5f;      ///< 颜色权重 sigma
};

/// 场景中的静态三角形（烘焙输入）
struct BakeTriangle {
    glm::vec3 v0, v1, v2;          ///< 世界空间顶点
    glm::vec3 n0, n1, n2;          ///< 顶点法线
    glm::vec2 uv0, uv1, uv2;      ///< lightmap UV (UV2)
    glm::vec3 albedo = glm::vec3(0.8f);  ///< 漫反射颜色
    uint32_t mesh_id = 0;          ///< 所属 mesh 标识
};

/// 场景中的光源
struct BakeLight {
    enum class Type : uint8_t { Directional, Point, Spot };
    Type type = Type::Directional;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;           ///< Point/Spot 衰减半径
    float spot_angle = 45.0f;      ///< Spot 外角
};

/// 烘焙场景（外部构建后传入 baker）
struct BakeScene {
    std::vector<BakeTriangle> triangles;
    std::vector<BakeLight> lights;
    glm::vec3 ambient = glm::vec3(0.03f);  ///< 环境光底色
};

/// 烘焙结果
struct LightmapResult {
    std::vector<glm::vec3> irradiance;     ///< resolution × resolution (RGB HDR)
    std::vector<float> ao;                 ///< resolution × resolution (0-1)
    uint32_t width = 0;
    uint32_t height = 0;
    bool success = false;
};

/// 进度回调 (progress 0.0~1.0)
using BakeProgressCallback = std::function<void(float progress)>;

/**
 * @class LightmapBaker
 * @brief CPU 光照贴图烘焙器
 */
class LightmapBaker {
public:
    LightmapBaker() = default;
    ~LightmapBaker() = default;

    /// 执行烘焙
    LightmapResult Bake(const BakeScene& scene, const LightmapBakeConfig& config,
                        BakeProgressCallback progress_cb = nullptr);

    /// 将结果保存为 .dlightmap 文件
    static bool SaveToFile(const LightmapResult& result, const std::string& output_path);

    /// 从 .dlightmap 文件加载
    static bool LoadFromFile(const std::string& path, LightmapResult& out);
};

/// 光照贴图运行时组件
struct LightmapComponent {
    std::string lightmap_path;          ///< .dlightmap 文件路径
    uint32_t lightmap_handle = 0;       ///< GPU 纹理句柄（运行时加载）
    float intensity = 1.0f;             ///< 光照贴图强度系数
    glm::vec4 st_offset = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f); ///< UV scale+offset (x=scaleU, y=scaleV, z=offsetU, w=offsetV)
    bool use_ao = true;                 ///< 是否应用 AO 通道
};

} // namespace render
} // namespace dse
