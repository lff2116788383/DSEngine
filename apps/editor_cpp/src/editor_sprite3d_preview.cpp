/**
 * @file editor_sprite3d_preview.cpp
 * @brief HD-2D Sprite3D 独立预览视口实现（见头文件说明）
 */

#include "editor_sprite3d_preview.h"

#include "editor_context.h"
#include "editor_gpu.h"
#include "editor_icons.h"
#include "editor_panel_registry.h"
#include "editor_scene_camera.h"
#include "editor_toolbar.h"
#include "editor_viewport_panel.h"

#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/transform.h"
#include "engine/platform/screen.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/runtime/frame_pipeline.h"

#include "imgui.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace dse::editor {

namespace {

constexpr const char* kPanelId = "sprite3d_preview";
constexpr const char* kPanelTitle = "Sprite3D Preview";

/// 预览相机与面板状态。单实例：编辑器同一时刻只预览一个 Sprite3D 实体。
struct Sprite3DPreviewState {
    bool  rendered_last_frame = false;  ///< 上一帧是否真的完成了引擎重渲
    float yaw = 0.6f;                   ///< 环绕角（弧度）
    float pitch = 0.35f;                ///< 俯仰角（弧度）
    float fov_deg = 32.0f;
    float distance = 0.0f;              ///< 0 = 未初始化，按精灵尺寸自动取景
    entt::entity framed_entity = entt::null;  ///< distance 自动取景对应的实体

    unsigned int blit_rt = 0;           ///< 面板私有 RT（RHI 句柄）
    unsigned int texture = 0;           ///< blit_rt 的颜色纹理（ImGui 显示用）
    int rt_w = 0;
    int rt_h = 0;
};

Sprite3DPreviewState g_preview;

void ReleaseTargets() {
    if (g_preview.blit_rt != 0) {
        dse::editor::EditorDeleteBlitTarget(g_preview.blit_rt);
        g_preview.blit_rt = 0;
    }
    g_preview.texture = 0;
    g_preview.rt_w = 0;
    g_preview.rt_h = 0;
    g_preview.rendered_last_frame = false;
}

/// 保证私有 RT 与场景 RT 同尺寸（RHI Blit 要求源/目标一致），否则重建。
bool EnsureTargets(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (g_preview.blit_rt != 0 && g_preview.rt_w == width && g_preview.rt_h == height) {
        return g_preview.texture != 0;
    }
    ReleaseTargets();
    g_preview.blit_rt = dse::editor::EditorCreateBlitTarget(width, height);
    if (g_preview.blit_rt == 0) return false;
    g_preview.rt_w = width;
    g_preview.rt_h = height;
    g_preview.texture = dse::editor::EditorRenderTargetColorTexture(g_preview.blit_rt);
    return g_preview.texture != 0;
}

bool* PanelVisible() {
    PanelEntry* entry = PanelRegistry::Get().Find(kPanelId);
    return entry ? entry->visible : nullptr;
}

/// 预览目标：优先用当前选中实体；选中项不是 Sprite3D 时退化为场景中第一个
/// 具备 Sprite3D + Transform 的实体，这样打开面板就能直接看到画面，而不必先
/// 去层级面板里挑一个目标。
entt::entity ResolvePreviewEntity(entt::registry& registry, entt::entity selected) {
    if (registry.valid(selected) &&
        registry.all_of<Sprite3DComponent, TransformComponent>(selected)) {
        return selected;
    }
    auto view = registry.view<Sprite3DComponent, TransformComponent>();
    for (auto e : view) return e;
    return entt::null;
}

/// 推进所选精灵的帧动画（Edit 模式下没有 gameplay tick，编辑器得自己驱动）。
void AdvanceSpriteAnimation(Sprite3DComponent& sprite, float dt) {
    if (!sprite.anim_playing || sprite.clip_uvs.empty() || sprite.anim_fps <= 0.0f) return;
    const int count = static_cast<int>(sprite.clip_uvs.size());
    sprite.anim_time += dt;
    int frame = static_cast<int>(sprite.anim_time * sprite.anim_fps);
    if (sprite.anim_loop) {
        frame %= count;
        if (frame < 0) frame += count;
    } else if (frame >= count - 1) {
        frame = count - 1;
        sprite.anim_playing = false;  // 非循环播完即停，与运行时语义一致
    }
    sprite.anim_frame = frame;
    sprite.uv_rect = sprite.clip_uvs[static_cast<size_t>(frame)];
}

}  // namespace

const char* Sprite3DPreviewPanelId() { return kPanelId; }

void OpenSprite3DPreview() {
    if (bool* visible = PanelVisible()) *visible = true;
}

void ReleaseSprite3DPreviewTargets() { ReleaseTargets(); }

void RenderSprite3DPreviewViewport(FramePipeline* pipeline,
                                   entt::registry& registry,
                                   entt::entity selected) {
    g_preview.rendered_last_frame = false;

    const bool* visible = PanelVisible();
    if (visible == nullptr || !*visible) {
        // 关闭时立刻归还全屏尺寸的 RT，避免长期占用显存。
        ReleaseTargets();
        return;
    }
    if (!pipeline) return;

    // Play/Pause 下引擎用游戏相机渲染，重渲恢复会用到编辑器相机矩阵，语义不成立：
    // 此时保留最后一帧画面，只更新控件。
    if (GetEditorState() != EditorState::Edit) return;

    const entt::entity target_entity = ResolvePreviewEntity(registry, selected);
    if (target_entity == entt::null) return;
    auto& sprite = registry.get<Sprite3DComponent>(target_entity);
    const auto& transform = registry.get<TransformComponent>(target_entity);
    AdvanceSpriteAnimation(sprite, ImGui::GetIO().DeltaTime);

    const int width = Screen::render_width();
    const int height = Screen::render_height();
    if (width <= 0 || height <= 0) return;
    if (!EnsureTargets(width, height)) return;
    if (!pipeline->GetSceneRenderTarget()) return;

    const glm::vec3 target = glm::vec3(transform.local_to_world[3]);
    if (g_preview.framed_entity != target_entity || g_preview.distance <= 0.0f) {
        g_preview.framed_entity = target_entity;
        // 让精灵竖边约占视口高度的一半：d ≈ h / (2·tan(fov/2)) / 0.5
        const float h = std::max(sprite.size_h, 0.05f);
        const float half_fov = glm::radians(g_preview.fov_deg) * 0.5f;
        g_preview.distance = h / (2.0f * std::tan(half_fov)) * 2.2f;
    }

    const float cp = std::cos(g_preview.pitch);
    const glm::vec3 offset(std::sin(g_preview.yaw) * cp,
                           std::sin(g_preview.pitch),
                           std::cos(g_preview.yaw) * cp);
    const glm::vec3 eye = target + offset * g_preview.distance;
    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspective(
        glm::radians(g_preview.fov_deg),
        static_cast<float>(width) / static_cast<float>(height),
        0.05f, 500.0f);

    if (pipeline->RenderSceneWithCamera(view, proj) == 0) return;

    dse::editor::EditorBlitRenderTarget(pipeline->GetSceneRenderTarget().raw(),
                                        g_preview.blit_rt);
    g_preview.texture = dse::editor::EditorRenderTargetColorTexture(g_preview.blit_rt);
    g_preview.rendered_last_frame = (g_preview.texture != 0);

    // 关键：预览重渲覆盖了场景 RT，必须用编辑器相机再渲一次把主视口画面还原。
    // RenderSceneWithCamera 自身会保存/恢复管线内的相机状态，这里只需提供矩阵。
    const EditorCamera& cam = GetEditorCamera();
    pipeline->RenderSceneWithCamera(cam.GetViewMatrix(),
                                    cam.GetProjectionMatrix(GetCachedSceneViewportAspect()));
}

void DrawSprite3DPreviewPanel(EditorContext& ctx) {
    bool* open = PanelRegistry::Get().GetCurrentPanelOpen();
    ImGui::SetNextWindowSize(ImVec2(560.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(kPanelTitle, open)) {
        ImGui::End();
        return;
    }
    if (open && !*open) {  // 用户点了标题栏 ×：本帧结束后由渲染钩子释放 RT
        ImGui::End();
        return;
    }

    const entt::entity e = ResolvePreviewEntity(ctx.registry, ctx.selected_entity);
    if (e == entt::null) {
        ImGui::TextUnformatted("No Sprite3D entity in the scene.");
        ImGui::TextUnformatted("Add a Sprite3D component (Inspector) to preview it here.");
        ImGui::End();
        return;
    }
    auto& sprite = ctx.registry.get<dse::Sprite3DComponent>(e);

    // ─── 视口画布 ───────────────────────────────────────────────────────
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float controls_height = 152.0f;
    ImVec2 canvas(avail.x, std::max(180.0f, avail.y - controls_height));
    ImGui::BeginChild("##sprite3d_preview_canvas", canvas, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 输入层先占位：图像本身不参与命中测试，按钮覆盖整个画布即可。
    ImGui::InvisibleButton("##sprite3d_preview_input", canvas_size,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    if (active) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            g_preview.yaw -= delta.x * 0.01f;
            g_preview.pitch = std::clamp(g_preview.pitch + delta.y * 0.01f, -1.45f, 1.45f);
        }
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        g_preview.distance = std::clamp(
            g_preview.distance * (1.0f - ImGui::GetIO().MouseWheel * 0.12f), 0.05f, 500.0f);
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        g_preview.distance = 0.0f;  // 触发下一帧按尺寸重新取景
        g_preview.yaw = 0.6f;
        g_preview.pitch = 0.35f;
    }

    dl->AddRectFilled(origin, ImVec2(origin.x + canvas_size.x, origin.y + canvas_size.y),
                      IM_COL32(16, 18, 24, 255));
    if (g_preview.texture != 0) {
        const float src_aspect = (g_preview.rt_h > 0)
            ? static_cast<float>(g_preview.rt_w) / static_cast<float>(g_preview.rt_h)
            : 1.0f;
        float draw_w = canvas_size.x;
        float draw_h = draw_w / std::max(src_aspect, 0.0001f);
        if (draw_h > canvas_size.y) {
            draw_h = canvas_size.y;
            draw_w = draw_h * src_aspect;
        }
        const ImVec2 p0(origin.x + (canvas_size.x - draw_w) * 0.5f,
                        origin.y + (canvas_size.y - draw_h) * 0.5f);
        // 场景 RT 的纹理坐标在 GL/Vulkan 下是下起原点，显示时上下翻转（与主视口一致）。
        dl->AddImage((ImTextureID)EditorImGuiTextureId(g_preview.texture), p0,
                     ImVec2(p0.x + draw_w, p0.y + draw_h), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        dl->AddRect(p0, ImVec2(p0.x + draw_w, p0.y + draw_h), IM_COL32(90, 96, 110, 200));
    } else {
        const char* message = (GetEditorState() == EditorState::Edit)
            ? "Preview unavailable (engine viewport not ready)."
            : "Preview frozen: return to Edit mode to keep rendering.";
        dl->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f),
                    IM_COL32(210, 210, 215, 230), message);
    }
    dl->AddText(ImVec2(origin.x + 10.0f, origin.y + canvas_size.y - 20.0f),
                IM_COL32(170, 175, 185, 220),
                "drag = orbit   wheel = zoom   double-click = reset");
    ImGui::EndChild();

    // ─── 控件 ───────────────────────────────────────────────────────────
    ImGui::Text("Entity %u   clip '%s'   size %.2f x %.2f   billboard %d",
                static_cast<unsigned>(e), sprite.clip_name.empty() ? "-" : sprite.clip_name.c_str(),
                sprite.size_w, sprite.size_h, sprite.billboard);
    if (e != ctx.selected_entity) {
        ImGui::SameLine();
        ImGui::TextDisabled("(first Sprite3D in scene)");
    }
    ImGui::Text("UV %.3f %.3f %.3f %.3f   lit %s   %s",
                sprite.uv_rect.x, sprite.uv_rect.y, sprite.uv_rect.z, sprite.uv_rect.w,
                sprite.lit ? "on" : "off",
                g_preview.rendered_last_frame ? "rendering" : "paused");

    const int frame_count = static_cast<int>(sprite.clip_uvs.size());
    if (frame_count > 0) {
        int frame = std::clamp(sprite.anim_frame, 0, frame_count - 1);
        // 与 Inspector 时间轴共用组件的 anim_playing / anim_frame，两个 UI 不会互相打架。
        if (sprite.anim_playing) {
            if (ImGui::Button("Pause")) sprite.anim_playing = false;
        } else if (ImGui::Button("Play")) {
            sprite.anim_playing = sprite.anim_fps > 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("|<")) {
            frame = 0;
            sprite.anim_playing = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("<")) {
            frame = (frame - 1 + frame_count) % frame_count;
            sprite.anim_playing = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(">")) {
            frame = (frame + 1) % frame_count;
            sprite.anim_playing = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(">|")) {
            frame = frame_count - 1;
            sprite.anim_playing = false;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##sprite3d_preview_frame", &frame, 0, frame_count - 1,
                             "frame %d")) {
            sprite.anim_frame = frame;
            sprite.uv_rect = sprite.clip_uvs[static_cast<size_t>(frame)];
            sprite.anim_time = (sprite.anim_fps > 0.0f) ? (frame / sprite.anim_fps) : 0.0f;
            sprite.anim_playing = false;
        }
    } else {
        ImGui::TextUnformatted("No .dsprite clip loaded (Inspector → Atlas / Clip).");
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("FOV", &g_preview.fov_deg, 10.0f, 70.0f, "%.0f deg");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Distance", &g_preview.distance, 0.1f, 40.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        g_preview.distance = 0.0f;
        g_preview.yaw = 0.6f;
        g_preview.pitch = 0.35f;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Lit", &sprite.lit);

    ImGui::End();
}

}  // namespace dse::editor

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = dse::editor::Sprite3DPreviewPanelId();
    e.display_name = "Sprite3D Preview";
    e.category = "Tool";
    e.menu_icon = MDI_ICON_PALETTE;
    e.order = 96;
    e.draw = [](dse::editor::EditorContext& ctx) { dse::editor::DrawSprite3DPreviewPanel(ctx); };
    reg.Register(std::move(e));
});
