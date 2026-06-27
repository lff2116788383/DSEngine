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
            IM_CHECK(std::abs(reg.get<TransformComponent>(ent).position.x - 0.0f) < 0.01f);

            PressRedo(ctx);
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
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
