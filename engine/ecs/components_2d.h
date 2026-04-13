/**
 * @file components_2d.h
 * @brief 2D 游戏引擎的所有 ECS 数据组件定义 (纯数据结构, POD 优先)
 */

#ifndef DSE_COMPONENTS_2D_H
#define DSE_COMPONENTS_2D_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <entt/entt.hpp>

using Entity = entt::entity;

class TextureAsset;

// Forward declarations for Box2D types
class b2Body;
class b2Fixture;

/**
 * @struct TransformComponent
 * @brief 空间变换组件，定义实体在世界中的位置、旋转和缩放
 */
struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);    ///< 本地坐标
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); ///< 本地旋转(四元数)
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);       ///< 本地缩放
    glm::mat4 local_to_world = glm::mat4(1.0f);          ///< 缓存的模型矩阵(世界坐标)
    bool dirty = true;                                   ///< 标记是否需要重新计算模型矩阵
};

/**
 * @struct ParentComponent
 * @brief 场景层级组件，指明当前实体的父节点
 */
struct ParentComponent {
    Entity parent = entt::null; ///< 父实体的 ID
};

/**
 * @enum SpriteBlendMode
 * @brief 渲染混合模式枚举
 */
enum class SpriteBlendMode {
    Alpha = 0,    ///< 传统的 Alpha 透明度混合
    Additive = 1, ///< 叠加混合（发光效果）
    Multiply = 2  ///< 正片叠底混合（阴影/加深效果）
};

/**
 * @struct MaterialInstanceComponent
 * @brief 材质实例组件，覆盖全局材质的属性
 */
struct MaterialInstanceComponent {
    unsigned int material_id = 0;
    std::string name;
    std::string shader_variant = "SPRITE_UNLIT";
    SpriteBlendMode blend_mode = SpriteBlendMode::Alpha;
    unsigned int texture_handle = 0;
    glm::vec4 tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
};

/**
 * @struct SpriteRendererComponent
 * @brief 2D 精灵图渲染组件，负责在场景中绘制纹理切片
 */
struct SpriteRendererComponent {
    std::shared_ptr<TextureAsset> texture;               ///< 持有的纹理资产引用
    unsigned int texture_handle = 0;                     ///< RHI 层的纹理句柄
    unsigned int material_instance_id = 0;               ///< 绑定的材质实例 ID
    std::string shader_variant = "SPRITE_UNLIT";         ///< 使用的着色器变体
    SpriteBlendMode blend_mode = SpriteBlendMode::Alpha; ///< 混合模式
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); ///< 顶点颜色/染色
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);    ///< 纹理的采样区域 (x,y,w,h)
    glm::vec2 uv_offset = glm::vec2(0.0f, 0.0f);         ///< UV 滚动的当前偏移量
    glm::vec2 uv_scroll_speed = glm::vec2(0.0f, 0.0f);   ///< UV 滚动的速度 (x, y)
    int sorting_layer = 0;                               ///< 渲染层级(大类)
    int order_in_layer = 0;                              ///< 层级内的渲染顺序(小类)
    bool visible = true;                                 ///< 是否可见
};

/**
 * @struct SpineRendererComponent
 * @brief Spine 2D 骨骼动画渲染组件
 */
struct SpineRendererComponent {
    struct RuntimeHandle {
        virtual ~RuntimeHandle() = default;
    };

    std::string skeleton_data_path;                      ///< 骨骼数据路径 (.skel / .json)
    std::string atlas_path;                              ///< 图集路径 (.atlas)
    std::shared_ptr<RuntimeHandle> runtime;              ///< Spine runtime 统一句柄
    std::vector<std::shared_ptr<TextureAsset>> textures; ///< 持有的纹理资产
    int sorting_layer = 0;
    int order_in_layer = 0;
    bool visible = true;
    float time_scale = 1.0f;                             ///< 动画时间缩放
    std::string current_animation = "";                  ///< 当前播放的动画名
    bool loop = true;                                    ///< 是否循环
    bool dirty_animation = false;                        ///< 标记是否需要应用新动画
};

/**
 * @struct UIRendererComponent
 * @brief UI 渲染与交互组件，处理屏幕空间的 UI 绘制及鼠标事件
 */
struct UIRendererComponent {
    std::shared_ptr<TextureAsset> texture;
    unsigned int texture_handle = 0;
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int order = 0;                                       ///< UI 遮挡排序层级
    bool visible = true;
    float scale = 1.0f;                                  ///< 当前动态缩放值
    float hover_scale = 1.08f;                           ///< 鼠标悬停时的目标缩放值
    float pressed_scale = 0.94f;                         ///< 鼠标按下时的目标缩放值
    float scale_lerp_speed = 12.0f;                      ///< 缩放补间的缓动速度
    
    // UI layout params (Anchor & Flex base)
    glm::vec2 position = glm::vec2(0.0f);                ///< 相对锚点的本地像素偏移
    glm::vec2 size = glm::vec2(100.0f);                  ///< UI 元素的绝对尺寸
    glm::vec2 anchor_min = glm::vec2(0.5f);              ///< 锚点最小百分比 (0-1)
    glm::vec2 anchor_max = glm::vec2(0.5f);              ///< 锚点最大百分比 (0-1)
    glm::vec2 pivot = glm::vec2(0.5f);                   ///< UI 元素自身的轴心点 (0-1)
    
    // UI Event state
    bool interactable = true;                            ///< 是否响应交互事件
    bool is_hovered = false;                             ///< 当前是否被鼠标悬停
    bool is_pressed = false;                             ///< 当前是否被鼠标按下

    // Callbacks for Event Bubbling
    std::function<void(Entity)> on_click;                ///< C++ 层的点击回调
    std::function<void(Entity)> on_pointer_enter;        ///< C++ 层的指针移入回调
    std::function<void(Entity)> on_pointer_exit;         ///< C++ 层的指针移出回调
    
    // Runtime computed layout
    glm::mat4 runtime_model = glm::mat4(1.0f);           ///< 运行时计算出的绝对变换矩阵
};

/**
 * @struct UIPanelComponent
 * @brief UI 容器面板组件，可用于拦截背后的输入事件
 */
struct UIPanelComponent {
    bool blocks_input = false; ///< 是否阻挡射线穿透面板
};

/**
 * @struct UIButtonComponent
 * @brief 按钮组件扩展，提供不同状态下的颜色染色，并可绑定回调
 */
struct UIButtonComponent {
    glm::vec4 normal_color = glm::vec4(1.0f);                   ///< 常态颜色
    glm::vec4 hover_color = glm::vec4(1.1f, 1.1f, 1.1f, 1.0f);  ///< 悬停颜色
    glm::vec4 pressed_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);///< 按下颜色
    std::function<void(Entity)> on_click;
    std::function<void(Entity)> on_pointer_enter;
    std::function<void(Entity)> on_pointer_exit;
};

/**
 * @struct UILabelComponent
 * @brief 位图字体标签组件，支持普通字符串和高性能数字模式的渲染
 */
struct UILabelComponent {
    std::string text;                                    ///< 显示的文本
    bool use_localization = false;                       ///< 是否启用本地化文本绑定
    std::string localization_key;                        ///< 本地化键，如 ui.button.ok
    std::string fallback_text;                           ///< 本地化缺失时使用的回退文本
    std::unordered_map<std::string, std::string> localization_params; ///< 本地化参数表
    long long number_value = 0;                          ///< 数字模式下的值
    bool numeric_mode = false;                           ///< 开启数字模式可避免字符串分配开销
    unsigned int font_texture_handle = 0;                ///< 位图字体图集句柄
    glm::vec2 glyph_size = glm::vec2(16.0f, 16.0f);      ///< 单个字符的基础尺寸
    glm::vec2 offset = glm::vec2(0.0f);                  ///< 整体偏移
    float spacing = 0.0f;                                ///< 字间距
    int atlas_cols = 16;                                 ///< 图集的列数
    int atlas_rows = 6;                                  ///< 图集的行数
    int ascii_start = 32;                                ///< 图集第一个字符对应的 ASCII 码（默认空格）
    glm::vec4 color = glm::vec4(1.0f);                   ///< 文本染色
    bool dirty = true;                                   ///< 数据是否变更，需重构子实体
    std::vector<Entity> runtime_glyph_entities;          ///< 运行时管理的用于显示单个字符的子实体
};

/**
 * @struct CameraComponent
 * @brief 摄像机组件，提供投影和视图矩阵的计算参数
 */
struct CameraComponent {
    bool orthographic = true;            ///< 是否为正交投影
    bool enabled = true;
    int priority = 0;
    float orthographic_size = 5.0f;      ///< 正交模式下摄像机垂直视野的一半大小
    float fov = 60.0f;                   ///< 透视投影的视场角 (度)
    float aspect_ratio = 1.333f;         ///< 透视投影的宽高比 (width / height)
    float near_clip = -1.0f;             ///< 近裁剪面
    float far_clip = 1.0f;               ///< 远裁剪面
    glm::mat4 view = glm::mat4(1.0f);    ///< 缓存的视图矩阵
    glm::mat4 projection = glm::mat4(1.0f);///< 缓存的投影矩阵
};

/**
 * @struct CameraFollowComponent
 * @brief 摄像机跟随组件，使实体平滑追踪目标
 */
struct CameraFollowComponent {
    Entity target = entt::null;                      ///< 追踪的目标实体
    glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f);  ///< 跟随目标的相对偏移
    glm::vec2 dead_zone = glm::vec2(0.0f, 0.0f);     ///< 死区，在此区域内摄像机不移动
    float damping = 0.12f;                           ///< 缓动阻尼系数 (0 瞬间到达, 值越大越平滑)
    bool follow_x = true;                            ///< 是否在 X 轴上追踪
    bool follow_y = true;                            ///< 是否在 Y 轴上追踪
    bool enabled = true;                             ///< 是否激活
};

/**
 * @struct ScriptComponent
 * @brief Lua 脚本挂载组件，指向实体绑定的 Lua 业务逻辑
 */
struct ScriptComponent {
    std::string script_path;  ///< Lua 脚本的资源路径
    bool enabled = true;      ///< 是否执行脚本的生命周期函数
};

/**
 * @enum RigidBody2DType
 * @brief 2D 刚体类型，与 Box2D (b2BodyType) 的语义对应
 */
enum class RigidBody2DType {
    Static,    ///< 静态物体，不受力，零质量
    Kinematic, ///< 运动学物体，不受力，但可通过代码控制速度
    Dynamic    ///< 动态物体，完全受物理引擎的力与重力模拟
};

/**
 * @struct RigidBody2DComponent
 * @brief 2D 刚体组件，封装物理状态与速度
 */
struct RigidBody2DComponent {
    RigidBody2DType type = RigidBody2DType::Dynamic;
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);          ///< 线性速度
    float gravity_scale = 1.0f;                          ///< 重力缩放倍数
    bool fixed_rotation = false;                         ///< 是否锁定旋转
    
    // Internal Box2D body handle
    b2Body* runtime_body = nullptr;                         ///< 运行时绑定的 Box2D 刚体实例
    
    // Callbacks for collision events
    std::function<void(Entity other)> on_collision_enter;///< 物理碰撞进入回调
    std::function<void(Entity other)> on_collision_exit; ///< 物理碰撞离开回调
    std::function<void(Entity other)> on_trigger_enter;  ///< 触发器进入回调
    std::function<void(Entity other)> on_trigger_exit;   ///< 触发器离开回调
};

/**
 * @struct BoxCollider2DComponent
 * @brief 2D 矩形碰撞体组件，定义物理形状和材质属性
 */
struct BoxCollider2DComponent {
    glm::vec2 size = glm::vec2(1.0f, 1.0f);              ///< 碰撞体尺寸
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);            ///< 相对实体的偏移
    float density = 1.0f;                                ///< 密度 (影响质量)
    float friction = 0.3f;                               ///< 摩擦系数
    float restitution = 0.0f;                            ///< 恢复系数 (弹性)
    bool is_trigger = false;                             ///< 是否为触发器 (仅检测不产生物理力)
    
    // Internal Box2D runtime fixture pointer
    b2Fixture* runtime_fixture = nullptr;                ///< 运行时绑定的 Box2D 夹具实例

};

// --- New Core Systems Components ---

// --- Animation State Machine ---
/**
 * @struct AnimationState
 * @brief 动画状态，包含一组序列帧和关键帧事件
 */
struct AnimationState {
    std::string name;                                    ///< 状态名称
    std::vector<std::shared_ptr<TextureAsset>> frames;   ///< 帧纹理列表
    std::vector<unsigned int> frame_handles;             ///< 帧句柄列表 (用于 Lua 绑定优化)
    std::vector<std::pair<float, std::string>> events;   ///< 时间点触发的事件列表
    std::vector<std::pair<int, int>> segments;           ///< 动画片段区间
    float frame_rate = 10.0f;                            ///< 播放帧率
    bool loop = true;                                    ///< 是否循环播放
};

/**
 * @struct AnimationTransition
 * @brief 动画状态转换条件
 */
struct AnimationTransition {
    std::string to_state;                                ///< 目标状态名称
    std::string condition_param;                         ///< 条件参数名 (如 "is_walking")
    bool condition_value;                                ///< 触发转换所需的条件值
};

/**
 * @struct AnimatorComponent
 * @brief 动画状态机组件，控制多状态的帧动画播放与切换
 */
struct AnimatorComponent {
    std::unordered_map<std::string, AnimationState> states;              ///< 所有可用状态
    std::unordered_map<std::string, std::vector<AnimationTransition>> transitions; ///< 状态间转换规则
    std::unordered_map<std::string, bool> bool_params;                   ///< 布尔型控制参数
    std::unordered_map<std::string, float> float_params;                 ///< 浮点型控制参数

    std::string current_state = "";                      ///< 当前处于的状态
    float current_time = 0.0f;                           ///< 当前状态已播放的时间
    int current_frame = 0;                               ///< 当前显示的帧索引
    int segment_start_frame = 0;                         ///< 分段播放的起始帧
    int segment_end_frame = -1;                          ///< 分段播放的结束帧
    bool segment_loop = true;                            ///< 分段是否循环
    bool playing = true;                                 ///< 是否正在播放
    std::vector<std::string> fired_events;               ///< 当前帧触发的事件列表
    
    // Add helper to set params
    /**
     * @brief 设置布尔参数以驱动状态机
     */
    void SetBool(const std::string& name, bool value) { bool_params[name] = value; }
    /**
     * @brief 设置浮点参数以驱动状态机
     */
    void SetFloat(const std::string& name, float value) { float_params[name] = value; }
    /**
     * @brief 指定播放当前状态的某个分段
     */
    void PlaySegment(int start_frame, int end_frame, bool loop_segment) {
        segment_start_frame = start_frame < 0 ? 0 : start_frame;
        segment_end_frame = end_frame;
        segment_loop = loop_segment;
        current_time = 0.0f;
        current_frame = segment_start_frame;
        playing = true;
    }
};

/**
 * @struct Particle2D
 * @brief 单个粒子的运行时数据结构
 */
struct Particle2D {
    glm::vec3 position;         ///< 粒子当前的世界坐标
    glm::vec3 velocity;         ///< 粒子的运动速度
    glm::vec4 color;            ///< 粒子的当前颜色
    float life_time;            ///< 粒子的总生命周期(秒)
    float life_remaining;       ///< 粒子的剩余生命周期(秒)
    float size;                 ///< 粒子的当前尺寸
    float rotation = 0.0f;      ///< 粒子旋转角度(弧度)
    float angular_velocity = 0.0f; ///< 角速度(弧度/秒)
};

/**
 * @enum ParticleCurveType
 * @brief 粒子曲线类型，用于驱动生命周期参数插值
 */
enum class ParticleCurveType {
    Linear,     ///< 线性插值
    EaseIn,     ///< 缓入
    EaseOut,    ///< 缓出
    EaseInOut   ///< 缓入缓出
};

/**
 * @struct ParticleCurve
 * @brief 标量粒子曲线，适用于尺寸、透明度、速度缩放等生命周期参数
 */
struct ParticleCurve {
    bool enabled = false;                               ///< 是否启用曲线
    ParticleCurveType type = ParticleCurveType::Linear; ///< 曲线类型
    float start_value = 1.0f;                           ///< 生命周期起始值
    float end_value = 0.0f;                             ///< 生命周期结束值

    float Evaluate(float t) const {
        t = std::clamp(t, 0.0f, 1.0f);
        float shaped_t = t;
        switch (type) {
        case ParticleCurveType::Linear:
            shaped_t = t;
            break;
        case ParticleCurveType::EaseIn:
            shaped_t = t * t;
            break;
        case ParticleCurveType::EaseOut:
            shaped_t = 1.0f - (1.0f - t) * (1.0f - t);
            break;
        case ParticleCurveType::EaseInOut:
            if (t < 0.5f) {
                shaped_t = 2.0f * t * t;
            } else {
                const float k = -2.0f * t + 2.0f;
                shaped_t = 1.0f - (k * k) * 0.5f;
            }
            break;
        }
        return glm::mix(start_value, end_value, shaped_t);
    }
};

/**
 * @enum ParticleCollisionMode
 * @brief 粒子碰撞模式，便于从简易地面碰撞逐步扩展到更正式的碰撞系统
 */
enum class ParticleCollisionMode {
    None,        ///< 不启用碰撞
    GroundPlane, ///< 与简易地面平面碰撞
    Box2D        ///< 预留给 Box2D 集成
};

/**
 * @struct ParticleEmitterComponent
 * @brief 粒子发射器组件，控制粒子的生成规则和渲染材质
 */
struct ParticleEmitterComponent {
    std::vector<Particle2D> particles;                   ///< 活跃的粒子池
    std::shared_ptr<TextureAsset> texture;               ///< 粒子的贴图资产
    unsigned int texture_handle = 0;                     ///< 粒子的渲染纹理句柄
    int max_particles = 100;                             ///< 允许的最大粒子数量
    float emit_rate = 10.0f;                             ///< 每秒发射的粒子数
    float emit_rate_scale = 1.0f;                        ///< 发射率缩放倍数
    float emit_accumulator = 0.0f;                       ///< 发射计时累加器
    bool emitting = true;                                ///< 是否持续发射中
    int pending_burst = 0;                               ///< 等待爆发(一次性生成)的粒子数
    
    // Emission parameters
    float start_life_time = 2.0f;                        ///< 新粒子的初始生命周期
    float start_size = 1.0f;                             ///< 新粒子的初始尺寸
    glm::vec4 start_color = glm::vec4(1.0f);             ///< 新粒子的初始颜色

    // --- Advanced: Randomization ---
    glm::vec3 velocity_min = glm::vec3(-1.0f, 0.5f, 0.0f);  ///< 随机速度下限
    glm::vec3 velocity_max = glm::vec3(1.0f, 2.0f, 0.0f);   ///< 随机速度上限
    float life_time_min = 1.0f;                              ///< 随机生命周期下限
    float life_time_max = 3.0f;                              ///< 随机生命周期上限
    float size_min = 0.5f;                                   ///< 随机尺寸下限
    float size_max = 1.5f;                                   ///< 随机尺寸上限
    float rotation_min = 0.0f;                               ///< 随机初始旋转下限(弧度)
    float rotation_max = 6.2832f;                            ///< 随机初始旋转上限(弧度)
    float angular_velocity_min = -1.0f;                      ///< 随机角速度下限
    float angular_velocity_max = 1.0f;                       ///< 随机角速度上限
    bool use_random_params = false;                          ///< 是否启用随机参数

    // --- Advanced: Lifetime Curves ---
    bool use_size_curve = false;                             ///< 是否启用尺寸曲线（兼容旧字段）
    float size_curve_end = 0.0f;                             ///< 生命末期尺寸（兼容旧字段）
    bool use_alpha_curve = false;                            ///< 是否启用透明度曲线（兼容旧字段）
    float alpha_curve_end = 0.0f;                            ///< 生命末期透明度（兼容旧字段）
    bool use_color_curve = false;                            ///< 是否启用颜色曲线
    glm::vec4 color_curve_end = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); ///< 生命末期颜色
    bool use_speed_curve = false;                            ///< 是否启用速度曲线（兼容旧字段）
    float speed_curve_end_scale = 0.0f;                      ///< 生命末期速度缩放（兼容旧字段）
    ParticleCurve size_curve = {false, ParticleCurveType::Linear, 1.0f, 0.0f};   ///< 正式尺寸曲线
    ParticleCurve alpha_curve = {false, ParticleCurveType::Linear, 1.0f, 0.0f};  ///< 正式透明度曲线
    ParticleCurve speed_curve = {false, ParticleCurveType::Linear, 1.0f, 0.0f};  ///< 正式速度曲线

    // --- Advanced: Gravity & Collision ---
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, 0.0f);        ///< 粒子重力加速度
    bool enable_collision = false;                           ///< 是否启用碰撞检测
    ParticleCollisionMode collision_mode = ParticleCollisionMode::None; ///< 碰撞模式
    float collision_bounce = 0.5f;                           ///< 碰撞反弹系数
    float collision_friction = 0.1f;                         ///< 碰撞摩擦系数
    float collision_life_loss = 0.0f;                        ///< 碰撞时生命损失
    float ground_y = 0.0f;                                   ///< 简易地面碰撞Y坐标
    bool use_ground_collision = false;                       ///< 是否启用简易地面碰撞
};

class AudioClipAsset;

/**
 * @struct AudioSourceComponent
 * @brief 音频源组件，挂载在实体上用于播放 3D/2D 空间音效
 */
struct AudioSourceComponent {
    std::shared_ptr<AudioClipAsset> clip;                ///< 引用的音频片段资产
    bool play_on_awake = true;                           ///< 是否在组件创建时自动播放
    bool loop = false;                                   ///< 是否循环播放
    float volume = 1.0f;                                 ///< 音量大小 (0.0 - 1.0)
    float pitch = 1.0f;                                  ///< 音高倍数 (1.0 为原始音高)
    bool is_playing = false;                             ///< 当前是否正在播放
    bool restart_requested = false;                      ///< 是否请求重新开始播放
    
    // Internal handle to audio engine (e.g., miniaudio)
    unsigned int runtime_handle = 0;                     ///< 引擎底层的音频句柄
};

/**
 * @struct GameplayTuningComponent
 * @brief 全局/关卡级别的玩法微调参数组件
 */
struct GameplayTuningComponent {
    float leaf_min_distance = 80.0f;                     ///< 树叶的最小判定距离
    float leaf_move_left = 140.0f;                       ///< 树叶向左移动的阈值
    float leaf_move_right = 410.0f;                      ///< 树叶向右移动的阈值
    float jump_speed_scale = 15.0f;                      ///< 跳跃速度的缩放系数
    float jump_speed_max = 18.0f;                        ///< 允许的最大跳跃速度
    float camera_follow_damping = 0.02f;                 ///< 摄像机跟随的默认阻尼
};

/**
 * @struct TilemapComponent
 * @brief 瓦片地图组件，管理网格地图数据和渲染图集
 */
struct TilemapComponent {
    std::vector<int> tiles;                              ///< 一维数组存储的瓦片 ID (0 为空)
    int width = 0;                                       ///< 地图的列数
    int height = 0;                                      ///< 地图的行数
    float tile_size = 1.0f;                              ///< 单个瓦片的物理/渲染尺寸
    std::shared_ptr<TextureAsset> tileset_texture;       ///< 引用的瓦片图集纹理
    unsigned int tileset_handle = 0;                     ///< 图集的 RHI 渲染句柄
    int tileset_cols = 1;
    int tileset_rows = 1;
    int sorting_layer = 0;
    int order_in_layer_base = 0;
    bool generate_colliders = false;
    int collider_tile_min = 1;
    bool dirty = true;
    std::vector<Entity> runtime_tile_entities;
};

// --- Lua Scripting Component ---

struct LuaScriptComponent {
    std::string script_path;
    bool is_initialized = false;
    
    // Sol2 table instance representing the script environment for this entity
    // We use a void pointer or forward declaration here if sol::table is not included
    // to avoid polluting the ECS header with Lua/Sol2 dependencies.
    void* script_instance = nullptr; 
};

/**
 * @struct UIMaskComponent
 * @brief UI 遮罩组件，用于裁剪超出指定区域的子 UI 元素并拦截输入
 */
struct UIMaskComponent {
    bool enabled = true;                     ///< 是否启用遮罩功能
    glm::vec2 size = glm::vec2(0.0f);        ///< 遮罩的绝对尺寸（为 0 时尝试继承渲染尺寸）
    glm::vec2 offset = glm::vec2(0.0f);      ///< 相对当前实体位置的中心偏移
    bool block_outside_input = true;         ///< 是否拦截遮罩区域外的所有 UI 输入事件
};

/**
 * @struct UIRichTextComponent
 * @brief UI 富文本组件，支持颜色标签、阴影和描边等复杂文本渲染
 */
struct UIRichTextComponent {
    std::string text;                                            ///< 包含颜色标签（如 <color=#ff0000>）的富文本字符串
    glm::vec4 default_color = glm::vec4(1.0f);                   ///< 文本的默认颜色
    bool enable_shadow = false;                                  ///< 是否启用文本阴影效果
    glm::vec2 shadow_offset = glm::vec2(1.0f, -1.0f);            ///< 阴影在 X/Y 轴上的像素偏移
    glm::vec4 shadow_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.75f); ///< 阴影的颜色及透明度
    bool enable_outline = false;                                 ///< 是否启用文本描边效果
    glm::vec4 outline_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); ///< 描边的颜色及透明度
    float outline_width = 1.0f;                                  ///< 描边的宽度（像素）
    bool dirty = true;                                           ///< 标记文本或样式发生变化，需要重新生成渲染实体
};

/**
 * @struct UIJoystickComponent
 * @brief UI 虚拟摇杆组件，处理玩家屏幕拖拽输入并输出二维方向向量
 */
struct UIJoystickComponent {
    glm::vec2 direction = glm::vec2(0.0f);   ///< 当前摇杆的标准化方向向量（输出值）
    float max_radius = 64.0f;                ///< 摇杆可拖拽的最大可视半径
    bool follow_pointer = true;              ///< 拖拽起始点是否跟随鼠标/触摸点
    bool reset_on_release = true;            ///< 松开后是否自动将方向重置为 (0,0)
    bool is_dragging = false;                ///< 当前是否正处于拖拽状态中
    glm::vec2 drag_anchor = glm::vec2(0.0f); ///< 记录本次拖拽的起始锚点屏幕坐标
};

// --- Advanced UI Layout Components ---

/**
 * @struct UIAnchorComponent
 * @brief UI 锚点组件，定义元素相对于屏幕/父容器的定位方式
 */
struct UIAnchorComponent {
    int anchor = 5;                          ///< 锚点类型 (对应 UIAnchor 枚举: 0=TopLeft...9=Stretch)
    glm::vec2 offset = glm::vec2(0.0f);      ///< 相对锚点的偏移
};

/**
 * @struct UIGridLayoutComponent
 * @brief UI 网格布局组件，自动排列子元素
 */
struct UIGridLayoutComponent {
    int columns = 1;                           ///< 列数
    int rows = 0;                              ///< 行数（0 表示自动）
    glm::vec2 cell_size = glm::vec2(100.0f);   ///< 单元格大小
    glm::vec2 spacing = glm::vec2(10.0f);      ///< 单元格间距
    int alignment = 0;                         ///< 对齐方式 (对应 GridLayoutAlignment 枚举)
};

/**
 * @struct UICanvasScalerComponent
 * @brief Canvas 缩放组件，处理多分辨率自适应
 */
struct UICanvasScalerComponent {
    glm::vec2 reference_resolution = glm::vec2(1920.0f, 1080.0f);  ///< 参考分辨率
    float scale_factor = 1.0f;                                      ///< 缩放因子
    bool match_width_or_height = true;                              ///< true=宽高平均, false=仅宽度
};

/**
 * @struct UIAnimationComponent
 * @brief UI 动画组件，基于 Tween 驱动位置、缩放、透明度、颜色动画
 */
struct UIAnimationComponent {
    // Target properties
    glm::vec2 target_position = glm::vec2(0.0f);
    glm::vec2 target_scale = glm::vec2(1.0f);
    float target_alpha = 1.0f;
    glm::vec4 target_color = glm::vec4(1.0f);

    // Current animation state
    bool animate_position = false;
    bool animate_scale = false;
    bool animate_alpha = false;
    bool animate_color = false;

    // Animation parameters
    float duration = 0.3f;          ///< 动画持续时间（秒）
    float elapsed = 0.0f;           ///< 已经过的时间
    float delay = 0.0f;             ///< 延迟开始时间
    float delay_remaining = 0.0f;   ///< 剩余延迟时间
    bool loop = false;              ///< 是否循环
    bool ping_pong = false;         ///< 是否来回播放
    bool playing = false;           ///< 是否正在播放
    bool reverse = false;           ///< 当前是否在反向播放（ping_pong 用）

    // Easing (0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out)
    int easing = 0;

    // Snapshot of start values (set when animation begins)
    glm::vec2 start_position = glm::vec2(0.0f);
    glm::vec2 start_scale = glm::vec2(1.0f);
    float start_alpha = 1.0f;
    glm::vec4 start_color = glm::vec4(1.0f);
};

#endif
