#include "editor_viewport_panel.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/script.h"
#include "engine/ecs/world.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "editor_scene_camera.h"
#include "editor_selection.h"
#include "editor_shortcuts.h"
#include "editor_shared_components.h"
#include "editor_console_panel.h"
#include "editor_asset_db.h"
#include "editor_tilemap_panel.h"
#include "editor_terrain_panel.h"
#include "editor_audio_panel.h"
#include "editor_preferences_panel.h"
#include "engine/ecs/components_3d_physics.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dse::editor {

namespace {

struct GizmoUndoState {
    bool was_using = false;
    entt::entity entity = entt::null;
    glm::vec3 initial_position{0.0f};
    glm::quat initial_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 initial_scale{1.0f};
};

GizmoUndoState& GetGizmoUndoState() {
    static GizmoUndoState state;
    return state;
}

struct MarqueeState {
    bool active = false;
    glm::vec2 start_pos{0};
    glm::vec2 end_pos{0};
};

MarqueeState& GetMarqueeState() {
    static MarqueeState state;
    return state;
}

struct MultiGizmoUndoState {
    bool was_using = false;
    glm::vec3 initial_centroid{0.0f};
    struct EntitySnapshot {
        entt::entity entity = entt::null;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
    };
    std::vector<EntitySnapshot> snapshots;
};

MultiGizmoUndoState& GetMultiGizmoUndoState() {
    static MultiGizmoUndoState state;
    return state;
}

// --- Color-ID FBO Picker ---
struct ColorIDPicker {
    unsigned int fbo = 0;
    unsigned int color_tex = 0;
    unsigned int depth_rbo = 0;
    int fb_width = 0;
    int fb_height = 0;

    unsigned int shader = 0;
    int u_pos_loc = -1;
    int u_size_loc = -1;
    int u_color_loc = -1;
    int u_viewport_loc = -1;

    unsigned int quad_vao = 0;
    unsigned int quad_vbo = 0;
    bool gl_ready = false;

    void Init() {
        if (gl_ready) return;
        const char* vs_src =
            "#version 330 core\n"
            "layout(location = 0) in vec2 a_vert;\n"
            "uniform vec2 u_pos;\n"
            "uniform vec2 u_size;\n"
            "uniform vec2 u_viewport;\n"
            "void main() {\n"
            "  vec2 screen = u_pos + a_vert * u_size;\n"
            "  vec2 ndc = (screen / u_viewport) * 2.0 - 1.0;\n"
            "  ndc.y = -ndc.y;\n"
            "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
            "}\n";
        const char* fs_src =
            "#version 330 core\n"
            "uniform vec4 u_color;\n"
            "out vec4 frag_color;\n"
            "void main() { frag_color = u_color; }\n";

        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vs_src, nullptr);
        glCompileShader(vs);
        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_src, nullptr);
        glCompileShader(fs);
        shader = glCreateProgram();
        glAttachShader(shader, vs);
        glAttachShader(shader, fs);
        glLinkProgram(shader);
        glDeleteShader(vs);
        glDeleteShader(fs);

        u_pos_loc      = glGetUniformLocation(shader, "u_pos");
        u_size_loc     = glGetUniformLocation(shader, "u_size");
        u_color_loc    = glGetUniformLocation(shader, "u_color");
        u_viewport_loc = glGetUniformLocation(shader, "u_viewport");

        float verts[] = { -1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1 };
        glGenVertexArrays(1, &quad_vao);
        glGenBuffers(1, &quad_vbo);
        glBindVertexArray(quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);
        gl_ready = true;
    }

    void EnsureFBO(int w, int h) {
        if (fb_width == w && fb_height == h && fbo != 0) return;
        if (fbo) { glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &color_tex); glDeleteRenderbuffers(1, &depth_rbo); }
        fb_width = w; fb_height = h;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &color_tex);
        glBindTexture(GL_TEXTURE_2D, color_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
        glGenRenderbuffers(1, &depth_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    static glm::vec4 EntityToColor(entt::entity e) {
        uint32_t id = static_cast<uint32_t>(e) + 1;
        return glm::vec4((id & 0xFF) / 255.0f, ((id >> 8) & 0xFF) / 255.0f,
                         ((id >> 16) & 0xFF) / 255.0f, 1.0f);
    }
    static entt::entity ColorToEntity(unsigned char r, unsigned char g, unsigned char b) {
        uint32_t id = r | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16);
        if (id == 0) return entt::null;
        return static_cast<entt::entity>(id - 1);
    }

    entt::entity Pick(entt::registry& registry,
                      const glm::vec2& mouse_screen,
                      const glm::vec2& viewport_pos,
                      const glm::vec2& viewport_size,
                      const glm::mat4& view,
                      const glm::mat4& proj) {
        Init();
        int w = static_cast<int>(viewport_size.x);
        int h = static_cast<int>(viewport_size.y);
        if (w <= 0 || h <= 0) return entt::null;
        EnsureFBO(w, h);

        GLint prev_fbo; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        GLint prev_vp[4]; glGetIntegerv(GL_VIEWPORT, prev_vp);
        GLboolean prev_blend = glIsEnabled(GL_BLEND);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_BLEND);

        glUseProgram(shader);
        glUniform2f(u_viewport_loc, static_cast<float>(w), static_cast<float>(h));
        glBindVertexArray(quad_vao);

        glm::mat4 vp = proj * view;
        const float pick_half = 14.0f;

        for (auto entity : registry.storage<entt::entity>()) {
            if (!registry.valid(entity)) continue;
            if (!registry.all_of<TransformComponent>(entity)) continue;
            auto& tf = registry.get<TransformComponent>(entity);
            glm::vec4 clip = vp * glm::vec4(tf.position, 1.0f);
            if (clip.w <= 0.001f) continue;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) continue;
            float sx = (ndc.x * 0.5f + 0.5f) * w;
            float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * h;
            glm::vec4 color = EntityToColor(entity);
            glUniform2f(u_pos_loc, sx, sy);
            glUniform2f(u_size_loc, pick_half, pick_half);
            glUniform4f(u_color_loc, color.r, color.g, color.b, color.a);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        float mx = mouse_screen.x - viewport_pos.x;
        float my = mouse_screen.y - viewport_pos.y;
        int px = static_cast<int>(mx);
        int py = h - 1 - static_cast<int>(my);
        entt::entity result = entt::null;
        if (px >= 0 && px < w && py >= 0 && py < h) {
            unsigned char pixel[4] = {0};
            glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            result = ColorToEntity(pixel[0], pixel[1], pixel[2]);
        }

        glBindVertexArray(0);
        glUseProgram(0);
        if (prev_blend) glEnable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
        return result;
    }
};

ColorIDPicker& GetColorIDPicker() {
    static ColorIDPicker picker;
    return picker;
}

void DrawViewportGrid(ImDrawList* draw_list,
                      const glm::vec2& viewport_pos,
                      const glm::vec2& viewport_size,
                      const glm::mat4& view,
                      const glm::mat4& proj) {
    const glm::mat4 vp = proj * view;

    auto project = [&](const glm::vec3& world) -> glm::vec2 {
        glm::vec4 clip = vp * glm::vec4(world, 1.0f);
        if (clip.w <= 0.001f) return glm::vec2(-10000.0f);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.x < -1.5f || ndc.x > 1.5f || ndc.y < -1.5f || ndc.y > 1.5f) return glm::vec2(-10000.0f);
        float sx = viewport_pos.x + (ndc.x * 0.5f + 0.5f) * viewport_size.x;
        float sy = viewport_pos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y;
        return glm::vec2(sx, sy);
    };

    auto draw_line = [&](const glm::vec3& a, const glm::vec3& b, ImU32 color, float thickness) {
        glm::vec2 sa = project(a);
        glm::vec2 sb = project(b);
        if (sa.x > -5000.0f && sb.x > -5000.0f) {
            draw_list->AddLine(ImVec2(sa.x, sa.y), ImVec2(sb.x, sb.y), color, thickness);
        }
    };

    // Compute camera distance to Y=0 plane for LOD
    glm::mat4 inv_view = glm::inverse(view);
    glm::vec3 cam_pos = glm::vec3(inv_view[3]);
    float cam_dist = std::abs(cam_pos.y) + glm::length(glm::vec2(cam_pos.x, cam_pos.z)) * 0.1f;
    if (cam_dist < 1.0f) cam_dist = 1.0f;

    // Determine grid extent based on camera distance
    float extent = cam_dist * 2.0f;
    if (extent < 20.0f) extent = 20.0f;
    if (extent > 500.0f) extent = 500.0f;

    // Snap extent to grid
    float major_step = 10.0f;
    float minor_step = 1.0f;
    float grid_min_x = std::floor((cam_pos.x - extent) / major_step) * major_step;
    float grid_max_x = std::ceil((cam_pos.x + extent) / major_step) * major_step;
    float grid_min_z = std::floor((cam_pos.z - extent) / major_step) * major_step;
    float grid_max_z = std::ceil((cam_pos.z + extent) / major_step) * major_step;

    // Draw minor grid lines (only when close enough)
    bool draw_minor = cam_dist < 50.0f;
    if (draw_minor) {
        ImU32 minor_color = IM_COL32(60, 60, 60, 80);
        float minor_min_x = std::floor((cam_pos.x - extent) / minor_step) * minor_step;
        float minor_max_x = std::ceil((cam_pos.x + extent) / minor_step) * minor_step;
        float minor_min_z = std::floor((cam_pos.z - extent) / minor_step) * minor_step;
        float minor_max_z = std::ceil((cam_pos.z + extent) / minor_step) * minor_step;
        // Limit max lines to avoid performance issues
        int max_lines = 200;
        int line_count = 0;
        for (float x = minor_min_x; x <= minor_max_x && line_count < max_lines; x += minor_step) {
            // Skip major grid lines
            if (std::abs(std::fmod(x, major_step)) < 0.01f) continue;
            draw_line(glm::vec3(x, 0, minor_min_z), glm::vec3(x, 0, minor_max_z), minor_color, 1.0f);
            ++line_count;
        }
        for (float z = minor_min_z; z <= minor_max_z && line_count < max_lines; z += minor_step) {
            if (std::abs(std::fmod(z, major_step)) < 0.01f) continue;
            draw_line(glm::vec3(minor_min_x, 0, z), glm::vec3(minor_max_x, 0, z), minor_color, 1.0f);
            ++line_count;
        }
    }

    // Draw major grid lines
    ImU32 major_color = IM_COL32(90, 90, 90, 120);
    for (float x = grid_min_x; x <= grid_max_x; x += major_step) {
        if (std::abs(x) < 0.01f) continue; // Skip origin, drawn separately
        draw_line(glm::vec3(x, 0, grid_min_z), glm::vec3(x, 0, grid_max_z), major_color, 1.0f);
    }
    for (float z = grid_min_z; z <= grid_max_z; z += major_step) {
        if (std::abs(z) < 0.01f) continue;
        draw_line(glm::vec3(grid_min_x, 0, z), glm::vec3(grid_max_x, 0, z), major_color, 1.0f);
    }

    // X axis (red)
    draw_line(glm::vec3(grid_min_x, 0, 0), glm::vec3(grid_max_x, 0, 0), IM_COL32(200, 50, 50, 200), 2.0f);
    // Z axis (blue)
    draw_line(glm::vec3(0, 0, grid_min_z), glm::vec3(0, 0, grid_max_z), IM_COL32(50, 50, 200, 200), 2.0f);
}

// --- Probe debug overlays (Light Probe sphere + Reflection Probe wireframe) ---
void DrawProbeOverlays(entt::registry& registry,
                       const glm::vec2& viewport_pos,
                       const glm::vec2& viewport_size,
                       const glm::mat4& view,
                       const glm::mat4& proj) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const glm::mat4 vp = proj * view;

    // Helper: project world point to screen
    auto project = [&](const glm::vec3& world_pos) -> ImVec2 {
        glm::vec4 clip = vp * glm::vec4(world_pos, 1.0f);
        if (clip.w <= 0.001f) return ImVec2(-9999, -9999);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = viewport_pos.x + (ndc.x * 0.5f + 0.5f) * viewport_size.x;
        float sy = viewport_pos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y;
        return ImVec2(sx, sy);
    };

    // Helper: draw wireframe circle in world space (approximated with line segments)
    auto draw_circle_3d = [&](const glm::vec3& center, float radius, const glm::vec3& axis, ImU32 color, int segments = 32) {
        // Build orthonormal basis from axis
        glm::vec3 up = (std::abs(glm::dot(axis, glm::vec3(0, 1, 0))) < 0.99f)
                       ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(axis, up));
        glm::vec3 forward = glm::normalize(glm::cross(right, axis));

        ImVec2 prev_pt = project(center + right * radius);
        for (int i = 1; i <= segments; ++i) {
            float angle = (static_cast<float>(i) / static_cast<float>(segments)) * 6.2831853f;
            glm::vec3 pt = center + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
            ImVec2 screen_pt = project(pt);
            if (prev_pt.x > -9000 && screen_pt.x > -9000) {
                draw_list->AddLine(prev_pt, screen_pt, color, 1.5f);
            }
            prev_pt = screen_pt;
        }
    };

    // Draw Light Probes
    auto lp_view = registry.view<dse::LightProbeComponent, TransformComponent>();
    for (auto [entity, probe, transform] : lp_view.each()) {
        if (!probe.enabled || !probe.show_debug) continue;
        glm::vec3 pos = transform.position;
        ImU32 color = IM_COL32(255, 220, 50, 180); // warm yellow

        // Draw 3 orthogonal circles
        draw_circle_3d(pos, probe.influence_radius, glm::vec3(1, 0, 0), color, 24);
        draw_circle_3d(pos, probe.influence_radius, glm::vec3(0, 1, 0), color, 24);
        draw_circle_3d(pos, probe.influence_radius, glm::vec3(0, 0, 1), color, 24);

        // Draw center dot
        ImVec2 center_screen = project(pos);
        if (center_screen.x > -9000) {
            draw_list->AddCircleFilled(center_screen, 5.0f, IM_COL32(255, 220, 50, 220));
            draw_list->AddText(ImVec2(center_screen.x + 8, center_screen.y - 8),
                               IM_COL32(255, 220, 50, 200), "LP");
        }
    }

    // Draw Reflection Probes
    auto rp_view = registry.view<dse::ReflectionProbeComponent, TransformComponent>();
    for (auto [entity, probe, transform] : rp_view.each()) {
        if (!probe.enabled || !probe.show_debug) continue;
        glm::vec3 pos = transform.position;
        ImU32 color = IM_COL32(80, 180, 255, 180); // cool blue

        if (probe.use_box_projection) {
            // Draw wireframe box
            glm::vec3 half(probe.box_size_x * 0.5f, probe.box_size_y * 0.5f, probe.box_size_z * 0.5f);
            glm::vec3 corners[8] = {
                pos + glm::vec3(-half.x, -half.y, -half.z),
                pos + glm::vec3( half.x, -half.y, -half.z),
                pos + glm::vec3( half.x,  half.y, -half.z),
                pos + glm::vec3(-half.x,  half.y, -half.z),
                pos + glm::vec3(-half.x, -half.y,  half.z),
                pos + glm::vec3( half.x, -half.y,  half.z),
                pos + glm::vec3( half.x,  half.y,  half.z),
                pos + glm::vec3(-half.x,  half.y,  half.z),
            };
            ImVec2 sc[8];
            for (int i = 0; i < 8; ++i) sc[i] = project(corners[i]);
            // Bottom face
            for (int i = 0; i < 4; ++i) {
                int next = (i + 1) % 4;
                if (sc[i].x > -9000 && sc[next].x > -9000) draw_list->AddLine(sc[i], sc[next], color, 1.5f);
            }
            // Top face
            for (int i = 4; i < 8; ++i) {
                int next = 4 + (i - 4 + 1) % 4;
                if (sc[i].x > -9000 && sc[next].x > -9000) draw_list->AddLine(sc[i], sc[next], color, 1.5f);
            }
            // Vertical edges
            for (int i = 0; i < 4; ++i) {
                if (sc[i].x > -9000 && sc[i + 4].x > -9000) draw_list->AddLine(sc[i], sc[i + 4], color, 1.5f);
            }
        } else {
            // Draw sphere (3 circles)
            draw_circle_3d(pos, probe.influence_radius, glm::vec3(1, 0, 0), color, 24);
            draw_circle_3d(pos, probe.influence_radius, glm::vec3(0, 1, 0), color, 24);
            draw_circle_3d(pos, probe.influence_radius, glm::vec3(0, 0, 1), color, 24);
        }

        // Center dot
        ImVec2 center_screen = project(pos);
        if (center_screen.x > -9000) {
            draw_list->AddCircleFilled(center_screen, 5.0f, IM_COL32(80, 180, 255, 220));
            draw_list->AddText(ImVec2(center_screen.x + 8, center_screen.y - 8),
                               IM_COL32(80, 180, 255, 200), "RP");
        }
    }
}

// --- Scene Gizmo (top-right 3D axis indicator) ---
void DrawSceneGizmo(ImDrawList* draw_list,
                    const ImVec2& viewport_pos,
                    const ImVec2& viewport_size,
                    const glm::mat4& view) {
    const float gizmo_size = 45.0f;
    const float margin = 18.0f;
    // Center of the gizmo widget
    ImVec2 center(viewport_pos.x + viewport_size.x - gizmo_size - margin,
                  viewport_pos.y + margin + gizmo_size);

    // Extract rotation-only part of view matrix (upper-left 3x3)
    glm::mat3 rot(view);

    // World axes projected through camera rotation
    struct AxisInfo { glm::vec3 dir; ImU32 color; const char* label; float yaw; float pitch; };
    AxisInfo axes[3] = {
        { {1,0,0}, IM_COL32(220, 60, 60, 255),  "X",  glm::radians(90.0f),  0.0f },           // Right
        { {0,1,0}, IM_COL32(100, 200, 60, 255),  "Y",  0.0f,                  glm::radians(89.0f) }, // Top
        { {0,0,1}, IM_COL32(60, 120, 255, 255),  "Z",  0.0f,                  0.0f },           // Front
    };

    // Sort by projected depth (draw back-to-front)
    struct Projected { int idx; glm::vec3 screen; float depth; };
    Projected proj[3];
    for (int i = 0; i < 3; ++i) {
        glm::vec3 transformed = rot * axes[i].dir;
        proj[i] = { i, transformed, transformed.z };
    }
    // Simple bubble sort for 3 elements (back first = smaller z)
    for (int i = 0; i < 2; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (proj[i].depth > proj[j].depth) std::swap(proj[i], proj[j]);

    // Background circle
    draw_list->AddCircleFilled(center, gizmo_size + 4.0f, IM_COL32(30, 30, 30, 160), 32);
    draw_list->AddCircle(center, gizmo_size + 4.0f, IM_COL32(80, 80, 80, 120), 32, 1.5f);

    EditorCamera& cam = GetEditorCamera();
    ImVec2 mouse = ImGui::GetMousePos();

    for (int i = 0; i < 3; ++i) {
        int ai = proj[i].idx;
        glm::vec3 s = proj[i].screen;
        // Screen endpoint: x goes right, y goes up (ImGui y is down so negate)
        ImVec2 end(center.x + s.x * gizmo_size, center.y - s.y * gizmo_size);

        // Fade lines that point away from camera
        float alpha_factor = 0.4f + 0.6f * std::max(0.0f, std::min(1.0f, (s.z + 1.0f) * 0.5f));
        ImU32 col = axes[ai].color;
        int r = (col >> 0) & 0xFF, g = (col >> 8) & 0xFF, b = (col >> 16) & 0xFF;
        ImU32 line_col = IM_COL32((int)(r * alpha_factor), (int)(g * alpha_factor), (int)(b * alpha_factor), 255);

        draw_list->AddLine(center, end, line_col, 2.5f);

        // Axis tip circle + label
        float tip_radius = 10.0f;
        bool hovered = false;
        float dx = mouse.x - end.x, dy = mouse.y - end.y;
        if (dx * dx + dy * dy <= tip_radius * tip_radius + 25.0f) hovered = true;

        ImU32 tip_col = hovered ? IM_COL32(255, 255, 100, 255) : line_col;
        draw_list->AddCircleFilled(end, tip_radius, tip_col, 16);

        // Label text
        ImVec2 text_size = ImGui::CalcTextSize(axes[ai].label);
        draw_list->AddText(ImVec2(end.x - text_size.x * 0.5f, end.y - text_size.y * 0.5f),
                           IM_COL32(255, 255, 255, 255), axes[ai].label);

        // Click to snap view
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            cam.yaw = axes[ai].yaw;
            cam.pitch = axes[ai].pitch;
        }
    }
}

} // namespace

void DrawSceneViewportPanel(EditorContext& ctx,
                            unsigned int scene_texture_id,
                            bool (*build_active_camera_matrices)(entt::registry&, float, glm::mat4&, glm::mat4&)) {
    // 兼容旧引用名
    auto& context = ctx;
    int& current_gizmo_operation = ctx.current_gizmo_operation;
    int current_gizmo_mode = ctx.current_gizmo_mode;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene");
    ImVec2 scene_panel_size = ImGui::GetContentRegionAvail();
    ImVec2 window_pos = ImGui::GetWindowPos();

    // Tilemap / Terrain paint handling (must run before entity picking to consume clicks)
    bool paint_consumed = false;
    if (ImGui::IsWindowHovered()) {
        const float tm_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
        EditorCamera& tm_cam = GetEditorCamera();
        glm::mat4 tm_view = tm_cam.GetViewMatrix();
        glm::mat4 tm_proj = tm_cam.GetProjectionMatrix(tm_aspect);
        paint_consumed = HandleTilemapViewportPaint(
            context.registry,
            glm::vec2(window_pos.x, window_pos.y),
            glm::vec2(scene_panel_size.x, scene_panel_size.y),
            tm_view, tm_proj);
        if (!paint_consumed) {
            paint_consumed = HandleTerrainViewportSculpt(
                context.registry,
                glm::vec2(window_pos.x, window_pos.y),
                glm::vec2(scene_panel_size.x, scene_panel_size.y),
                tm_view, tm_proj,
                ImGui::GetIO().DeltaTime);
        }
    }

    // --- Marquee Selection + Point Picking ---
    {
        auto& marquee = GetMarqueeState();
        ImVec2 mouse_pos = ImGui::GetMousePos();
        bool can_pick = !paint_consumed && ImGui::IsWindowHovered() && !ImGuizmo::IsOver();

        // Start marquee drag
        if (can_pick && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            marquee.active = true;
            marquee.start_pos = glm::vec2(mouse_pos.x, mouse_pos.y);
            marquee.end_pos = marquee.start_pos;
        }

        // Update marquee drag
        if (marquee.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            marquee.end_pos = glm::vec2(mouse_pos.x, mouse_pos.y);
        }

        // Release: either point-pick or marquee-select
        if (marquee.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            marquee.active = false;
            glm::vec2 delta = marquee.end_pos - marquee.start_pos;
            float drag_dist = glm::length(delta);

            const float pick_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            EditorCamera& pick_cam = GetEditorCamera();
            glm::mat4 pick_view = pick_cam.GetViewMatrix();
            glm::mat4 pick_proj = pick_cam.GetProjectionMatrix(pick_aspect);

            if (drag_dist < 5.0f) {
                // Point pick
                entt::entity picked = GetColorIDPicker().Pick(
                    context.registry,
                    glm::vec2(mouse_pos.x, mouse_pos.y),
                    glm::vec2(window_pos.x, window_pos.y),
                    glm::vec2(scene_panel_size.x, scene_panel_size.y),
                    pick_view, pick_proj);
                if (picked != entt::null) {
                    context.selected_entity = picked;
                }
            } else {
                // Marquee select: find all entities whose screen-space projection falls within the rect
                glm::vec2 rect_min(std::min(marquee.start_pos.x, marquee.end_pos.x),
                                   std::min(marquee.start_pos.y, marquee.end_pos.y));
                glm::vec2 rect_max(std::max(marquee.start_pos.x, marquee.end_pos.x),
                                   std::max(marquee.start_pos.y, marquee.end_pos.y));
                glm::mat4 vp = pick_proj * pick_view;
                bool additive = ImGui::GetIO().KeyCtrl;
                auto& sel = SelectionManager::Get();
                if (!additive) sel.Clear();

                for (auto entity : context.registry.storage<entt::entity>()) {
                    if (!context.registry.valid(entity)) continue;
                    if (!context.registry.all_of<TransformComponent>(entity)) continue;
                    auto& tf = context.registry.get<TransformComponent>(entity);
                    glm::vec4 clip = vp * glm::vec4(tf.position, 1.0f);
                    if (clip.w <= 0.001f) continue;
                    glm::vec3 ndc = glm::vec3(clip) / clip.w;
                    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) continue;
                    float sx = window_pos.x + (ndc.x * 0.5f + 0.5f) * scene_panel_size.x;
                    float sy = window_pos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * scene_panel_size.y;
                    if (sx >= rect_min.x && sx <= rect_max.x && sy >= rect_min.y && sy <= rect_max.y) {
                        sel.Add(entity);
                        context.selected_entity = entity; // set last selected
                    }
                }
            }
        }

        // Draw marquee rectangle overlay
        if (marquee.active) {
            float drag_dist_current = glm::length(marquee.end_pos - marquee.start_pos);
            if (drag_dist_current >= 5.0f) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 r_min(std::min(marquee.start_pos.x, marquee.end_pos.x),
                             std::min(marquee.start_pos.y, marquee.end_pos.y));
                ImVec2 r_max(std::max(marquee.start_pos.x, marquee.end_pos.x),
                             std::max(marquee.start_pos.y, marquee.end_pos.y));
                dl->AddRectFilled(r_min, r_max, IM_COL32(71, 143, 255, 40));
                dl->AddRect(r_min, r_max, IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.5f);
            }
        }
    }

    // Process editor camera input while Scene window is active
    ProcessEditorCameraInput(GetEditorCamera());

    if (scene_texture_id != 0) {
        ImGui::Image((ImTextureID)(intptr_t)scene_texture_id, scene_panel_size, ImVec2(0, 1), ImVec2(1, 0));

        // Tilemap grid overlay + Terrain brush overlay
        {
            const float ov_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            EditorCamera& ov_cam = GetEditorCamera();
            glm::mat4 ov_view = ov_cam.GetViewMatrix();
            glm::mat4 ov_proj = ov_cam.GetProjectionMatrix(ov_aspect);
            // Draw ground grid
            if (GetShowGrid()) {
                DrawViewportGrid(
                    ImGui::GetWindowDrawList(),
                    glm::vec2(window_pos.x, window_pos.y),
                    glm::vec2(scene_panel_size.x, scene_panel_size.y),
                    ov_view, ov_proj);
            }

            DrawTilemapGridOverlay(
                context.registry,
                glm::vec2(window_pos.x, window_pos.y),
                glm::vec2(scene_panel_size.x, scene_panel_size.y),
                ov_view, ov_proj);
            DrawTerrainBrushOverlay(
                context.registry,
                glm::vec2(window_pos.x, window_pos.y),
                glm::vec2(scene_panel_size.x, scene_panel_size.y),
                ov_view, ov_proj);
            DrawAudioRangeOverlay(
                context.registry,
                context.selected_entity,
                glm::vec2(window_pos.x, window_pos.y),
                glm::vec2(scene_panel_size.x, scene_panel_size.y),
                ov_view, ov_proj);
            DrawPhysicsColliderOverlay(
                context.registry,
                context.selected_entity,
                ImGui::GetWindowDrawList(),
                glm::vec2(window_pos.x, window_pos.y),
                glm::vec2(scene_panel_size.x, scene_panel_size.y),
                ov_view, ov_proj);
            DrawProbeOverlays(
                context.registry,
                glm::vec2(window_pos.x, window_pos.y),
                glm::vec2(scene_panel_size.x, scene_panel_size.y),
                ov_view, ov_proj);

            // Scene Gizmo (top-right axis indicator)
            DrawSceneGizmo(ImGui::GetWindowDrawList(), window_pos, scene_panel_size, ov_view);
        }

        // --- Gizmo: Multi-select or Single-select ---
        auto& selection = SelectionManager::Get();
        const bool is_multi = selection.IsMultiSelect();

        if (is_multi) {
            // Multi-select gizmo: compute centroid, translate all selected entities
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(window_pos.x, window_pos.y, scene_panel_size.x, scene_panel_size.y);

            const float panel_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            EditorCamera& editor_cam = GetEditorCamera();
            glm::mat4 view_matrix = editor_cam.GetViewMatrix();
            glm::mat4 proj_matrix = editor_cam.GetProjectionMatrix(panel_aspect);

            // Compute centroid of all selected entities
            glm::vec3 centroid(0.0f);
            int count = 0;
            for (auto ent : selection.GetAll()) {
                if (context.registry.valid(ent) && context.registry.all_of<TransformComponent>(ent)) {
                    centroid += context.registry.get<TransformComponent>(ent).position;
                    count++;
                }
            }
            if (count > 0) centroid /= static_cast<float>(count);

            glm::mat4 centroid_matrix = glm::translate(glm::mat4(1.0f), centroid);

            ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;
            if (current_gizmo_operation == 0) gizmo_op = ImGuizmo::TRANSLATE;
            else if (current_gizmo_operation == 1) gizmo_op = ImGuizmo::TRANSLATE; // multi-select only supports translate
            else if (current_gizmo_operation == 2) gizmo_op = ImGuizmo::TRANSLATE;

            ImGuizmo::MODE gizmo_mode = ImGuizmo::WORLD;

            if (ImGui::IsKeyPressed(ImGuiKey_W)) current_gizmo_operation = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) current_gizmo_operation = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) current_gizmo_operation = 2;

            // Snap support (Ctrl held)
            float snap_values[3] = {0};
            float* snap_ptr = nullptr;
            if (ImGui::GetIO().KeyCtrl) {
                float snap_val = GetSnapTranslate();
                snap_values[0] = snap_val;
                snap_values[1] = snap_val;
                snap_values[2] = snap_val;
                snap_ptr = snap_values;
            }

            if (count > 0 && current_gizmo_operation != -1 &&
                ImGuizmo::Manipulate(glm::value_ptr(view_matrix), glm::value_ptr(proj_matrix),
                                     gizmo_op, gizmo_mode, glm::value_ptr(centroid_matrix), nullptr, snap_ptr)) {
                glm::vec3 new_centroid_pos, dummy_rot, dummy_scale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(centroid_matrix),
                                                      glm::value_ptr(new_centroid_pos),
                                                      glm::value_ptr(dummy_rot),
                                                      glm::value_ptr(dummy_scale));
                glm::vec3 delta = new_centroid_pos - centroid;
                for (auto ent : selection.GetAll()) {
                    if (context.registry.valid(ent) && context.registry.all_of<TransformComponent>(ent)) {
                        auto& t = context.registry.get<TransformComponent>(ent);
                        t.position += delta;
                        t.dirty = true;
                    }
                }
            }

            // Multi-gizmo Undo/Redo tracking
            auto& mg_state = GetMultiGizmoUndoState();
            const bool is_using = ImGuizmo::IsUsing();
            if (is_using && !mg_state.was_using) {
                mg_state.initial_centroid = centroid;
                mg_state.snapshots.clear();
                for (auto ent : selection.GetAll()) {
                    if (context.registry.valid(ent) && context.registry.all_of<TransformComponent>(ent)) {
                        auto& t = context.registry.get<TransformComponent>(ent);
                        mg_state.snapshots.push_back({ent, t.position, t.rotation, t.scale});
                    }
                }
            } else if (!is_using && mg_state.was_using) {
                // Capture final positions and push undo
                std::vector<MultiGizmoUndoState::EntitySnapshot> before = mg_state.snapshots;
                std::vector<MultiGizmoUndoState::EntitySnapshot> after;
                for (auto& snap : before) {
                    if (context.registry.valid(snap.entity) && context.registry.all_of<TransformComponent>(snap.entity)) {
                        auto& t = context.registry.get<TransformComponent>(snap.entity);
                        after.push_back({snap.entity, t.position, t.rotation, t.scale});
                    }
                }
                if (!after.empty()) {
                    auto& undo_mgr = GetUndoRedoManager();
                    auto cmd = std::make_unique<LambdaCommand>(
                        "Gizmo Multi Transform",
                        [&reg = context.registry, after]() {
                            for (auto& s : after) {
                                if (reg.valid(s.entity) && reg.all_of<TransformComponent>(s.entity)) {
                                    auto& t = reg.get<TransformComponent>(s.entity);
                                    t.position = s.position;
                                    t.rotation = s.rotation;
                                    t.scale = s.scale;
                                    t.dirty = true;
                                }
                            }
                        },
                        [&reg = context.registry, before]() {
                            for (auto& s : before) {
                                if (reg.valid(s.entity) && reg.all_of<TransformComponent>(s.entity)) {
                                    auto& t = reg.get<TransformComponent>(s.entity);
                                    t.position = s.position;
                                    t.rotation = s.rotation;
                                    t.scale = s.scale;
                                    t.dirty = true;
                                }
                            }
                        },
                        "gizmo_multi_transform"
                    );
                    undo_mgr.Execute(std::move(cmd), true);
                }
            }
            mg_state.was_using = is_using;

        } else if (context.selected_entity != entt::null && context.registry.all_of<TransformComponent>(context.selected_entity)) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(window_pos.x, window_pos.y, scene_panel_size.x, scene_panel_size.y);

            const float panel_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            EditorCamera& editor_cam = GetEditorCamera();
            glm::mat4 view_matrix = editor_cam.GetViewMatrix();
            glm::mat4 proj_matrix = editor_cam.GetProjectionMatrix(panel_aspect);

            auto& transform = context.registry.get<TransformComponent>(context.selected_entity);
            glm::mat4 model_matrix = transform.local_to_world;

            ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;
            if (current_gizmo_operation == 0) gizmo_op = ImGuizmo::TRANSLATE;
            else if (current_gizmo_operation == 1) gizmo_op = ImGuizmo::ROTATE;
            else if (current_gizmo_operation == 2) gizmo_op = ImGuizmo::SCALE;

            ImGuizmo::MODE gizmo_mode = (current_gizmo_mode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

            if (ImGui::IsKeyPressed(ImGuiKey_W)) current_gizmo_operation = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) current_gizmo_operation = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) current_gizmo_operation = 2;

            // Snap support (Ctrl held)
            float snap_values_single[3] = {0};
            float* snap_ptr_single = nullptr;
            if (ImGui::GetIO().KeyCtrl) {
                float snap_val = 0.0f;
                if (current_gizmo_operation == 0) snap_val = GetSnapTranslate();
                else if (current_gizmo_operation == 1) snap_val = GetSnapRotate();
                else if (current_gizmo_operation == 2) snap_val = GetSnapScale();
                snap_values_single[0] = snap_val;
                snap_values_single[1] = snap_val;
                snap_values_single[2] = snap_val;
                snap_ptr_single = snap_values_single;
            }

            if (current_gizmo_operation != -1 && ImGuizmo::Manipulate(glm::value_ptr(view_matrix), glm::value_ptr(proj_matrix), gizmo_op, gizmo_mode, glm::value_ptr(model_matrix), nullptr, snap_ptr_single)) {
                glm::vec3 translation, rotation, scale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model_matrix), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));
                transform.position = translation;
                transform.rotation = glm::quat(glm::radians(rotation));
                transform.scale = scale;
                transform.dirty = true;
            }

            // Gizmo Undo/Redo tracking
            auto& gizmo_state = GetGizmoUndoState();
            const bool is_using = ImGuizmo::IsUsing();
            if (is_using && !gizmo_state.was_using) {
                gizmo_state.entity = context.selected_entity;
                gizmo_state.initial_position = transform.position;
                gizmo_state.initial_rotation = transform.rotation;
                gizmo_state.initial_scale = transform.scale;
            } else if (!is_using && gizmo_state.was_using) {
                if (gizmo_state.entity == context.selected_entity && context.registry.valid(gizmo_state.entity)) {
                    const glm::vec3 final_pos = transform.position;
                    const glm::quat final_rot = transform.rotation;
                    const glm::vec3 final_scale = transform.scale;
                    const glm::vec3 old_pos = gizmo_state.initial_position;
                    const glm::quat old_rot = gizmo_state.initial_rotation;
                    const glm::vec3 old_scale = gizmo_state.initial_scale;
                    entt::entity ent = gizmo_state.entity;

                    auto& undo_mgr = GetUndoRedoManager();
                    std::string merge_id = "gizmo_transform_" + std::to_string(static_cast<uint32_t>(ent));
                    auto cmd = std::make_unique<LambdaCommand>(
                        "Gizmo Transform",
                        [&reg = context.registry, ent, final_pos, final_rot, final_scale]() {
                            if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                                auto& t = reg.get<TransformComponent>(ent);
                                t.position = final_pos;
                                t.rotation = final_rot;
                                t.scale = final_scale;
                                t.dirty = true;
                            }
                        },
                        [&reg = context.registry, ent, old_pos, old_rot, old_scale]() {
                            if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
                                auto& t = reg.get<TransformComponent>(ent);
                                t.position = old_pos;
                                t.rotation = old_rot;
                                t.scale = old_scale;
                                t.dirty = true;
                            }
                        },
                        merge_id
                    );
                    undo_mgr.Execute(std::move(cmd), true);
                }
            }
            gizmo_state.was_using = is_using;
        }
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + scene_panel_size.x, p_min.y + scene_panel_size.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
        for (float i = 0; i < scene_panel_size.x; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x + i, p_min.y), ImVec2(p_min.x + i, p_max.y), IM_COL32(60, 60, 60, 255));
        }
        for (float i = 0; i < scene_panel_size.y; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x, p_min.y + i), ImVec2(p_max.x, p_min.y + i), IM_COL32(60, 60, 60, 255));
        }
        draw_list->AddText(ImVec2(p_min.x + scene_panel_size.x / 2 - 50, p_min.y + scene_panel_size.y / 2), IM_COL32(200, 200, 200, 255), "Scene View");
    }

    // ─── Drag & Drop from Project panel ─────────────────────────────────────────
    if (!ctx.read_only && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string rel_path(static_cast<const char*>(payload->Data));
            std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

            // Ray-cast drop position onto Y=0 world plane
            ImVec2 mouse_pos = ImGui::GetMousePos();
            float nx = ((mouse_pos.x - window_pos.x) / scene_panel_size.x) * 2.0f - 1.0f;
            float ny = 1.0f - ((mouse_pos.y - window_pos.y) / scene_panel_size.y) * 2.0f;
            const float drop_aspect = scene_panel_size.y > 0.0f
                ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            EditorCamera& drop_cam = GetEditorCamera();
            glm::mat4 drop_view = drop_cam.GetViewMatrix();
            glm::mat4 drop_proj = drop_cam.GetProjectionMatrix(drop_aspect);
            glm::mat4 inv_vp    = glm::inverse(drop_proj * drop_view);
            glm::vec4 near4     = inv_vp * glm::vec4(nx, ny, -1.0f, 1.0f);
            glm::vec4 far4      = inv_vp * glm::vec4(nx, ny,  1.0f, 1.0f);
            glm::vec3 ray_origin = glm::vec3(near4) / near4.w;
            glm::vec3 ray_dir    = glm::normalize(glm::vec3(far4) / far4.w - ray_origin);
            // Intersect with Y=0 plane; fallback to focal_point if ray is parallel
            glm::vec3 drop_pos = drop_cam.focal_point;
            if (std::abs(ray_dir.y) > 1e-4f) {
                float t = -ray_origin.y / ray_dir.y;
                if (t > 0.0f) drop_pos = ray_origin + t * ray_dir;
            }

            // Determine asset type and create appropriate entity
            const auto* info = AssetDatabase::Get().FindByPath(rel_path);
            AssetType atype = info ? info->type : AssetTypeFromExtension(
                std::filesystem::path(rel_path).extension().string());

            if (atype == AssetType::Mesh) {
                auto ent = ctx.world.CreateEntity();
                std::string stem = std::filesystem::path(rel_path).stem().string();
                ctx.registry.emplace<EditorNameComponent>(ent, stem);
                auto& tf = ctx.registry.emplace<TransformComponent>(ent);
                tf.position = drop_pos;
                tf.dirty = true;
                auto& mesh = ctx.registry.emplace<MeshRendererComponent>(ent);
                mesh.mesh_path = rel_path;
                mesh.shader_variant = "MESH_LIT";
                SelectionManager::Get().SetSingle(ent);
                ctx.selected_entity = ent;
                EditorLog(LogLevel::Info, "Dropped mesh: " + rel_path);
            } else if (atype == AssetType::Script) {
                auto ent = ctx.world.CreateEntity();
                std::string stem = std::filesystem::path(rel_path).stem().string();
                ctx.registry.emplace<EditorNameComponent>(ent, stem);
                auto& tf = ctx.registry.emplace<TransformComponent>(ent);
                tf.position = drop_pos;
                tf.dirty = true;
                auto& script = ctx.registry.emplace<LuaScriptComponent>(ent);
                script.script_path = rel_path;
                SelectionManager::Get().SetSingle(ent);
                ctx.selected_entity = ent;
                EditorLog(LogLevel::Info, "Dropped script: " + rel_path);
            } else {
                EditorLog(LogLevel::Warning,
                    "Viewport drop: unsupported asset type for '" + rel_path + "'");
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─── Physics Collider Wireframe Overlay ─────────────────────────────────────

namespace {

inline bool W2S(const glm::vec3& world, const glm::mat4& vp,
                const glm::vec2& vp_pos, const glm::vec2& vp_size, ImVec2& out) {
    glm::vec4 clip = vp * glm::vec4(world, 1.0f);
    if (clip.w <= 0.001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.5f || ndc.x > 1.5f || ndc.y < -1.5f || ndc.y > 1.5f) return false;
    out = ImVec2(vp_pos.x + (ndc.x * 0.5f + 0.5f) * vp_size.x,
                 vp_pos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vp_size.y);
    return true;
}

void DrawBox(ImDrawList* dl, const glm::mat4& vp,
             const glm::vec2& vp_pos, const glm::vec2& vp_size,
             const glm::mat4& model, const glm::vec3& half, ImU32 col) {
    static const int edges[12][2] = {
        {0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
    glm::vec3 corners[8];
    int idx = 0;
    for (int x : {-1,1}) for (int y : {-1,1}) for (int z : {-1,1})
        corners[idx++] = glm::vec3(model * glm::vec4(half.x*x, half.y*y, half.z*z, 1.0f));
    for (auto& e : edges) {
        ImVec2 a, b;
        if (W2S(corners[e[0]], vp, vp_pos, vp_size, a) &&
            W2S(corners[e[1]], vp, vp_pos, vp_size, b))
            dl->AddLine(a, b, col, 1.5f);
    }
}

void DrawCircle3D(ImDrawList* dl, const glm::mat4& vp,
                  const glm::vec2& vp_pos, const glm::vec2& vp_size,
                  const glm::vec3& center, const glm::vec3& axisA, const glm::vec3& axisB,
                  float r, ImU32 col, int segs = 32) {
    ImVec2 prev; bool prev_ok = false;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / (float)segs * 6.28318530f;
        glm::vec3 p = center + axisA * (r * std::cos(a)) + axisB * (r * std::sin(a));
        ImVec2 s; bool ok = W2S(p, vp, vp_pos, vp_size, s);
        if (ok && prev_ok) dl->AddLine(prev, s, col, 1.5f);
        prev = s; prev_ok = ok;
    }
}

} // anon namespace

void DrawPhysicsColliderOverlay(
    entt::registry& registry,
    entt::entity selected,
    ImDrawList* dl,
    const glm::vec2& vp_pos, const glm::vec2& vp_size,
    const glm::mat4& view, const glm::mat4& proj)
{
    if (!dl) return;
    glm::mat4 vp_mat = proj * view;

    auto draw_one = [&](entt::entity e) {
        if (!registry.valid(e)) return;
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 world_pos(0.0f);
        glm::quat world_rot(1.0f,0.0f,0.0f,0.0f);
        glm::vec3 world_scale(1.0f);
        if (registry.all_of<TransformComponent>(e)) {
            auto& tf = registry.get<TransformComponent>(e);
            world_pos   = tf.position;
            world_rot   = tf.rotation;
            world_scale = tf.scale;
            model = tf.local_to_world;
        }

        if (registry.all_of<BoxCollider3DComponent>(e)) {
            auto& bc = registry.get<BoxCollider3DComponent>(e);
            ImU32 col = bc.is_trigger ? IM_COL32(255,200,0,220) : IM_COL32(100,230,100,220);
            glm::mat4 m = glm::translate(model, bc.center);
            glm::vec3 half = bc.size * 0.5f * world_scale;
            glm::mat4 rot_m = glm::mat4_cast(world_rot);
            glm::mat4 box_m = glm::translate(glm::mat4(1.0f), world_pos + glm::vec3(rot_m * glm::vec4(bc.center, 0.0f)))
                            * rot_m;
            DrawBox(dl, vp_mat, vp_pos, vp_size, box_m, half, col);
        }

        if (registry.all_of<SphereCollider3DComponent>(e)) {
            auto& sc = registry.get<SphereCollider3DComponent>(e);
            ImU32 col = sc.is_trigger ? IM_COL32(255,200,0,220) : IM_COL32(100,230,100,220);
            float r = sc.radius * std::max({world_scale.x, world_scale.y, world_scale.z});
            glm::vec3 c = world_pos + glm::vec3(glm::mat4_cast(world_rot) * glm::vec4(sc.center, 0.0f));
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, c, {1,0,0}, {0,1,0}, r, col);
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, c, {1,0,0}, {0,0,1}, r, col);
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, c, {0,1,0}, {0,0,1}, r, col);
        }

        if (registry.all_of<CapsuleCollider3DComponent>(e)) {
            auto& cc = registry.get<CapsuleCollider3DComponent>(e);
            ImU32 col = cc.is_trigger ? IM_COL32(255,200,0,220) : IM_COL32(100,230,100,220);
            float r = cc.radius * std::max(world_scale.x, world_scale.z);
            float hh = cc.height * 0.5f * world_scale.y;
            glm::mat4 rot_m = glm::mat4_cast(world_rot);
            glm::vec3 c = world_pos + glm::vec3(rot_m * glm::vec4(cc.center, 0.0f));
            glm::vec3 up   = cc.direction==1 ? glm::vec3(rot_m[1]) :
                             cc.direction==0 ? glm::vec3(rot_m[0]) : glm::vec3(rot_m[2]);
            glm::vec3 side = cc.direction==1 ? glm::vec3(rot_m[0]) : glm::vec3(rot_m[1]);
            glm::vec3 fwd  = glm::normalize(glm::cross(up, side));
            glm::vec3 top_c = c + up * hh;
            glm::vec3 bot_c = c - up * hh;
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, top_c, side, fwd, r, col);
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, bot_c, side, fwd, r, col);
            for (int qi = 0; qi < 4; qi++) {
                float a = qi * 1.5707963f;
                glm::vec3 off = side * (r * std::cos(a)) + fwd * (r * std::sin(a));
                ImVec2 ta, ba;
                if (W2S(top_c + off, vp_mat, vp_pos, vp_size, ta) &&
                    W2S(bot_c + off, vp_mat, vp_pos, vp_size, ba))
                    dl->AddLine(ta, ba, col, 1.5f);
            }
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, top_c, side, up, r, col, 16);
            DrawCircle3D(dl, vp_mat, vp_pos, vp_size, bot_c, side, glm::vec3(-up.x,-up.y,-up.z), r, col, 16);
        }
    };

    // Always draw all entities with colliders; highlight selected
    for (auto e : registry.storage<entt::entity>()) {
        if (!registry.valid(e)) continue;
        bool has_collider = registry.all_of<BoxCollider3DComponent>(e) ||
                            registry.all_of<SphereCollider3DComponent>(e) ||
                            registry.all_of<CapsuleCollider3DComponent>(e);
        if (has_collider) draw_one(e);
    }
}

void DrawGameViewportPanel(unsigned int texture_id) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Game");
    ImVec2 game_panel_size = ImGui::GetContentRegionAvail();
    if (texture_id != 0) {
        ImGui::Image((ImTextureID)(intptr_t)texture_id, game_panel_size, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + game_panel_size.x, p_min.y + game_panel_size.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 20, 20, 255));
        draw_list->AddText(ImVec2(p_min.x + game_panel_size.x / 2 - 40, p_min.y + game_panel_size.y / 2), IM_COL32(150, 150, 150, 255), "Game View");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace dse::editor
