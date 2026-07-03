/**
 * @file render_capture_helper.cpp
 * @brief Implementation of viewport capture and pixel analysis helpers.
 */
#include "render_capture_helper.h"

#include "stb/stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dse::editor::uitest {

bool CaptureEditorWindow(const std::string& output_path, int delay_ms) {
#ifdef _WIN32
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "powershell -ExecutionPolicy Bypass -File tools/capture_viewport.ps1 "
        "-OutputPath \"%s\" -Mode window -WindowTitle DSEngine -DelayMs %d",
        output_path.c_str(), delay_ms);
    int ret = system(cmd);
    return ret == 0;
#else
    (void)output_path; (void)delay_ms;
    return false;
#endif
}

bool CaptureScreenRegion(const std::string& output_path, int x, int y, int w, int h) {
#ifdef _WIN32
    char coords[64];
    snprintf(coords, sizeof(coords), "%d,%d,%d,%d", x, y, w, h);
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "powershell -ExecutionPolicy Bypass -File tools/capture_viewport.ps1 "
        "-OutputPath \"%s\" -Mode region -CoordsFile \"%s\" -DelayMs 100",
        output_path.c_str(), coords);
    int ret = system(cmd);
    return ret == 0;
#else
    (void)output_path; (void)x; (void)y; (void)w; (void)h;
    return false;
#endif
}

bool CaptureSceneViewport(const std::string& output_path, int delay_ms) {
#ifdef _WIN32
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "powershell -ExecutionPolicy Bypass -File tools/capture_viewport.ps1 "
        "-OutputPath \"%s\" -Mode viewport -DelayMs %d",
        output_path.c_str(), delay_ms);
    int ret = system(cmd);
    return ret == 0;
#else
    (void)output_path; (void)delay_ms;
    return false;
#endif
}

CapturedPixels LoadCapturedPNG(const std::string& path) {
    CapturedPixels result;
    int channels = 0;
    unsigned char* img = stbi_load(path.c_str(), &result.width, &result.height, &channels, 4);
    if (!img) {
        result.valid = false;
        return result;
    }
    int size = result.width * result.height * 4;
    result.data.assign(img, img + size);
    stbi_image_free(img);
    result.valid = true;
    return result;
}

bool WaitForRenderStable(int max_attempts, float threshold, int interval_ms) {
    const std::string tmp1 = "C:\\temp\\_stable_a.png";
    const std::string tmp2 = "C:\\temp\\_stable_b.png";

    CaptureEditorWindow(tmp1, 50);
    auto prev = LoadCapturedPNG(tmp1);

    for (int i = 0; i < max_attempts; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        CaptureEditorWindow(tmp2, 50);
        auto curr = LoadCapturedPNG(tmp2);

        if (prev.valid && curr.valid && CapturedPixels::IsStable(prev, curr, threshold)) {
            return true;
        }
        prev = curr;
    }
    return false;
}

void WriteViewportCoords(const std::string& path, int x, int y, int w, int h) {
    std::ofstream f(path);
    if (f.is_open()) {
        f << x << "," << y << "," << w << "," << h;
        f.close();
    }
}

} // namespace dse::editor::uitest