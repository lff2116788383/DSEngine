/**
 * @file splash_screen.h
 * @brief 启动期原生 splash 窗口 — 在重型初始化期间显示 logo + 加载状态，带淡入淡出。
 *
 * 设计目标（对齐商业引擎编辑器/运行时的启动体验）：
 * - 进程一启动就瞬间弹出（早于 RHI / 渲染管线就绪），盖住主窗口创建到首帧之间的空白期。
 * - 独立线程内自带消息泵与动画，主线程做初始化时 splash 仍能淡入并刷新状态文字。
 * - 后端无关、不触碰渲染管线：纯原生窗口实现，避免改动 OpenGL/Vulkan/D3D11 三后端。
 *
 * 平台支持：Windows 与 Linux/X11 提供原生实现；macOS/Android 为安全 no-op（Show 返回 false）。
 */

#ifndef DSE_PLATFORM_SPLASH_SCREEN_H
#define DSE_PLATFORM_SPLASH_SCREEN_H

#include <cstdint>
#include <memory>
#include <string>

namespace dse::platform {

/// Splash 外观与时序配置。
struct SplashConfig {
    std::string image_path;            ///< logo 图片路径（PNG，带 alpha 最佳）。空则只显示文字。
    std::string app_name = "DSEngine"; ///< 大标题。
    std::string initial_status;        ///< 初始状态行（如“正在启动…”）。
    int card_width = 460;              ///< splash 卡片宽（像素）。
    int card_height = 300;             ///< splash 卡片高（像素）。
    int logo_size = 128;              ///< logo 绘制边长（像素，等比缩放到此正方形内）。
    uint32_t bg_argb = 0xFF1E1E28u;    ///< 卡片背景色 ARGB。
    uint32_t title_argb = 0xFFF2F2F5u; ///< 标题文字色 ARGB。
    uint32_t status_argb = 0xFF9AA0B4u;///< 状态文字色 ARGB。
    uint32_t accent_argb = 0xFF4A9EFFu;///< 强调色（进度条/分隔线）ARGB。
    int fade_in_ms = 220;              ///< 淡入时长。
    int fade_out_ms = 220;             ///< 淡出时长。
    int min_display_ms = 600;          ///< 最短可见时长（避免一闪而过）。
    bool enabled = true;               ///< 关闭后 Show 直接返回 false。
};

/// 应用环境变量覆盖（便于调试/品牌定制）：
///   DSE_SPLASH_MIN_MS / DSE_SPLASH_FADE_IN_MS / DSE_SPLASH_FADE_OUT_MS。
void ApplySplashEnvOverrides(SplashConfig& config);

/**
 * @class SplashScreen
 * @brief 非阻塞的原生 splash 窗口句柄。Show 启动后台线程，Finish 淡出并回收。
 *
 * 典型用法：
 * @code
 *   SplashScreen splash;
 *   splash.Show(cfg);                    // 立即返回，后台线程显示并淡入
 *   splash.SetStatus("正在初始化渲染设备…");
 *   ... 重型初始化 ...
 *   splash.SetStatus("正在加载资源…");
 *   ... 首帧渲染并显示主窗口 ...
 *   splash.Finish();                     // 满足最短时长后淡出，join 后台线程
 * @endcode
 */
class SplashScreen {
public:
    SplashScreen();
    ~SplashScreen();

    SplashScreen(const SplashScreen&) = delete;
    SplashScreen& operator=(const SplashScreen&) = delete;

    /// 启动 splash（非阻塞）。返回 false 表示未显示（被禁用 / 平台不支持 / 资源加载失败）。
    bool Show(const SplashConfig& config);

    /// 更新状态文字（线程安全）。
    void SetStatus(const std::string& status);

    /// 用户是否请求跳过（点击 splash 或按 Esc）。
    bool SkipRequested() const;

    /// 是否仍在显示。
    bool IsActive() const;

    /// 请求结束：满足最短显示时长（或已被跳过）后淡出，并阻塞直至后台线程退出。
    void Finish();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dse::platform

#endif // DSE_PLATFORM_SPLASH_SCREEN_H
