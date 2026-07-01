/**
 * @file splash_screen.cpp
 * @brief SplashScreen 实现。Windows 走原生分层窗口；其它平台为 no-op。
 */

#include "engine/platform/splash_screen.h"

#include <cstdlib>
#include <string>

namespace dse::platform {

void ApplySplashEnvOverrides(SplashConfig& config) {
    auto read_int = [](const char* name, int& out) {
        if (const char* v = std::getenv(name)) {
            try {
                const int parsed = std::stoi(v);
                if (parsed >= 0) out = parsed;
            } catch (...) {
                // 忽略非法值
            }
        }
    };
    read_int("DSE_SPLASH_MIN_MS", config.min_display_ms);
    read_int("DSE_SPLASH_FADE_IN_MS", config.fade_in_ms);
    read_int("DSE_SPLASH_FADE_OUT_MS", config.fade_out_ms);
}

} // namespace dse::platform

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#pragma comment(lib, "msimg32.lib")

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <stb/stb_image.h>

namespace dse::platform {
namespace {

constexpr wchar_t kSplashClassName[] = L"DSEngineSplashWindow";
std::once_flag g_class_once;

std::wstring Widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

COLORREF ToColorRef(uint32_t argb) {
    return RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}

} // namespace

struct SplashScreen::Impl {
    SplashConfig config;

    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> finish_requested{false};
    std::atomic<bool> skip_requested{false};

    std::mutex status_mutex;
    std::string status;

    // 预乘 alpha 的 BGRA logo 像素（线程内创建/使用，无需加锁）。
    std::vector<uint8_t> logo_bgra;
    int logo_w = 0;
    int logo_h = 0;

    // 绘制状态（仅 splash 线程访问）。
    HWND hwnd = nullptr;
    HFONT title_font = nullptr;
    HFONT status_font = nullptr;
    uint64_t show_tick = 0;
    uint64_t fade_out_tick = 0;
    int phase = 0; // 0=FadeIn 1=Shown 2=FadeOut
    std::string last_painted_status;

    void LoadLogo();
    void ThreadMain();
    void Paint(HDC hdc, const RECT& client);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

void SplashScreen::Impl::LoadLogo() {
    if (config.image_path.empty()) return;
    int w = 0, h = 0, ch = 0;
    unsigned char* px = stbi_load(config.image_path.c_str(), &w, &h, &ch, 4);
    if (!px) return;
    logo_w = w;
    logo_h = h;
    logo_bgra.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    // RGBA -> 预乘 BGRA（AlphaBlend AC_SRC_ALPHA 需要预乘源）。
    for (size_t i = 0; i < static_cast<size_t>(w) * static_cast<size_t>(h); ++i) {
        const uint8_t r = px[i * 4 + 0];
        const uint8_t g = px[i * 4 + 1];
        const uint8_t b = px[i * 4 + 2];
        const uint8_t a = px[i * 4 + 3];
        logo_bgra[i * 4 + 0] = static_cast<uint8_t>(b * a / 255);
        logo_bgra[i * 4 + 1] = static_cast<uint8_t>(g * a / 255);
        logo_bgra[i * 4 + 2] = static_cast<uint8_t>(r * a / 255);
        logo_bgra[i * 4 + 3] = a;
    }
    stbi_image_free(px);
}

void SplashScreen::Impl::Paint(HDC hdc, const RECT& client) {
    const int cw = client.right - client.left;
    const int ch = client.bottom - client.top;

    // 双缓冲，避免状态文字刷新时闪烁。
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP old_bmp = static_cast<HBITMAP>(SelectObject(mem, bmp));

    // 背景。
    HBRUSH bg = CreateSolidBrush(ToColorRef(config.bg_argb));
    RECT full{0, 0, cw, ch};
    FillRect(mem, &full, bg);
    DeleteObject(bg);

    // 底部强调色细线（贯穿整宽，作为静态“加载中”指示）。
    HBRUSH accent = CreateSolidBrush(ToColorRef(config.accent_argb));
    RECT line{0, ch - 3, cw, ch};
    FillRect(mem, &line, accent);
    DeleteObject(accent);

    // logo（等比缩放到 logo_size 方框内，居中偏上）。
    int logo_box = std::min(config.logo_size, std::min(cw, ch));
    int logo_top = static_cast<int>(ch * 0.16);
    if (!logo_bgra.empty() && logo_w > 0 && logo_h > 0) {
        double scale = static_cast<double>(logo_box) / std::max(logo_w, logo_h);
        int dw = std::max(1, static_cast<int>(logo_w * scale));
        int dh = std::max(1, static_cast<int>(logo_h * scale));
        int dx = (cw - dw) / 2;
        int dy = logo_top;

        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = logo_w;
        bi.bmiHeader.biHeight = -logo_h; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        void* dib_bits = nullptr;
        HDC logo_dc = CreateCompatibleDC(hdc);
        HBITMAP logo_bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &dib_bits, nullptr, 0);
        if (logo_bmp && dib_bits) {
            std::memcpy(dib_bits, logo_bgra.data(), logo_bgra.size());
            HBITMAP logo_old = static_cast<HBITMAP>(SelectObject(logo_dc, logo_bmp));
            BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            AlphaBlend(mem, dx, dy, dw, dh, logo_dc, 0, 0, logo_w, logo_h, blend);
            SelectObject(logo_dc, logo_old);
        }
        if (logo_bmp) DeleteObject(logo_bmp);
        DeleteDC(logo_dc);
        logo_top = dy + dh;
    }

    SetBkMode(mem, TRANSPARENT);

    // 标题。
    {
        const std::wstring title = Widen(config.app_name);
        HFONT old = static_cast<HFONT>(SelectObject(mem, title_font));
        SetTextColor(mem, ToColorRef(config.title_argb));
        RECT tr{0, logo_top + 16, cw, logo_top + 16 + 44};
        DrawTextW(mem, title.c_str(), static_cast<int>(title.size()), &tr,
                  DT_CENTER | DT_SINGLELINE | DT_TOP);
        SelectObject(mem, old);
    }

    // 状态行。
    {
        std::string status_copy;
        {
            std::lock_guard<std::mutex> lk(status_mutex);
            status_copy = status;
        }
        last_painted_status = status_copy;
        const std::wstring status_w = Widen(status_copy);
        HFONT old = static_cast<HFONT>(SelectObject(mem, status_font));
        SetTextColor(mem, ToColorRef(config.status_argb));
        RECT sr{16, ch - 40, cw - 16, ch - 12};
        DrawTextW(mem, status_w.c_str(), static_cast<int>(status_w.size()), &sr,
                  DT_CENTER | DT_SINGLELINE | DT_BOTTOM | DT_END_ELLIPSIS);
        SelectObject(mem, old);
    }

    BitBlt(hdc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);

    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

LRESULT CALLBACK SplashScreen::Impl::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wparam, lparam);

    switch (msg) {
        case WM_TIMER: {
            const uint64_t now = NowMs();
            const uint64_t elapsed = now - self->show_tick;
            const int fade_in = std::max(1, self->config.fade_in_ms);
            const int fade_out = std::max(1, self->config.fade_out_ms);

            const bool can_close =
                self->finish_requested.load() &&
                (elapsed >= static_cast<uint64_t>(self->config.min_display_ms) ||
                 self->skip_requested.load());
            if (self->phase != 2 && can_close) {
                self->phase = 2;
                self->fade_out_tick = now;
            }

            int alpha = 255;
            if (self->phase == 0) {
                alpha = static_cast<int>(255 * elapsed / fade_in);
                if (elapsed >= static_cast<uint64_t>(fade_in)) {
                    alpha = 255;
                    self->phase = 1;
                }
            } else if (self->phase == 2) {
                const uint64_t fo = now - self->fade_out_tick;
                alpha = static_cast<int>(255 - 255 * fo / fade_out);
                if (fo >= static_cast<uint64_t>(fade_out)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            alpha = std::clamp(alpha, 0, 255);
            SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(alpha), LWA_ALPHA);

            // 状态文字变化时才重绘。
            std::string cur;
            {
                std::lock_guard<std::mutex> lk(self->status_mutex);
                cur = self->status;
            }
            if (cur != self->last_painted_status) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client;
            GetClientRect(hwnd, &client);
            self->Paint(hdc, client);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            self->skip_requested.store(true);
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) self->skip_requested.store(true);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void SplashScreen::Impl::ThreadMain() {
    LoadLogo();

    std::call_once(g_class_once, []() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &SplashScreen::Impl::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kSplashClassName;
        RegisterClassExW(&wc);
    });

    const int cw = std::max(120, config.card_width);
    const int chh = std::max(120, config.card_height);
    const int sx = (GetSystemMetrics(SM_CXSCREEN) - cw) / 2;
    const int sy = (GetSystemMetrics(SM_CYSCREEN) - chh) / 2;

    title_font = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             L"Microsoft YaHei UI");
    status_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Microsoft YaHei UI");

    hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kSplashClassName, L"DSEngine", WS_POPUP,
        sx, sy, cw, chh, nullptr, nullptr, GetModuleHandleW(nullptr), this);

    if (std::getenv("DSE_SPLASH_DEBUG")) {
        fprintf(stderr, "[splash] logo=%dx%d hwnd=%p err=%lu pos=%d,%d size=%dx%d img='%s'\n",
                logo_w, logo_h, (void*)hwnd, GetLastError(), sx, sy, cw, chh, config.image_path.c_str());
        fflush(stderr);
    }

    if (!hwnd) {
        if (title_font) DeleteObject(title_font);
        if (status_font) DeleteObject(status_font);
        running.store(false);
        return;
    }

    show_tick = NowMs();
    phase = 0;
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetTimer(hwnd, 1, 15, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (title_font) DeleteObject(title_font);
    if (status_font) DeleteObject(status_font);
    title_font = nullptr;
    status_font = nullptr;
    hwnd = nullptr;
    running.store(false);
}

SplashScreen::SplashScreen() : impl_(std::make_unique<Impl>()) {}

SplashScreen::~SplashScreen() {
    if (impl_ && impl_->running.load()) {
        impl_->skip_requested.store(true);
        impl_->finish_requested.store(true);
        if (impl_->thread.joinable()) impl_->thread.join();
    }
}

bool SplashScreen::Show(const SplashConfig& config) {
    if (!config.enabled) return false;
    if (impl_->running.load()) return false;
    impl_->config = config;
    impl_->status = config.initial_status;
    impl_->running.store(true);
    impl_->thread = std::thread([this]() { impl_->ThreadMain(); });
    return true;
}

void SplashScreen::SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lk(impl_->status_mutex);
    impl_->status = status;
}

bool SplashScreen::SkipRequested() const {
    return impl_->skip_requested.load();
}

bool SplashScreen::IsActive() const {
    return impl_->running.load();
}

void SplashScreen::Finish() {
    impl_->finish_requested.store(true);
    if (impl_->thread.joinable()) impl_->thread.join();
}

} // namespace dse::platform

#elif defined(__linux__) && !defined(__ANDROID__)

// =============================================================================
// Linux / X11 原生实现（对齐 Windows 语义）：
// 独立线程自带事件循环；override-redirect 无边框居中窗口 + _NET_WM_WINDOW_TYPE_SPLASH；
// 双缓冲 Pixmap 绘制 背景 / 底部 accent 线 / logo / 标题 / 状态文字；
// logo 经 stb 加载 RGBA，按窗口 visual 掩码预乘到背景后用 XPutPixel 写入 XImage（避免字节序坑）；
// 文字走 XCreateFontSet + Xutf8DrawString（支持中文）；淡入淡出用 _NET_WM_WINDOW_OPACITY；
// ButtonPress / Esc = skip；无 DISPLAY 时 Show 直接返回 false（优雅降级）。
// =============================================================================

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <stb/stb_image.h>

namespace dse::platform {
namespace {

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 按 TrueColor/DirectColor visual 的通道掩码把 8bit r/g/b 打包成像素值。
unsigned long PackChannel(unsigned long mask, uint8_t value) {
    if (mask == 0) return 0;
    int shift = 0;
    unsigned long m = mask;
    while ((m & 1u) == 0u) {
        m >>= 1;
        ++shift;
    }
    const unsigned long max_value = m; // 连续 1 的掩码右移后即最大值
    const unsigned long scaled = static_cast<unsigned long>(value) * max_value / 255u;
    return (scaled << shift) & mask;
}

unsigned long PackColor(Visual* visual, uint32_t argb) {
    const uint8_t r = (argb >> 16) & 0xFF;
    const uint8_t g = (argb >> 8) & 0xFF;
    const uint8_t b = argb & 0xFF;
    return PackChannel(visual->red_mask, r) |
           PackChannel(visual->green_mask, g) |
           PackChannel(visual->blue_mask, b);
}

} // namespace

struct SplashScreen::Impl {
    SplashConfig config;

    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> finish_requested{false};
    std::atomic<bool> skip_requested{false};

    std::mutex status_mutex;
    std::string status;

    // 原始 RGBA logo（线程内加载/使用）。
    std::vector<uint8_t> logo_rgba;
    int logo_w = 0;
    int logo_h = 0;

    // X11 资源（仅 splash 线程访问）。
    Display* dpy = nullptr;
    int screen = 0;
    Window window = 0;
    Visual* visual = nullptr;
    int depth = 0;
    Colormap colormap = 0;
    Pixmap back = 0;          // 双缓冲后台 Pixmap
    GC gc = nullptr;
    XFontSet title_fs = nullptr;
    XFontSet status_fs = nullptr;
    XImage* logo_img = nullptr; // 预乘到背景、按 visual 打包好的 logo（已缩放）
    int logo_dx = 0, logo_dy = 0, logo_dw = 0, logo_dh = 0;
    int logo_bottom = 0;

    uint64_t show_tick = 0;
    uint64_t fade_out_tick = 0;
    int phase = 0; // 0=FadeIn 1=Shown 2=FadeOut
    int last_alpha = -1;
    std::string last_painted_status;

    void LoadLogo();
    void BuildLogoImage();
    XFontSet MakeFontSet(int pixel_size);
    void SetOpacity(int alpha);
    void Repaint();
    void ThreadMain();
};

void SplashScreen::Impl::LoadLogo() {
    if (config.image_path.empty()) return;
    int w = 0, h = 0, ch = 0;
    unsigned char* px = stbi_load(config.image_path.c_str(), &w, &h, &ch, 4);
    if (!px) return;
    logo_w = w;
    logo_h = h;
    logo_rgba.assign(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    stbi_image_free(px);
}

void SplashScreen::Impl::BuildLogoImage() {
    if (logo_rgba.empty() || logo_w <= 0 || logo_h <= 0) return;
    // 仅支持 TrueColor / DirectColor（现代桌面默认）。
    if (visual->c_class != TrueColor && visual->c_class != DirectColor) return;

    const int cw = config.card_width;
    const int chh = config.card_height;
    const int logo_box = std::min(config.logo_size, std::min(cw, chh));
    const double scale = static_cast<double>(logo_box) / std::max(logo_w, logo_h);
    const int dw = std::max(1, static_cast<int>(logo_w * scale));
    const int dh = std::max(1, static_cast<int>(logo_h * scale));

    XImage* img = XCreateImage(dpy, visual, static_cast<unsigned int>(depth), ZPixmap,
                               0, nullptr, static_cast<unsigned int>(dw),
                               static_cast<unsigned int>(dh), 32, 0);
    if (!img) return;
    img->data = static_cast<char*>(std::malloc(static_cast<size_t>(img->bytes_per_line) * dh));
    if (!img->data) {
        XDestroyImage(img);
        return;
    }

    // 背景色（用于把半透明 logo 预乘合成到不透明背景上）。
    const uint8_t bg_r = (config.bg_argb >> 16) & 0xFF;
    const uint8_t bg_g = (config.bg_argb >> 8) & 0xFF;
    const uint8_t bg_b = config.bg_argb & 0xFF;

    for (int y = 0; y < dh; ++y) {
        const int sy = std::min(logo_h - 1, static_cast<int>(y / scale));
        for (int x = 0; x < dw; ++x) {
            const int sx = std::min(logo_w - 1, static_cast<int>(x / scale));
            const size_t si = (static_cast<size_t>(sy) * logo_w + sx) * 4u;
            const uint8_t r = logo_rgba[si + 0];
            const uint8_t g = logo_rgba[si + 1];
            const uint8_t b = logo_rgba[si + 2];
            const uint8_t a = logo_rgba[si + 3];
            // out = logo.rgb * a + bg.rgb * (1-a)
            const uint8_t out_r = static_cast<uint8_t>((r * a + bg_r * (255 - a)) / 255);
            const uint8_t out_g = static_cast<uint8_t>((g * a + bg_g * (255 - a)) / 255);
            const uint8_t out_b = static_cast<uint8_t>((b * a + bg_b * (255 - a)) / 255);
            const unsigned long pixel =
                PackChannel(visual->red_mask, out_r) |
                PackChannel(visual->green_mask, out_g) |
                PackChannel(visual->blue_mask, out_b);
            XPutPixel(img, x, y, pixel); // XPutPixel 自动处理字节序 / bytes_per_pixel
        }
    }

    logo_img = img;
    logo_dw = dw;
    logo_dh = dh;
    logo_dx = (cw - dw) / 2;
    logo_dy = static_cast<int>(chh * 0.16);
    logo_bottom = logo_dy + dh;
}

XFontSet SplashScreen::Impl::MakeFontSet(int pixel_size) {
    char base[256];
    // 主名 + 通配回退；XCreateFontSet 会按 locale 补齐所需字符集（含中文）。
    std::snprintf(base, sizeof(base),
                  "-*-*-medium-r-normal--%d-*-*-*-*-*-*-*,"
                  "-*-*-*-r-*--%d-*-*-*-*-*-*-*,"
                  "-*-*-*-*-*--%d-*-*-*-*-*-*-*",
                  pixel_size, pixel_size, pixel_size);
    char** missing = nullptr;
    int missing_count = 0;
    char* def_string = nullptr;
    XFontSet fs = XCreateFontSet(dpy, base, &missing, &missing_count, &def_string);
    if (missing) XFreeStringList(missing);
    return fs;
}

void SplashScreen::Impl::SetOpacity(int alpha) {
    Atom prop = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    if (prop == None) return;
    const unsigned long opacity =
        static_cast<unsigned long>(0xFFFFFFFFu / 255u) * static_cast<unsigned long>(alpha);
    XChangeProperty(dpy, window, prop, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&opacity), 1);
}

void SplashScreen::Impl::Repaint() {
    const int cw = config.card_width;
    const int chh = config.card_height;

    // 背景。
    XSetForeground(dpy, gc, PackColor(visual, config.bg_argb));
    XFillRectangle(dpy, back, gc, 0, 0, static_cast<unsigned int>(cw), static_cast<unsigned int>(chh));

    // 底部强调色细线（贯穿整宽，静态“加载中”指示）。
    XSetForeground(dpy, gc, PackColor(visual, config.accent_argb));
    XFillRectangle(dpy, back, gc, 0, chh - 3, static_cast<unsigned int>(cw), 3u);

    // logo。
    int title_top = static_cast<int>(chh * 0.16);
    if (logo_img) {
        XPutImage(dpy, back, gc, logo_img, 0, 0, logo_dx, logo_dy,
                  static_cast<unsigned int>(logo_dw), static_cast<unsigned int>(logo_dh));
        title_top = logo_bottom;
    }

    // 标题（居中）。
    if (title_fs && !config.app_name.empty()) {
        const char* text = config.app_name.c_str();
        const int len = static_cast<int>(config.app_name.size());
        XRectangle ink{}, logical{};
        Xutf8TextExtents(title_fs, text, len, &ink, &logical);
        const int tx = (cw - logical.width) / 2;
        const int baseline = title_top + 16 - logical.y; // -logical.y == ascent
        XSetForeground(dpy, gc, PackColor(visual, config.title_argb));
        Xutf8DrawString(dpy, back, title_fs, gc, tx, baseline, text, len);
    }

    // 状态行（居中，限制在底部区域内）。
    std::string status_copy;
    {
        std::lock_guard<std::mutex> lk(status_mutex);
        status_copy = status;
    }
    last_painted_status = status_copy;
    if (status_fs && !status_copy.empty()) {
        const char* text = status_copy.c_str();
        const int len = static_cast<int>(status_copy.size());
        XRectangle ink{}, logical{};
        Xutf8TextExtents(status_fs, text, len, &ink, &logical);
        int sx = (cw - logical.width) / 2;
        if (sx < 16) sx = 16;
        const int baseline = chh - 16;
        XRectangle clip{16, static_cast<short>(chh - 40),
                        static_cast<unsigned short>(std::max(1, cw - 32)), 28};
        XSetClipRectangles(dpy, gc, 0, 0, &clip, 1, Unsorted);
        XSetForeground(dpy, gc, PackColor(visual, config.status_argb));
        Xutf8DrawString(dpy, back, status_fs, gc, sx, baseline, text, len);
        XSetClipMask(dpy, gc, None);
    }

    // 后台 Pixmap 拷到窗口。
    XCopyArea(dpy, back, window, gc, 0, 0,
              static_cast<unsigned int>(cw), static_cast<unsigned int>(chh), 0, 0);
    XFlush(dpy);
}

void SplashScreen::Impl::ThreadMain() {
    LoadLogo();

    static std::once_flag locale_once;
    std::call_once(locale_once, []() { std::setlocale(LC_CTYPE, ""); });

    screen = DefaultScreen(dpy);
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
    colormap = DefaultColormap(dpy, screen);

    const int cw = std::max(120, config.card_width);
    const int chh = std::max(120, config.card_height);
    config.card_width = cw;
    config.card_height = chh;
    const int sx = (DisplayWidth(dpy, screen) - cw) / 2;
    const int sy = (DisplayHeight(dpy, screen) - chh) / 2;

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True; // 无边框、绕过 WM 摆放
    attrs.background_pixel = PackColor(visual, config.bg_argb);
    attrs.border_pixel = 0;
    attrs.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    const unsigned long mask = CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask;

    window = XCreateWindow(dpy, RootWindow(dpy, screen), sx, sy,
                           static_cast<unsigned int>(cw), static_cast<unsigned int>(chh), 0,
                           depth, InputOutput, visual, mask, &attrs);
    if (!window) {
        running.store(false);
        return;
    }

    // _NET_WM_WINDOW_TYPE = _NET_WM_WINDOW_TYPE_SPLASH
    Atom wt = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wt_splash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    if (wt != None && wt_splash != None) {
        XChangeProperty(dpy, window, wt, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&wt_splash), 1);
    }

    back = XCreatePixmap(dpy, window, static_cast<unsigned int>(cw),
                         static_cast<unsigned int>(chh), static_cast<unsigned int>(depth));
    gc = XCreateGC(dpy, back, 0, nullptr);

    title_fs = MakeFontSet(30);
    status_fs = MakeFontSet(15);

    BuildLogoImage();

    if (std::getenv("DSE_SPLASH_DEBUG")) {
        std::fprintf(stderr,
                     "[splash][x11] logo=%dx%d win=%lu pos=%d,%d size=%dx%d depth=%d img='%s'\n",
                     logo_w, logo_h, static_cast<unsigned long>(window), sx, sy, cw, chh, depth,
                     config.image_path.c_str());
        std::fflush(stderr);
    }

    show_tick = NowMs();
    phase = 0;
    SetOpacity(0);
    XMapRaised(dpy, window);
    XFlush(dpy);

    const int fade_in = std::max(1, config.fade_in_ms);
    const int fade_out = std::max(1, config.fade_out_ms);

    bool done = false;
    bool first_paint = true;
    while (!done) {
        // 处理已到达的事件（非阻塞）。
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case Expose:
                    Repaint();
                    break;
                case ButtonPress:
                    skip_requested.store(true);
                    break;
                case KeyPress: {
                    KeySym ks = XLookupKeysym(&ev.xkey, 0);
                    if (ks == XK_Escape) skip_requested.store(true);
                    break;
                }
                default:
                    break;
            }
        }

        const uint64_t now = NowMs();
        const uint64_t elapsed = now - show_tick;

        const bool can_close =
            finish_requested.load() &&
            (elapsed >= static_cast<uint64_t>(config.min_display_ms) || skip_requested.load());
        if (phase != 2 && can_close) {
            phase = 2;
            fade_out_tick = now;
        }

        int alpha = 255;
        if (phase == 0) {
            alpha = static_cast<int>(255 * elapsed / static_cast<uint64_t>(fade_in));
            if (elapsed >= static_cast<uint64_t>(fade_in)) {
                alpha = 255;
                phase = 1;
            }
        } else if (phase == 2) {
            const uint64_t fo = now - fade_out_tick;
            alpha = static_cast<int>(255 - 255 * fo / static_cast<uint64_t>(fade_out));
            if (fo >= static_cast<uint64_t>(fade_out)) {
                done = true;
            }
        }
        alpha = std::clamp(alpha, 0, 255);
        if (alpha != last_alpha) {
            SetOpacity(alpha);
            last_alpha = alpha;
        }

        // 首帧或状态文字变化时重绘。
        std::string cur;
        {
            std::lock_guard<std::mutex> lk(status_mutex);
            cur = status;
        }
        if (first_paint || cur != last_painted_status) {
            Repaint();
            first_paint = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    // 清理。
    if (logo_img) {
        XDestroyImage(logo_img);
        logo_img = nullptr;
    }
    if (title_fs) {
        XFreeFontSet(dpy, title_fs);
        title_fs = nullptr;
    }
    if (status_fs) {
        XFreeFontSet(dpy, status_fs);
        status_fs = nullptr;
    }
    if (gc) {
        XFreeGC(dpy, gc);
        gc = nullptr;
    }
    if (back) {
        XFreePixmap(dpy, back);
        back = 0;
    }
    if (window) {
        XDestroyWindow(dpy, window);
        window = 0;
    }
    XCloseDisplay(dpy);
    dpy = nullptr;
    running.store(false);
}

SplashScreen::SplashScreen() : impl_(std::make_unique<Impl>()) {}

SplashScreen::~SplashScreen() {
    if (impl_ && impl_->running.load()) {
        impl_->skip_requested.store(true);
        impl_->finish_requested.store(true);
        if (impl_->thread.joinable()) impl_->thread.join();
    }
}

bool SplashScreen::Show(const SplashConfig& config) {
    if (!config.enabled) return false;
    if (impl_->running.load()) return false;

    // 无 DISPLAY / 无法连接 X server 时优雅降级。
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;

    impl_->config = config;
    impl_->status = config.initial_status;
    impl_->dpy = dpy; // 仅 splash 线程后续使用，主线程不再触碰
    impl_->running.store(true);
    impl_->thread = std::thread([this]() { impl_->ThreadMain(); });
    return true;
}

void SplashScreen::SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lk(impl_->status_mutex);
    impl_->status = status;
}

bool SplashScreen::SkipRequested() const {
    return impl_->skip_requested.load();
}

bool SplashScreen::IsActive() const {
    return impl_->running.load();
}

void SplashScreen::Finish() {
    impl_->finish_requested.store(true);
    if (impl_->thread.joinable()) impl_->thread.join();
}

} // namespace dse::platform

#elif defined(DSE_ENABLE_APPLE_PLATFORM) && defined(__APPLE__)
// =============================================================================
// Apple 平台（macOS / iOS）：安全 no-op。
// macOS/iOS 的 splash 由系统 LaunchScreen.storyboard 处理，
// 引擎不自绘 splash 窗口，Show() 返回 false 使引擎跳过自绘 splash 流程。
// =============================================================================

namespace dse::platform {

struct SplashScreen::Impl {};

SplashScreen::SplashScreen() = default;
SplashScreen::~SplashScreen() = default;
bool SplashScreen::Show(const SplashConfig&) { return false; }
void SplashScreen::SetStatus(const std::string&) {}
bool SplashScreen::SkipRequested() const { return false; }
bool SplashScreen::IsActive() const { return false; }
void SplashScreen::Finish() {}

} // namespace dse::platform

#else // 其它平台（android 等）：安全 no-op。

namespace dse::platform {

struct SplashScreen::Impl {};

SplashScreen::SplashScreen() = default;
SplashScreen::~SplashScreen() = default;
bool SplashScreen::Show(const SplashConfig&) { return false; }
void SplashScreen::SetStatus(const std::string&) {}
bool SplashScreen::SkipRequested() const { return false; }
bool SplashScreen::IsActive() const { return false; }
void SplashScreen::Finish() {}

} // namespace dse::platform

#endif // _WIN32 / __linux__
