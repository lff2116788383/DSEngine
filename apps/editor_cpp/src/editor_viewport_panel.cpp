#include "editor_viewport_panel.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "editor_scene_camera.h"
#include "editor_selection.h"
#include "editor_shortcuts.h"
#include "editor_tilemap_panel.h"
#include "editor_terrain_panel.h"
#include <glad/gl.h>
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

} // namespace

void DrawSceneViewportPanel(EditorViewportPanelContext& context,
                            int& current_gizmo_operation,
                            int current_gizmo_mode,
                            bool (*build_active_camera_matrices)(entt::registry&, float, glm::mat4&, glm::mat4&)) {
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

    if (!paint_consumed && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
        const float pick_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
        EditorCamera& pick_cam = GetEditorCamera();
        glm::mat4 pick_view = pick_cam.GetViewMatrix();
        glm::mat4 pick_proj = pick_cam.GetProjectionMatrix(pick_aspect);
        ImVec2 mouse_pos = ImGui::GetMousePos();

        entt::entity picked = GetColorIDPicker().Pick(
            context.registry,
            glm::vec2(mouse_pos.x, mouse_pos.y),
            glm::vec2(window_pos.x, window_pos.y),
            glm::vec2(scene_panel_size.x, scene_panel_size.y),
            pick_view, pick_proj);

        if (picked != entt::null) {
            context.selected_entity = picked;
        }
    }

    // Process editor camera input while Scene window is active
    ProcessEditorCameraInput(GetEditorCamera());

    if (context.texture_id != 0) {
        ImGui::Image((ImTextureID)(intptr_t)context.texture_id, scene_panel_size, ImVec2(0, 1), ImVec2(1, 0));

        // Tilemap grid overlay + Terrain brush overlay
        {
            const float ov_aspect = scene_panel_size.y > 0.0f ? (scene_panel_size.x / scene_panel_size.y) : 1.7777f;
            EditorCamera& ov_cam = GetEditorCamera();
            glm::mat4 ov_view = ov_cam.GetViewMatrix();
            glm::mat4 ov_proj = ov_cam.GetProjectionMatrix(ov_aspect);
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

            if (count > 0 && current_gizmo_operation != -1 &&
                ImGuizmo::Manipulate(glm::value_ptr(view_matrix), glm::value_ptr(proj_matrix),
                                     gizmo_op, gizmo_mode, glm::value_ptr(centroid_matrix))) {
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

            if (current_gizmo_operation != -1 && ImGuizmo::Manipulate(glm::value_ptr(view_matrix), glm::value_ptr(proj_matrix), gizmo_op, gizmo_mode, glm::value_ptr(model_matrix))) {
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

    ImGui::End();
    ImGui::PopStyleVar();
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
