/**
 * @file ui.h
 * @brief UI 渲染、交互与布局组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_UI_H
#define DSE_ECS_COMPONENTS_2D_UI_H

#include <glm/glm.hpp>
#include <cmath>
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

// ============================================================
// P0: 文本输入框
// ============================================================

/**
 * @struct UITextInputComponent
 * @brief 文本输入框组件，支持光标、选区、占位符与 IME 输入
 */
struct UITextInputComponent {
    std::string text;                                    ///< 当前输入的文本
    std::string placeholder;                             ///< 占位提示文本
    int cursor_position = 0;                             ///< 光标位置（字符索引）
    int selection_start = -1;                            ///< 选区起始索引（-1 表示无选区）
    int selection_end = -1;                              ///< 选区结束索引
    int max_length = 0;                                  ///< 最大字符数（0 表示不限制）
    bool is_focused = false;                             ///< 是否获得焦点
    bool is_password = false;                            ///< 是否密码模式（显示为 '*'）
    bool multiline = false;                              ///< 是否支持多行输入
    bool read_only = false;                              ///< 是否只读
    bool submit_on_enter = true;                         ///< 按回车是否触发提交

    glm::vec4 text_color = glm::vec4(1.0f);              ///< 文本颜色
    glm::vec4 placeholder_color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f); ///< 占位符颜色
    glm::vec4 cursor_color = glm::vec4(1.0f);            ///< 光标颜色
    glm::vec4 selection_color = glm::vec4(0.3f, 0.5f, 0.8f, 0.5f);   ///< 选区高亮颜色
    float cursor_blink_rate = 0.53f;                     ///< 光标闪烁间隔（秒）

    unsigned int font_texture_handle = 0;                ///< 字体图集句柄

    // Runtime state
    float cursor_blink_timer = 0.0f;                     ///< 闪烁计时器
    bool cursor_visible = true;                          ///< 当前光标是否可见
    float scroll_offset_x = 0.0f;                        ///< 单行模式下的水平滚动偏移

    // Callbacks
    std::function<void(Entity, const std::string&)> on_value_changed;  ///< 文本变更回调
    std::function<void(Entity, const std::string&)> on_submit;         ///< 提交回调（Enter）
    std::function<void(Entity)> on_focus;                ///< 获得焦点回调
    std::function<void(Entity)> on_blur;                 ///< 失去焦点回调
};

// ============================================================
// P1: 滚动视图
// ============================================================

/**
 * @struct UIScrollViewComponent
 * @brief 滚动容器组件，支持惯性滚动和滚动条
 */
struct UIScrollViewComponent {
    glm::vec2 content_size = glm::vec2(0.0f);            ///< 内容的实际大小
    glm::vec2 viewport_size = glm::vec2(0.0f);           ///< 可视区域大小（为 0 时自动从 UIRendererComponent::size 继承）
    glm::vec2 scroll_offset = glm::vec2(0.0f);           ///< 当前滚动偏移
    bool horizontal = false;                             ///< 是否允许水平滚动
    bool vertical = true;                                ///< 是否允许垂直滚动
    bool elastic = true;                                 ///< 是否启用弹性边缘效果
    float elasticity = 0.1f;                             ///< 弹性回弹系数
    bool inertia = true;                                 ///< 是否启用惯性滚动
    float deceleration_rate = 0.135f;                    ///< 惯性减速率
    bool show_scrollbar = true;                          ///< 是否显示滚动条
    float scrollbar_width = 6.0f;                        ///< 滚动条宽度（像素）
    glm::vec4 scrollbar_color = glm::vec4(0.5f, 0.5f, 0.5f, 0.6f); ///< 滚动条颜色

    // Runtime state
    glm::vec2 velocity = glm::vec2(0.0f);                ///< 当前惯性速度
    bool is_dragging = false;                            ///< 是否正在拖拽
    glm::vec2 drag_start_pos = glm::vec2(0.0f);          ///< 拖拽起始位置
    glm::vec2 drag_start_offset = glm::vec2(0.0f);       ///< 拖拽起始偏移

    /// 获取归一化滚动位置 [0, 1]
    glm::vec2 GetNormalizedPosition() const {
        glm::vec2 max_offset = glm::max(content_size - viewport_size, glm::vec2(0.0f));
        return glm::vec2(
            max_offset.x > 0.0f ? scroll_offset.x / max_offset.x : 0.0f,
            max_offset.y > 0.0f ? scroll_offset.y / max_offset.y : 0.0f
        );
    }
};

// ============================================================
// P2: 滑动条
// ============================================================

/**
 * @struct UISliderComponent
 * @brief 滑动条组件，水平或垂直方向的数值输入
 */
struct UISliderComponent {
    float value = 0.0f;                                  ///< 当前值
    float min_value = 0.0f;                              ///< 最小值
    float max_value = 1.0f;                              ///< 最大值
    bool whole_numbers = false;                          ///< 是否仅允许整数步进
    bool vertical = false;                               ///< 垂直模式
    glm::vec4 track_color = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);   ///< 轨道颜色
    glm::vec4 fill_color = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);    ///< 已填充区域颜色
    glm::vec4 handle_color = glm::vec4(1.0f);            ///< 滑块颜色
    float handle_size = 20.0f;                           ///< 滑块大小（像素）

    // Runtime
    bool is_dragging = false;                            ///< 是否正在拖拽滑块
    std::function<void(Entity, float)> on_value_changed; ///< 值变更回调

    /// 获取归一化值 [0, 1]
    float GetNormalizedValue() const {
        float range = max_value - min_value;
        return range > 0.0f ? (value - min_value) / range : 0.0f;
    }

    /// 从归一化值设置 value
    void SetFromNormalized(float t) {
        value = min_value + t * (max_value - min_value);
        if (whole_numbers) value = std::round(value);
    }
};

// ============================================================
// P2: 开关
// ============================================================

/**
 * @struct UIToggleComponent
 * @brief 开关（复选框）组件
 */
struct UIToggleComponent {
    bool is_on = false;                                  ///< 当前状态
    int group = -1;                                      ///< 互斥组 ID（-1 表示不属于任何组，>= 0 时同组只能有一个 on）
    glm::vec4 on_color = glm::vec4(0.2f, 0.7f, 0.3f, 1.0f);  ///< 开启颜色
    glm::vec4 off_color = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f); ///< 关闭颜色
    float transition_duration = 0.15f;                   ///< 切换动画时长

    // Runtime
    float transition_progress = 1.0f;                    ///< 动画进度 [0, 1]
    std::function<void(Entity, bool)> on_value_changed;  ///< 值变更回调
};

// ============================================================
// P2: 进度条
// ============================================================

/**
 * @struct UIProgressBarComponent
 * @brief 进度条组件
 */
struct UIProgressBarComponent {
    float value = 0.0f;                                  ///< 当前值 [0, 1]
    float max_value = 1.0f;                              ///< 最大值
    bool right_to_left = false;                          ///< 是否从右往左填充
    bool vertical = false;                               ///< 垂直进度条
    glm::vec4 background_color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f); ///< 背景颜色
    glm::vec4 fill_color = glm::vec4(0.2f, 0.7f, 0.3f, 1.0f);       ///< 填充颜色

    /// 获取填充比例 [0, 1]
    float GetFillAmount() const {
        return max_value > 0.0f ? glm::clamp(value / max_value, 0.0f, 1.0f) : 0.0f;
    }
};

#endif // DSE_ECS_COMPONENTS_2D_UI_H
