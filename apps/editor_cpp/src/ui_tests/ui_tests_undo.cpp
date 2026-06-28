/**
 * @file ui_tests_undo.cpp
 * @brief 撤销/重做用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 编辑器最核心的工作流。撤销栈按操作类型铺开覆盖：
 *   - undo_redo_via_menu ：建实体(入栈) → Edit/Undo / Edit/Redo（菜单项标签运行期动态拼）。
 *   - undo_redo_transform：Inspector 改 Transform.Position → Ctrl+Z 回弹 / Ctrl+Y 复原。
 *   - undo_redo_rename   ：Inspector 改 ##Name → Ctrl+Z 回弹 / Ctrl+Y 复原。
 *   - undo_redo_reparent ：Hierarchy 拖拽 reparent → Ctrl+Z 解除 / Ctrl+Y 复原。
 * 断言以真实 ECS registry 的组件数据为准。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_shortcuts.h"        // GetUndoRedoManager
#include "../editor_selection.h"        // SelectionManager
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent / ParentComponent（全局命名空间）

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 在 Hierarchy 里建一个新的根实体（右键 → Create Empty Entity，创建即选中），返回新建实体。
entt::entity CreateSelectedEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield();
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) return en;
    }
    return entt::null;
}

// 发全局撤销/重做快捷键（先 Escape 让任何活动输入框失活，避免 WantTextInput 吞掉快捷键）。
void PressUndo(ImGuiTestContext* ctx) {
    ctx->KeyPress(ImGuiKey_Escape);
    ctx->Yield();
    ctx->SetRef("//DSEngineRoot");
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
    ctx->Yield(2);
}
void PressRedo(ImGuiTestContext* ctx) {
    ctx->SetRef("//DSEngineRoot");
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Y);
    ctx->Yield(2);
}

} // namespace

void RegisterUndoTests(ImGuiTestEngine* e) {
    // dse-undo/undo_redo_via_menu：创建实体(+1) → Edit/Undo(回到原数) → Edit/Redo(再+1)。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_via_menu");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& mgr = GetUndoRedoManager();

            const int before = CountValidEntities();
            IM_CHECK(before >= 0);

            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            ctx->Yield();
            IM_CHECK_EQ(CountValidEntities(), before + 1);
            IM_CHECK(mgr.CanUndo());

            // Edit 菜单 Undo 标签为动态 "Undo (<描述>)"，按运行期描述拼精确标签。
            const std::string undo_label = std::string("Edit/Undo (") + mgr.GetUndoDescription() + ")";
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(undo_label.c_str());
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before);
            IM_CHECK(mgr.CanRedo());

            const std::string redo_label = std::string("Edit/Redo (") + mgr.GetRedoDescription() + ")";
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(redo_label.c_str());
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }

    // dse-undo/undo_redo_transform：Inspector 改 Position.X → Ctrl+Z 回到 0 → Ctrl+Y 回到改后值。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_transform");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = CreateSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            SelectionManager::Get().Clear();  // 走单选 Transform 渲染分支

            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 7.5f);
            ctx->Yield(2);

            entt::registry& reg = Reg();
            IM_CHECK(reg.valid(ent) && reg.all_of<TransformComponent>(ent));
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 7.5f) < 0.01f);

            PressUndo(ctx);
            IM_CHECK(reg.valid(ent) && reg.all_of<TransformComponent>(ent));
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 0.0f) < 0.01f);

            PressRedo(ctx);
            IM_CHECK(reg.valid(ent) && reg.all_of<TransformComponent>(ent));
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 7.5f) < 0.01f);
        };
    }

    // dse-undo/undo_redo_rename：Inspector 改 ##Name → Ctrl+Z 回旧名 → Ctrl+Y 回新名。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_rename");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = CreateSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            SelectionManager::Get().Clear();

            entt::registry& reg = Reg();
            const std::string old_name =
                reg.all_of<EditorNameComponent>(ent) ? reg.get<EditorNameComponent>(ent).name : std::string();

            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##Name", "DSEUndoRenameTarget");
            ctx->Yield(2);
            IM_CHECK(reg.valid(ent) && reg.all_of<EditorNameComponent>(ent));
            IM_CHECK_STR_EQ(reg.get<EditorNameComponent>(ent).name.c_str(), "DSEUndoRenameTarget");

            PressUndo(ctx);
            IM_CHECK_STR_EQ(reg.get<EditorNameComponent>(ent).name.c_str(), old_name.c_str());

            PressRedo(ctx);
            IM_CHECK_STR_EQ(reg.get<EditorNameComponent>(ent).name.c_str(), "DSEUndoRenameTarget");
        };
    }

    // dse-undo/undo_redo_reparent：Hierarchy 拖拽 reparent → Ctrl+Z 解除父子 → Ctrl+Y 复原。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_reparent");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = Reg();

            HideOptionalPanels();
            ctx->Yield(4);

            const entt::entity a = CreateSelectedEntity(ctx);
            const entt::entity b = CreateSelectedEntity(ctx);
            IM_CHECK(a != entt::null && b != entt::null && a != b);

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield(2);

            char src_ref[96], dst_ref[96];
            std::snprintf(src_ref, sizeof(src_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(a)));
            std::snprintf(dst_ref, sizeof(dst_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(b)));

            ctx->WindowFocus("//Hierarchy");
            // 累积实体可能令 A/B 节点被裁剪/滚出可视区；先滚动目标节点入视口再读屏幕矩形。
            ctx->ScrollToItemY(dst_ref);
            ctx->Yield(2);
            const ImGuiTestItemInfo si = ctx->ItemInfo(src_ref);
            const ImGuiTestItemInfo di = ctx->ItemInfo(dst_ref);
            IM_CHECK(si.ID != 0 && di.ID != 0);
            const ImVec2 src_pos(si.RectFull.GetCenter().x, si.RectFull.Min.y + si.RectFull.GetHeight() * 0.5f);
            const ImVec2 dst_pos(di.RectFull.GetCenter().x, di.RectFull.Min.y + di.RectFull.GetHeight() * 0.25f);

            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield();
            ManualMouseDrag(ctx, src_pos, dst_pos);

            IM_CHECK(reg.all_of<ParentComponent>(a));
            IM_CHECK(reg.get<ParentComponent>(a).parent == b);

            PressUndo(ctx);
            const bool detached = !reg.all_of<ParentComponent>(a) ||
                                  reg.get<ParentComponent>(a).parent != b;
            IM_CHECK(detached);

            PressRedo(ctx);
            IM_CHECK(reg.all_of<ParentComponent>(a));
            IM_CHECK(reg.get<ParentComponent>(a).parent == b);
        };
    }

    // dse-undo/undo_redo_multistep_stack：连建 3 个实体压满撤销栈 → 连撤 3 次回到起点 → 连重做 3 次复原。
    // 逐级断言实体计数 + UndoRedoManager 的 GetUndoCount/GetRedoCount（验证撤销栈“深度”而非单步）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_multistep_stack");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = Reg();
            auto& mgr = GetUndoRedoManager();
            mgr.Clear();

            // 起点快照：用于收尾时只清理本用例新建的实体。
            std::vector<entt::entity> base;
            for (auto en : reg.storage<entt::entity>())
                if (reg.valid(en)) base.push_back(en);
            const int before = static_cast<int>(base.size());

            for (int i = 1; i <= 3; ++i) {
                OpenHierarchyContextMenu(ctx);
                ctx->ItemClick("Create Empty Entity");
                ctx->Yield();
                IM_CHECK_EQ(CountValidEntities(), before + i);
                IM_CHECK_EQ(mgr.GetUndoCount(), i);
            }
            IM_CHECK(!mgr.CanRedo());
            IM_CHECK_EQ(mgr.GetRedoCount(), 0);

            // 连撤 3 次：每撤一次计数 -1、撤销栈 -1、重做栈 +1。
            for (int i = 2; i >= 0; --i) {
                PressUndo(ctx);
                IM_CHECK_EQ(CountValidEntities(), before + i);
                IM_CHECK_EQ(mgr.GetUndoCount(), i);
                IM_CHECK_EQ(mgr.GetRedoCount(), 3 - i);
            }

            // 连重做 3 次：精确回到 before+3。
            for (int i = 1; i <= 3; ++i) {
                PressRedo(ctx);
                IM_CHECK_EQ(CountValidEntities(), before + i);
                IM_CHECK_EQ(mgr.GetUndoCount(), i);
                IM_CHECK_EQ(mgr.GetRedoCount(), 3 - i);
            }

            // 收尾：删掉本用例新建出来的实体（按差集），避免污染后续用例计数。
            std::vector<entt::entity> extra;
            for (auto en : reg.storage<entt::entity>()) {
                if (!reg.valid(en)) continue;
                bool seen = false;
                for (auto b : base) if (b == en) { seen = true; break; }
                if (!seen) extra.push_back(en);
            }
            for (auto en : extra) Services().engine->pipeline()->world().DestroyEntity(en);
            ctx->Yield();
            mgr.Clear();
        };
    }

    // dse-undo/undo_redo_invalidation：建实体 → 撤销（重做栈出现 1 项）→ 再做一个“新操作”（再建实体）
    // → 重做栈应被清空（CanRedo()==false、GetRedoCount()==0）。验证“撤销后做新操作会截断重做分支”。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_invalidation");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& mgr = GetUndoRedoManager();
            mgr.Clear();

            const int before = CountValidEntities();

            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            ctx->Yield();
            IM_CHECK_EQ(CountValidEntities(), before + 1);
            IM_CHECK(mgr.CanUndo());

            PressUndo(ctx);
            IM_CHECK_EQ(CountValidEntities(), before);
            IM_CHECK(mgr.CanRedo());
            IM_CHECK_EQ(mgr.GetRedoCount(), 1);

            // 撤销后做新操作 → Execute 内部 redo_stack_.clear()，重做分支被截断。
            const entt::entity y = CreateSelectedEntity(ctx);
            IM_CHECK(y != entt::null);
            IM_CHECK(!mgr.CanRedo());
            IM_CHECK_EQ(mgr.GetRedoCount(), 0);

            // Ctrl+Y 此时应无效（栈空），实体数不变。
            PressRedo(ctx);
            IM_CHECK_EQ(CountValidEntities(), before + 1);

            Services().engine->pipeline()->world().DestroyEntity(y);
            ctx->Yield();
            mgr.Clear();
        };
    }

    // dse-undo/undo_redo_mixed_ops：对同一实体连做三种不同类型操作（改 Position.X / 改 Name / 改 Position.Y）
    // 压入混合栈 → 逆序连撤每一类都回弹 → 顺序连重做每一类都复原。验证跨操作类型的栈顺序正确。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_mixed_ops");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = CreateSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            SelectionManager::Get().Clear();  // 单选 Inspector 分支

            auto& mgr = GetUndoRedoManager();
            mgr.Clear();  // 把“创建”移出栈，只测三步编辑的混合撤销栈
            entt::registry& reg = Reg();

            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 5.0f);   // op1
            ctx->Yield(2);
            ctx->ItemInputValue("##Name", "DSEMixedOps");          // op2
            ctx->Yield(2);
            ctx->ItemInputValue("##pos_undo/##pos/##y", 9.0f);   // op3
            ctx->Yield(2);

            IM_CHECK_EQ(mgr.GetUndoCount(), 3);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 5.0f) < 0.01f);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.y - 9.0f) < 0.01f);
            IM_CHECK_STR_EQ(reg.get<EditorNameComponent>(ent).name.c_str(), "DSEMixedOps");

            // 逆序撤销：op3(Position.Y) → op2(Name) → op1(Position.X)。
            PressUndo(ctx);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.y - 0.0f) < 0.01f);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 5.0f) < 0.01f);  // 未被波及

            PressUndo(ctx);  // 撤销 op2(Name)：名字回退，不再是 "DSEMixedOps"
            IM_CHECK(reg.get<EditorNameComponent>(ent).name != "DSEMixedOps");
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 5.0f) < 0.01f);  // X 仍在

            PressUndo(ctx);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 0.0f) < 0.01f);
            IM_CHECK_EQ(mgr.GetUndoCount(), 0);
            IM_CHECK_EQ(mgr.GetRedoCount(), 3);

            // 顺序重做：op1(X=5) → op2(Name) → op3(Y=9)。
            PressRedo(ctx);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 5.0f) < 0.01f);
            PressRedo(ctx);
            IM_CHECK_STR_EQ(reg.get<EditorNameComponent>(ent).name.c_str(), "DSEMixedOps");
            PressRedo(ctx);
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.y - 9.0f) < 0.01f);

            Services().engine->pipeline()->world().DestroyEntity(ent);
            ctx->Yield();
            mgr.Clear();
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
