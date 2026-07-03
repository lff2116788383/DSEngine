#pragma once
/**
 * @file render_capture_helper.h
 * @brief Viewport screenshot capture and pixel analysis for render validation tests.
 */

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace dse::editor::uitest {

struct CapturedPixels {
    std::vector<uint8_t> data;  // RGBA, row-major, top-left origin
    int width = 0, height = 0;
    bool valid = false;

    struct RGBA {
        uint8_t r, g, b, a;
        float Brightness() const {
            return (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;
        }
        bool IsBlack(uint8_t threshold = 10) const {
            return r < threshold && g < threshold && b < threshold;
        }
    };

    RGBA GetPixel(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return {0,0,0,0};
        int idx = (y * width + x) * 4;
        return {data[idx], data[idx+1], data[idx+2], data[idx+3]};
    }

    int PixelCount() const { return width * height; }

    // === Basic assertions ===

    float NonBlackRatio(uint8_t threshold = 10) const {
        if (!valid || data.empty()) return 0.0f;
        int count = 0;
        int total = width * height;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            if (data[idx] > threshold || data[idx+1] > threshold || data[idx+2] > threshold)
                count++;
        }
        return (float)count / total;
    }

    float AverageBrightness() const {
        if (!valid || data.empty()) return 0.0f;
        double sum = 0.0;
        int total = width * height;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            sum += (data[idx] * 0.299 + data[idx+1] * 0.587 + data[idx+2] * 0.114) / 255.0;
        }
        return (float)(sum / total);
    }

    float RegionBrightness(int x0, int y0, int x1, int y1) const {
        if (!valid) return 0.0f;
        x0 = std::max(0, x0); y0 = std::max(0, y0);
        x1 = std::min(width, x1); y1 = std::min(height, y1);
        double sum = 0.0;
        int count = 0;
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                int idx = (y * width + x) * 4;
                sum += (data[idx] * 0.299 + data[idx+1] * 0.587 + data[idx+2] * 0.114) / 255.0;
                count++;
            }
        }
        return count > 0 ? (float)(sum / count) : 0.0f;
    }

    float CenterBrightness(float radius_ratio = 0.25f) const {
        int cx = width / 2, cy = height / 2;
        int r = (int)(std::min(width, height) * radius_ratio);
        return RegionBrightness(cx - r, cy - r, cx + r, cy + r);
    }

    // === Color assertions ===

    float RedDominance() const {
        if (!valid || data.empty()) return 0.0f;
        double rSum = 0, gSum = 0, bSum = 0;
        int total = width * height;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            rSum += data[idx]; gSum += data[idx+1]; bSum += data[idx+2];
        }
        double all = rSum + gSum + bSum;
        return all > 0 ? (float)(rSum / all) : 0.333f;
    }

    float GreenDominance() const {
        if (!valid || data.empty()) return 0.0f;
        double rSum = 0, gSum = 0, bSum = 0;
        int total = width * height;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            rSum += data[idx]; gSum += data[idx+1]; bSum += data[idx+2];
        }
        double all = rSum + gSum + bSum;
        return all > 0 ? (float)(gSum / all) : 0.333f;
    }

    float BlueDominance() const {
        if (!valid || data.empty()) return 0.0f;
        double rSum = 0, gSum = 0, bSum = 0;
        int total = width * height;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            rSum += data[idx]; gSum += data[idx+1]; bSum += data[idx+2];
        }
        double all = rSum + gSum + bSum;
        return all > 0 ? (float)(bSum / all) : 0.333f;
    }

    bool IsColorDominant(char channel, float threshold = 0.45f) const {
        switch (channel) {
            case 'r': case 'R': return RedDominance() > threshold;
            case 'g': case 'G': return GreenDominance() > threshold;
            case 'b': case 'B': return BlueDominance() > threshold;
        }
        return false;
    }

    float ColorVariance() const {
        if (!valid || data.empty()) return 0.0f;
        int total = width * height;
        double mean_r = 0, mean_g = 0, mean_b = 0;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            mean_r += data[idx]; mean_g += data[idx+1]; mean_b += data[idx+2];
        }
        mean_r /= total; mean_g /= total; mean_b /= total;
        double var = 0;
        for (int i = 0; i < total; i++) {
            int idx = i * 4;
            double dr = data[idx] - mean_r, dg = data[idx+1] - mean_g, db = data[idx+2] - mean_b;
            var += (dr*dr + dg*dg + db*db);
        }
        return (float)(var / (total * 3.0 * 255.0 * 255.0));
    }

    // === Region comparison ===

    float LeftHalfBrightness() const { return RegionBrightness(0, 0, width/2, height); }
    float RightHalfBrightness() const { return RegionBrightness(width/2, 0, width, height); }
    float TopHalfBrightness() const { return RegionBrightness(0, 0, width, height/2); }
    float BottomHalfBrightness() const { return RegionBrightness(0, height/2, width, height); }

    // === Static comparison ===

    static float BrightnessDiff(const CapturedPixels& a, const CapturedPixels& b) {
        return a.AverageBrightness() - b.AverageBrightness();
    }

    static bool IsBrighter(const CapturedPixels& brighter, const CapturedPixels& darker) {
        return brighter.AverageBrightness() > darker.AverageBrightness();
    }

    static float PixelDifference(const CapturedPixels& a, const CapturedPixels& b) {
        if (!a.valid || !b.valid || a.width != b.width || a.height != b.height) return 1.0f;
        double diff = 0;
        int total = a.width * a.height;
        for (int i = 0; i < total * 4; i++) {
            double d = (double)a.data[i] - (double)b.data[i];
            diff += std::abs(d);
        }
        return (float)(diff / (total * 4.0 * 255.0));
    }

    static bool IsStable(const CapturedPixels& f1, const CapturedPixels& f2, float threshold = 0.01f) {
        return PixelDifference(f1, f2) < threshold;
    }
};

// === Capture operations ===

/// Capture the editor window to a PNG file using PowerShell
/// Returns true on success
bool CaptureEditorWindow(const std::string& output_path, int delay_ms = 300);

/// Capture a specific screen region
bool CaptureScreenRegion(const std::string& output_path, int x, int y, int w, int h);

/// Capture Scene viewport (reads coords from viewport_rect.txt)
bool CaptureSceneViewport(const std::string& output_path, int delay_ms = 300);

/// Load a PNG file into CapturedPixels (uses stb_image)
CapturedPixels LoadCapturedPNG(const std::string& path);

/// Wait for render to stabilize (compare consecutive frames)
bool WaitForRenderStable(int max_attempts = 5, float threshold = 0.02f, int interval_ms = 100);

/// Write viewport coordinates file for PowerShell to read
void WriteViewportCoords(const std::string& path, int x, int y, int w, int h);

} // namespace dse::editor::uitest