/**
 * @file lightmap_system.h
 * @brief Lightmap 运行时系统 —— 加载 .dlightmap 纹理并驱动 forward pass 采样
 *
 * 职责：
 * - 扫描 LightmapComponent，按需加载/卸载 .dlightmap 文件为 GPU 纹理
 * - 为拥有 lightmap 的实体在渲染时绑定纹理到 forward pass 指定槽位
 * - 管理 lightmap 纹理缓存（LRU，避免重复加载）
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/render/gi/lightmap_baker.h"

namespace dse {
namespace render {

class RhiDevice;

/// Lightmap GPU 纹理条目
struct LightmapEntry {
    uint32_t texture_handle = 0;    ///< GPU 纹理句柄
    uint32_t width = 0;
    uint32_t height = 0;
    bool has_ao = false;            ///< 是否包含 AO 通道
    uint32_t ao_texture_handle = 0; ///< AO 通道 GPU 纹理句柄
    uint32_t ref_count = 0;         ///< 引用计数
};

/**
 * @class LightmapSystem
 * @brief 管理 lightmap 纹理的加载、缓存和生命周期
 */
class LightmapSystem {
public:
    LightmapSystem() = default;
    ~LightmapSystem() = default;

    /// 初始化（传入 RHI 设备用于纹理创建）
    void Init(RhiDevice* device);

    /// 获取指定路径的 lightmap 纹理句柄（已加载时返回有效值）
    uint32_t GetTextureHandle(const std::string& path) const;

    /// 获取 AO 纹理句柄
    uint32_t GetAOTextureHandle(const std::string& path) const;

    /// 强制加载一张 lightmap（返回纹理句柄）
    uint32_t LoadLightmap(const std::string& path);

    /// 释放一张 lightmap
    void UnloadLightmap(const std::string& path);

    /// 关闭：释放所有 GPU 纹理
    void Shutdown();

    /// 获取已加载的 lightmap 数量
    size_t GetLoadedCount() const { return cache_.size(); }

    /// 判断指定路径的 lightmap 是否已加载
    bool IsLoaded(const std::string& path) const;

private:
    RhiDevice* device_ = nullptr;
    std::unordered_map<std::string, LightmapEntry> cache_;
};

} // namespace render
} // namespace dse
