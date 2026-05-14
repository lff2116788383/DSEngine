/**
 * @file ui.h
 * @brief UI 渲染、交互与布局组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_UI_H
#define DSE_ECS_COMPONENTS_2D_UI_H

#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <entt/entt.hpp>

class TextureAsset;
using Entity = entt::entity;

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
    
    // Nine-slice / 9 宫格
    bool nine_slice_enabled = false;                     ///< 启用 9 宫格拉伸模式
    glm::vec4 nine_slice_border = glm::vec4(0.0f);       ///< 9 宫格边框 UV 分量 (left, bottom, right, top)，[0, 0.5]
    glm::vec2 nine_slice_src_size = glm::vec2(0.0f);     ///< 源精灵像素尺寸 (src_w, src_h)；> 0 时角块屏幕尺寸固定为 border × src_size
                                                         ///< = (0,0) 时退为等比模式：角块随 widget 缩放（适用于均匀缩放的按钮/图标）

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

#endif // DSE_ECS_COMPONENTS_2D_UI_H
