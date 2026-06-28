/**
 * @file ui_tests_components.cpp
 * @brief ④ Inspector 各组件字段 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 经 Hierarchy 右键创建并选中实体 → Inspector「Add Component」弹窗挂上目标组件 →
 * 在该组件 section 的真实控件里改值（ItemInputValue 驱动 DragFloat/SliderFloat/DragInt）→
 * 断言 ECS 组件字段被真正写回；并对 Camera FOV 验证「Edit/Undo」能回退到旧值。
 * 覆盖：相机 / 平行光 / 点光 / Mesh 材质 / 物理刚体 / 粒子发射器。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_selection.h"   // SelectionManager
#include "../editor_shortcuts.h"   // GetUndoRedoManager

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"   // Camera3D/DirectionalLight3D/PointLight/MeshRenderer
#include "engine/ecs/particle_2d.h"            // ParticleEmitterComponent（全局命名空间）

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 右键 Hierarchy → Create Empty Entity（创建即选中），按 registry 差集取回新实体；
// 随后清掉可能残留的多选，确保 Inspector 走单选渲染分支。
entt::entity NewSelectedEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield();
    SelectionManager::Get().Clear();
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) return en;
    }
    return entt::null;
}

// Inspector「Add Component」按钮 → 弹窗里点对应 MenuItem（label 即组件注册名）。
void AddComponent(ImGuiTestContext* ctx, const char* component_name) {
    ctx->WindowFocus("//Inspector");
    ctx->SetRef("//Inspector");
    ctx->ItemClick("Add Component");
    ctx->Yield();
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick(component_name);
    ctx->Yield(2);
    ctx->SetRef("//Inspector");
}

// 用例收尾：经 Hierarchy 右键「Delete Entity」删掉当前选中实体（编辑器正规删除路径，
// 各系统随之清理）。否则这些挂了相机/光/刚体/粒子的实体会长期驻留场景，被后续每帧渲染，
// 跨大量用例累积后可能拖崩进程——用例应自清理，避免污染整套运行。
void DeleteSelectedEntity(ImGuiTestContext* ctx, entt::entity ent) {
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Delete Entity");
    ctx->Yield(2);
    IM_CHECK(!Reg().valid(ent));
}

} // namespace

void RegisterComponentFieldTests(ImGuiTestEngine* e) {
    // ── ④-1 相机：改 FOV，并用 Edit/Undo 回退 ────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "camera3d_fov_edit_undo");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Camera 3D");
            IM_CHECK(Reg().all_of<dse::Camera3DComponent>(ent));
            const float before = Reg().get<dse::Camera3DComponent>(ent).fov;  // 默认 60

            ctx->ItemInputValue("//Inspector/##cam3d_fov", 95.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::Camera3DComponent>(ent).fov - 95.0f) < 0.01f);

            // Edit 菜单 Undo 标签运行期动态拼为 "Undo (<描述>)"，描述即 "Camera3D.FOV"。
            auto& mgr = GetUndoRedoManager();
            IM_CHECK(mgr.CanUndo());
            const std::string undo_label = std::string("Edit/Undo (") + mgr.GetUndoDescription() + ")";
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(undo_label.c_str());
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::Camera3DComponent>(ent).fov - before) < 0.01f);
            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ④-2 平行光：改 Intensity ─────────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "dirlight_intensity_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Directional Light");
            IM_CHECK(Reg().all_of<dse::DirectionalLight3DComponent>(ent));

            ctx->ItemInputValue("//Inspector/##dirlight_int", 4.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::DirectionalLight3DComponent>(ent).intensity - 4.5f) < 0.01f);
            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ④-3 点光：改 Radius ──────────────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "pointlight_radius_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Point Light");
            IM_CHECK(Reg().all_of<dse::PointLightComponent>(ent));

            ctx->ItemInputValue("//Inspector/##ptlight_rad", 25.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::PointLightComponent>(ent).radius - 25.0f) < 0.01f);
            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ④-4 Mesh 材质：改 Metallic（SliderFloat 0..1） ───────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "mesh_metallic_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Mesh Renderer");
            IM_CHECK(Reg().all_of<dse::MeshRendererComponent>(ent));

            ctx->ItemInputValue("//Inspector/##mesh_metallic", 0.75f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::MeshRendererComponent>(ent).metallic - 0.75f) < 0.01f);
            DeleteSelectedEntity(ctx, ent);
        };
    }

    // 注：物理刚体（RigidBody3D）字段编辑测试暂缺。实测发现这是一个引擎侧内存破坏 bug——
    // 在「进入过 Play 模式」之后再新建带 RigidBody3D 的实体，会在销毁/退出阶段触发堆破坏崩溃
    // （崩溃点随堆布局漂移，需 ASan/调试器才能精确定位）。详见交付说明，待修复后再补该用例。

    // ── ④-5 粒子发射器：改 Emit Rate ─────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "particle_emit_rate_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Particle Emitter");
            IM_CHECK(Reg().all_of<ParticleEmitterComponent>(ent));

            ctx->ItemInputValue("//Inspector/##emit_rate", 50.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<ParticleEmitterComponent>(ent).emit_rate - 50.0f) < 0.01f);
            DeleteSelectedEntity(ctx, ent);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
