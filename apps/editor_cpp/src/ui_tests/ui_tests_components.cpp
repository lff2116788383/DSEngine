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
#include "../editor_toolbar.h"     // EnterPlayMode / ExitPlayMode / IsEditorInPlayMode

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"    // Camera3D/DirectionalLight3D/PointLight/MeshRenderer/PostProcess/Grass
#include "engine/ecs/components_3d_tree.h"      // dse::TreeComponent
#include "engine/ecs/components_3d_physics.h"   // dse::RigidBody3DComponent
#include "engine/ecs/particle_2d.h"             // ParticleEmitterComponent（全局命名空间）

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

// Inspector「Remove Component」按钮 → 弹窗里点对应 MenuItem（label 即组件注册名）。
void RemoveComponent(ImGuiTestContext* ctx, const char* component_name) {
    ctx->WindowFocus("//Inspector");
    ctx->SetRef("//Inspector");
    ctx->ItemClick("Remove Component");
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

    // ── 聚光灯：改 Intensity（DragFloat） ────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "spotlight_intensity_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Spot Light");
            IM_CHECK(Reg().all_of<dse::SpotLightComponent>(ent));

            ctx->ItemInputValue("//Inspector/##spotlight_int", 4.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::SpotLightComponent>(ent).intensity - 4.5f) < 0.01f);
            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── 天空光：改 Intensity（DragFloat） ────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "skylight_intensity_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Sky Light");
            IM_CHECK(Reg().all_of<dse::SkyLightComponent>(ent));

            ctx->ItemInputValue("//Inspector/##skylight_int", 3.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::SkyLightComponent>(ent).intensity - 3.0f) < 0.01f);
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

    // ── ④-6 物理刚体：改 Mass/Drag/Gravity Scale/Use Gravity 字段；并跑「进出 Play
    //        模式 → 再建带 RigidBody3D 的实体并删除」回归路径 ──────────────────────
    //
    // 这条路径曾触发引擎侧堆破坏崩溃：Jolt 后端在 Init 时 on_destroy<RigidBody3D>
    // 连了个处理器（destroy_connections_），但 Shutdown 里用 std::vector<entt::connection>
    // 的 clear() 并不会断开 sink（entt::connection 析构是空操作，只有 scoped_connection 才断）。
    // 于是 ExitPlayMode→ResetPhysics3D 销毁物理系统后，registry 仍持悬空 this，下次销毁任一
    // RigidBody3D 实体（还原快照清空 registry / 删实体 / 退出）即在已释放对象上调成员函数 → 堆破坏。
    // 已改 destroy_connections_ 为 scoped_connection（Shutdown.clear() 真正断开），此用例即回归守卫。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "rigidbody3d_edit_and_play_cycle");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "RigidBody 3D");
            IM_CHECK(Reg().all_of<dse::RigidBody3DComponent>(ent));

            ctx->ItemInputValue("//Inspector/##rb3d_mass", 7.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::RigidBody3DComponent>(ent).mass - 7.5f) < 0.01f);

            ctx->ItemInputValue("//Inspector/##rb3d_drag", 1.25f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::RigidBody3DComponent>(ent).drag - 1.25f) < 0.01f);

            ctx->ItemInputValue("//Inspector/##rb3d_gscale", 2.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::RigidBody3DComponent>(ent).gravity_scale - 2.0f) < 0.01f);

            const bool grav0 = Reg().get<dse::RigidBody3DComponent>(ent).use_gravity;
            ctx->ItemClick("//Inspector/##rb3d_grav");
            ctx->Yield(2);
            IM_CHECK(Reg().get<dse::RigidBody3DComponent>(ent).use_gravity == !grav0);

            // 回归：进出 Play 模式（Stop 即 ExitPlayMode→ResetPhysics3D），场景内存在
            // RigidBody3D 实体；旧 bug 会在 Stop 还原快照销毁该实体时堆破坏崩溃。
            // 直接调编辑器 Enter/ExitPlayMode（工具栏 Play/Stop 按钮内部就是调它俩）——
            // 选中态会拉起 ImGuizmo 浮层盖住 Toolbar 致按钮点不中，故走函数级真实播放路径。
            SelectionManager::Get().Clear();
            ctx->Yield();
            entt::registry& reg = Reg();
            entt::entity sel = entt::null;
            if (IsEditorInPlayMode()) { ExitPlayMode(reg, sel, Services().engine); ctx->Yield(2); }
            IM_CHECK(!IsEditorInPlayMode());

            EnterPlayMode(reg);
            ctx->Yield(4);   // 跑几帧物理：为 RigidBody3D 实体建 Jolt body
            IM_CHECK(IsEditorInPlayMode());

            ExitPlayMode(reg, sel, Services().engine);
            ctx->Yield(4);   // ResetPhysics3D + 还原快照（销毁 RigidBody3D 实体）
            IM_CHECK(!IsEditorInPlayMode());

            // 进出 Play 后再建一个带 RigidBody3D 的实体并删除——旧 bug 的第二种触发点。
            const entt::entity ent2 = NewSelectedEntity(ctx);
            IM_CHECK(ent2 != entt::null);
            AddComponent(ctx, "RigidBody 3D");
            IM_CHECK(Reg().all_of<dse::RigidBody3DComponent>(ent2));
            DeleteSelectedEntity(ctx, ent2);

            // 还原快照后 ent 以同一 id 复活但未选中；直接走世界删除清理，避免污染后续用例。
            if (Reg().valid(ent)) {
                Services().engine->pipeline()->world().DestroyEntity(ent);
                ctx->Yield();
            }
            IM_CHECK(!Reg().valid(ent));
        };
    }

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

    // ── ④-6 增删组件的 Undo/Redo（Add: 可逆 add/remove；Remove: 撤销须连数据一并恢复）──────────
    // 通过 UndoRedoManager 直接驱动 Undo/Redo（验证命令可逆性本身，避开 Edit 菜单动态标签的脆弱）；
    // Edit 菜单 → mgr.Undo/Redo 的接线已由 dse-undo / camera3d_fov_edit_undo 等用例覆盖。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "add_remove_component_undo_redo");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            auto& mgr = GetUndoRedoManager();

            // Add 组件 → 入撤销栈，组件出现，描述为 "Add Camera 3D"。
            const int u0 = mgr.GetUndoCount();
            AddComponent(ctx, "Camera 3D");
            IM_CHECK(Reg().all_of<dse::Camera3DComponent>(ent));
            IM_CHECK_EQ(mgr.GetUndoCount(), u0 + 1);
            IM_CHECK_STR_EQ(mgr.GetUndoDescription().c_str(), "Add Camera 3D");

            // 改 FOV 到特征值（便于稍后验证 Remove 的撤销恢复了数据，而非默认值）。
            ctx->ItemInputValue("//Inspector/##cam3d_fov", 77.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::Camera3DComponent>(ent).fov - 77.0f) < 0.01f);

            // 撤销 FOV、再撤销 Add → 组件消失；连做两次 Redo → 组件与 FOV 恢复（验证 Add 可逆）。
            mgr.Undo(); ctx->Yield(2);   // 撤销 FOV
            mgr.Undo(); ctx->Yield(2);   // 撤销 Add
            IM_CHECK(Reg().valid(ent));
            IM_CHECK(!Reg().all_of<dse::Camera3DComponent>(ent));
            mgr.Redo(); ctx->Yield(2);   // 重做 Add
            IM_CHECK(Reg().all_of<dse::Camera3DComponent>(ent));
            mgr.Redo(); ctx->Yield(2);   // 重做 FOV
            IM_CHECK(std::abs(Reg().get<dse::Camera3DComponent>(ent).fov - 77.0f) < 0.01f);

            // Remove 组件 → 入撤销栈，组件消失，描述为 "Remove Camera 3D"。
            RemoveComponent(ctx, "Camera 3D");
            IM_CHECK(!Reg().all_of<dse::Camera3DComponent>(ent));
            IM_CHECK_STR_EQ(mgr.GetUndoDescription().c_str(), "Remove Camera 3D");

            // Undo（撤销 Remove）→ 组件连同 FOV=77 一并恢复（移除前 EntitySnapshot 抓取了实体组件，
            // Undo 只补回当前缺失的 Camera3D 并带回其数据；实体本身未删，故 ent 句柄仍有效）。
            mgr.Undo(); ctx->Yield(2);
            IM_CHECK(Reg().valid(ent));
            IM_CHECK(Reg().all_of<dse::Camera3DComponent>(ent));
            IM_CHECK(std::abs(Reg().get<dse::Camera3DComponent>(ent).fov - 77.0f) < 0.01f);

            // Redo（重做 Remove）→ 组件再次消失（验证 Remove 可逆）。
            mgr.Redo(); ctx->Yield(2);
            IM_CHECK(!Reg().all_of<dse::Camera3DComponent>(ent));

            // 收尾：删掉实体（此时无相机组件，直接走世界删除即可）。
            if (Reg().valid(ent)) {
                Services().engine->pipeline()->world().DestroyEntity(ent);
                ctx->Yield();
            }
            IM_CHECK(!Reg().valid(ent));
        };
    }

    // ── ④-7 反射驱动 Inspector：PostProcess 浮点字段改值 + Edit/Undo 回退 ──────
    // 验证 DrawReflectedSection 通用驱动渲染出可寻址控件、写回 ECS、并接入撤销栈。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "reflected_postprocess_edit_undo");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Post Process");
            IM_CHECK(Reg().all_of<dse::PostProcessComponent>(ent));
            const float before = Reg().get<dse::PostProcessComponent>(ent).bloom_threshold;

            // 控件 id 由通用驱动按 "##<Type>.<field>" 生成。
            ctx->ItemInputValue("//Inspector/##PostProcessComponent.bloom_threshold", 3.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::PostProcessComponent>(ent).bloom_threshold - 3.5f) < 0.01f);

            auto& mgr = GetUndoRedoManager();
            IM_CHECK(mgr.CanUndo());
            IM_CHECK_STR_EQ(mgr.GetUndoDescription().c_str(), "PostProcessComponent.bloom_threshold");
            mgr.Undo(); ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::PostProcessComponent>(ent).bloom_threshold - before) < 0.01f);
            mgr.Redo(); ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::PostProcessComponent>(ent).bloom_threshold - 3.5f) < 0.01f);

            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ④-8 反射驱动 Inspector：Tree 字符串/布尔字段改值 ─────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-components", "reflected_tree_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Tree");
            IM_CHECK(Reg().all_of<dse::TreeComponent>(ent));

            // bool（Checkbox）
            const bool before_shadow = Reg().get<dse::TreeComponent>(ent).cast_shadow;
            ctx->ItemClick("//Inspector/##TreeComponent.cast_shadow");
            ctx->Yield(2);
            IM_CHECK(Reg().get<dse::TreeComponent>(ent).cast_shadow != before_shadow);

            // float（DragFloat，range 钳制）
            ctx->ItemInputValue("//Inspector/##TreeComponent.density", 0.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::TreeComponent>(ent).density - 0.5f) < 0.01f);

            DeleteSelectedEntity(ctx, ent);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
