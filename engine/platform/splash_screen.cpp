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

#else // !_WIN32

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

#endif // _WIN32
