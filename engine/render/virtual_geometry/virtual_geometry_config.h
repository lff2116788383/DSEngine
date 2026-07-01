/**
 * @file virtual_geometry_config.h
 * @brief Virtual Geometry (Nanite-style) system configuration
 *
 * Compile-time gate: DSE_ENABLE_VIRTUAL_GEOMETRY
 * Runtime toggle:    VirtualGeometryConfig::enabled
 *
 * When disabled at compile time, all VG code is stripped.
 * When disabled at runtime, the renderer falls back to traditional per-object draw.
 */

#ifndef DSE_VIRTUAL_GEOMETRY_CONFIG_H
#define DSE_VIRTUAL_GEOMETRY_CONFIG_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include <cstdint>

namespace dse {
namespace render {
namespace vg {

struct VirtualGeometryConfig {
    bool enabled = false;

    bool enable_software_rasterizer = true;
    float software_raster_threshold = 32.0f;

    bool enable_visibility_buffer = true;

    bool enable_cluster_streaming = true;
    uint32_t vram_budget_mb = 512;
    uint32_t max_resident_clusters = 1024 * 1024;

    float lod_error_threshold = 1.0f;
    uint32_t max_screen_error_pixels = 1;

    bool enable_impostor_fallback = true;
    float impostor_distance_factor = 100.0f;

    bool debug_draw_clusters = false;
    bool debug_freeze_lod = false;
    bool debug_force_software_raster = false;
    bool debug_show_visibility_buffer = false;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_VIRTUAL_GEOMETRY_CONFIG_H
