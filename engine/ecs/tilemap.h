/**
 * @file tilemap.h
 * @brief 瓦片地图组件 — 增强版（多层、动画瓦片、瓦片属性、.dtilemap 格式）
 *
 * 增强内容（对标主流引擎）:
 * - 多层 TilemapLayer（背景层/前景层/碰撞层等，独立排序和可见性）
 * - 动画瓦片 TileAnimation（帧序列 + 帧时长，运行时自动切换 UV）
 * - 瓦片属性 TileProperties（碰撞标记、自定义属性键值对，供游戏逻辑读取）
 * - .dtilemap 二进制序列化格式（Save/Load）
 */

#ifndef DSE_ECS_COMPONENTS_2D_TILEMAP_H
#define DSE_ECS_COMPONENTS_2D_TILEMAP_H

#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/texture_ref.h"
#include <entt/entt.hpp>

class TextureAsset;
using Entity = entt::entity;

// ── 动画瓦片 ──────────────────────────────────────────────────────────────

/// 动画瓦片帧
struct TileAnimationFrame {
    int tile_id = 0;       ///< 此帧使用的 tile_id（在图集中的索引+1）
    float duration = 0.2f; ///< 此帧持续时长（秒）
};

/// 瓦片动画定义
struct TileAnimation {
    std::vector<TileAnimationFrame> frames;  ///< 动画帧序列
    bool loop = true;                         ///< 是否循环
    float total_duration = 0.0f;              ///< 总时长（自动计算）

    /// 计算总时长
    void ComputeDuration() {
        total_duration = 0.0f;
        for (const auto& f : frames) total_duration += f.duration;
    }

    /// 在时间 t 处获取当前帧的 tile_id
    int GetFrameAt(float t) const {
        if (frames.empty()) return 0;
        if (total_duration <= 0.0f) return frames.front().tile_id;
        if (!loop && t >= total_duration) return frames.back().tile_id;
        if (loop) {
            t = std::fmod(t, total_duration);
            if (t < 0.0f) t += total_duration;
        }
        float elapsed = 0.0f;
        for (const auto& f : frames) {
            elapsed += f.duration;
            if (t < elapsed) return f.tile_id;
        }
        return frames.back().tile_id;
    }
};

// ── 瓦片属性 ──────────────────────────────────────────────────────────────

/// 瓦片自定义属性（键值对，供游戏逻辑读取）
struct TileProperties {
    bool solid = false;          ///< 是否为实心碰撞体
    int collision_type = 0;      ///< 碰撞类型 (0=无, 1=实心, 2=单向, 3=斜坡)
    float friction = 0.4f;       ///< 摩擦系数
    float restitution = 0.0f;   ///< 弹性系数
    std::unordered_map<std::string, std::string> custom;  ///< 自定义属性

    /// 设置自定义属性
    void Set(const std::string& key, const std::string& value) {
        custom[key] = value;
    }

    /// 获取自定义属性
    std::string Get(const std::string& key, const std::string& default_val = "") const {
        auto it = custom.find(key);
        return it != custom.end() ? it->second : default_val;
    }

    /// 检查是否存在自定义属性
    bool Has(const std::string& key) const {
        return custom.find(key) != custom.end();
    }
};

// ── 瓦片地图层 ────────────────────────────────────────────────────────────

/// 瓦片地图层定义
struct TilemapLayer {
    std::string name;                                ///< 层名称
    std::vector<int> tiles;                          ///< 一维数组存储的瓦片 ID (0 为空)
    int width = 0;                                    ///< 本层列数
    int height = 0;                                   ///< 本层行数
    float opacity = 1.0f;                             ///< 透明度
    bool visible = true;                              ///< 可见性
    int sorting_layer = 0;                           ///< 排序层
    int order_in_layer_base = 0;                     ///< 层内排序基准
    bool generate_colliders = false;                  ///< 是否生成碰撞体
    int collider_tile_min = 1;                       ///< 碰撞体最小 tile_id
    bool dirty = true;                                ///< 脏标记
    std::vector<Entity> runtime_tile_entities;       ///< 运行时生成的实体
};

// ── 瓦片地图组件 ──────────────────────────────────────────────────────────

/**
 * @struct TilemapComponent
 * @brief 瓦片地图组件 — 增强版
 *
 * 兼容性说明:
 * - 旧代码可直接使用 tiles/width/height/tile_size 等字段（单层模式）
 * - 新代码可使用 layers 数组实现多层
 * - 当 layers 为空时，系统自动回退到单层模式（使用 tiles 字段）
 */
struct TilemapComponent {
    // ── 单层模式字段（向后兼容）──
    std::vector<int> tiles;                              ///< 一维数组存储的瓦片 ID (0 为空)
    int width = 0;                                       ///< 地图的列数
    int height = 0;                                      ///< 地图的行数
    float tile_size = 1.0f;                              ///< 单个瓦片的物理/渲染尺寸
    std::shared_ptr<TextureAsset> tileset_texture;       ///< 引用的瓦片图集纹理
    dse::render::TextureRef tileset_handle;           ///< 图集的 RHI 渲染句柄
    int tileset_cols = 1;
    int tileset_rows = 1;
    int sorting_layer = 0;
    int order_in_layer_base = 0;
    bool generate_colliders = false;
    int collider_tile_min = 1;
    bool dirty = true;
    std::vector<Entity> runtime_tile_entities;

    // ── 多层模式字段（新增）──
    std::vector<TilemapLayer> layers;                   ///< 瓦片地图层列表

    // ── 动画与属性（新增）──
    std::unordered_map<int, TileAnimation> animations;  ///< tile_id → 动画定义
    std::unordered_map<int, TileProperties> properties;  ///< tile_id → 属性

    // ── 运行时动画状态 ──
    float animation_time = 0.0f;                         ///< 动画累计时间

    // ── 便捷方法 ──

    /// 添加一个动画
    void AddAnimation(int tile_id, const TileAnimation& anim) {
        auto a = anim;
        a.ComputeDuration();
        animations[tile_id] = std::move(a);
    }

    /// 获取瓦片属性（如果不存在返回默认）
    const TileProperties& GetProperties(int tile_id) const {
        static const TileProperties default_props;
        auto it = properties.find(tile_id);
        return it != properties.end() ? it->second : default_props;
    }

    /// 设置瓦片属性
    void SetProperties(int tile_id, const TileProperties& props) {
        properties[tile_id] = props;
    }

    /// 添加层
    int AddLayer(const std::string& name) {
        TilemapLayer layer;
        layer.name = name;
        layer.width = width;
        layer.height = height;
        layer.tiles.resize(static_cast<size_t>(width * height), 0);
        layers.push_back(std::move(layer));
        return static_cast<int>(layers.size()) - 1;
    }

    /// 获取层（只读）
    const TilemapLayer* GetLayer(int index) const {
        if (index < 0 || index >= static_cast<int>(layers.size())) return nullptr;
        return &layers[index];
    }

    /// 获取层（可写）
    TilemapLayer* GetLayerMut(int index) {
        if (index < 0 || index >= static_cast<int>(layers.size())) return nullptr;
        return &layers[index];
    }

    /// 获取层数
    int LayerCount() const { return static_cast<int>(layers.size()); }

    /// 标记所有层为脏
    void MarkAllDirty() {
        dirty = true;
        for (auto& layer : layers) layer.dirty = true;
    }
};

// ── .dtilemap 序列化格式 ──────────────────────────────────────────────────

/**
 * .dtilemap 二进制格式:
 *
 * Header:
 *   magic: "DTM1" (4 bytes)
 *   version: uint32 (1)
 *   width: int32
 *   height: int32
 *   tile_size: float32
 *   tileset_cols: int32
 *   tileset_rows: int32
 *   tileset_path_len: int32
 *   tileset_path: bytes[tileset_path_len]
 *
 * Layer count: int32
 * For each layer:
 *   name_len: int32
 *   name: bytes[name_len]
 *   opacity: float32
 *   visible: uint8
 *   sorting_layer: int32
 *   order_in_layer_base: int32
 *   generate_colliders: uint8
 *   collider_tile_min: int32
 *   tiles: int32[width * height]
 *
 * Animation count: int32
 * For each animation:
 *   tile_id: int32
 *   loop: uint8
 *   frame_count: int32
 *   For each frame:
 *     tile_id: int32
 *     duration: float32
 *
 * Property count: int32
 * For each property:
 *   tile_id: int32
 *   solid: uint8
 *   collision_type: int32
 *   friction: float32
 *   restitution: float32
 *   custom_count: int32
 *   For each custom:
 *     key_len: int32
 *     key: bytes[key_len]
 *     val_len: int32
 *     val: bytes[val_len]
 */

/// .dtilemap 文件序列化器
class TilemapSerializer {
public:
    /// 保存 TilemapComponent 到二进制数据
    static std::vector<uint8_t> Save(const TilemapComponent& tm,
                                      const std::string& tileset_path = "");

    /// 从二进制数据加载 TilemapComponent
    /// @return true 成功
    static bool Load(const uint8_t* data, size_t size,
                     TilemapComponent& out_tm,
                     std::string& out_tileset_path);

    /// 从文件加载
    static bool LoadFromFile(const std::string& filepath,
                            TilemapComponent& out_tm,
                            std::string& out_tileset_path);

    /// 保存到文件
    static bool SaveToFile(const std::string& filepath,
                           const TilemapComponent& tm,
                           const std::string& tileset_path = "");
};

#endif // DSE_ECS_COMPONENTS_2D_TILEMAP_H
