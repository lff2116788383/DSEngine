/**
 * @file ui_tests_negative.cpp
 * @brief 负向/边界用例（仅 DSE_EDITOR_UI_TESTS 编入）——“绝不能崩”是最初诉求。
 *
 *   - circular_parenting_rejected：把父实体拖到自己的子孙节点上 → 形成环被拒，层级保持不变。
 *   - empty_stack_undo_redo      ：清空撤销栈后连发 Ctrl+Z/Ctrl+Y → 不崩、栈仍为空、实体数不变。
 *   - delete_without_selection   ：删除已被删的（悬空选择）实体 → 二次 Delete 安全无操作、不崩。
 * 断言以真实 ECS registry / UndoRedoManager 状态为准。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_shortcuts.h"          // GetUndoRedoManager
#include "../editor_selection.h"          // SelectionManager
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent / ParentComponent（全局命名空间）

namespace dse::editor::uitest {

namespace {

entt::registry& NReg() { return Services().engine->pipeline()->world().registry(); }

entt::entity NCreate(ImGuiTestContext* ctx) {
    entt::registry& reg = NReg();
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

ImVec2 NodeDropPos(const ImGuiTestItemInfo& info) {
    // 取节点矩形上四分之一，避开正下方紧贴的“插入兄弟”落区，稳稳命中节点本体 reparent 落区。
    return ImVec2(info.RectFull.GetCenter().x, info.RectFull.Min.y + info.RectFull.GetHeight() * 0.25f);
}

} // namespace

void RegisterNegativeTests(ImGuiTestEngine* e) {
    // dse-negative/circular_parenting_rejected：先 B 挂到 A 下，再把 A 拖到其子 B 上 → 环检测拒绝，
    // A 仍为根、B 仍是 A 的子（层级不变、不崩）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-negative", "circular_parenting_rejected");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = NReg();

            HideOptionalPanels();
            ctx->Yield(4);

            const entt::entity a = NCreate(ctx);
            const entt::entity b = NCreate(ctx);
            IM_CHECK(a != entt::null && b != entt::null && a != b);

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield(2);

            char a_ref[96], b_ref[96];
            std::snprintf(a_ref, sizeof(a_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(a)));
            std::snprintf(b_ref, sizeof(b_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(b)));

            // Step 1：把 B 拖到 A 上 → B.parent == A。
            ctx->WindowFocus("//Hierarchy");
            // 累积实体可能令 A/B 节点被裁剪/滚出可视区；先滚动入视口再按屏幕坐标投递拖拽。
            ctx->ScrollToItemY(b_ref);
            ctx->Yield(2);
            {
                const ImGuiTestItemInfo bi = ctx->ItemInfo(b_ref);
                const ImGuiTestItemInfo ai = ctx->ItemInfo(a_ref);
                IM_CHECK(bi.ID != 0 && ai.ID != 0);
                const ImVec2 src(bi.RectFull.GetCenter().x, bi.RectFull.Min.y + bi.RectFull.GetHeight() * 0.5f);
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield();
                ManualMouseDrag(ctx, src, NodeDropPos(ai));
            }
            IM_CHECK(reg.all_of<ParentComponent>(b) && reg.get<ParentComponent>(b).parent == a);

            // 等若干帧让 Hierarchy 据更新后的 ParentComponent 重绘：A 这一帧才从“叶子”变成
            // 带折叠箭头的父节点，否则紧接着 ItemOpen 会因目标仍是叶子而报“Unable to Open”。
            ctx->Yield(4);

            // reparent 落点会把被拖实体重新选中并拉起 ImGuizmo 全屏覆盖窗，挡住紧接着对 A 折叠
            // 箭头的点击（ItemOpen 走真实鼠标），故先清空选择移除覆盖窗。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield(2);
            ctx->WindowFocus("//Hierarchy");

            // 展开 A 让其子 B 的节点可见（折叠时子节点不绘制）。
            // 实体节点用 ImGuiTreeNodeFlags_OpenOnArrow，且双击被内联重命名占用，所以 ItemOpen
            // （点节点本体/双击）都无法展开——必须点最左侧的折叠箭头。直接对 A 节点矩形最左侧投点击。
            {
                const ImGuiTestItemInfo aii = ctx->ItemInfo(a_ref);
                IM_CHECK(aii.ID != 0);
                const ImVec2 arrow(aii.RectFull.Min.x + aii.RectFull.GetHeight() * 0.5f,
                                   aii.RectFull.GetCenter().y);
                ctx->MouseMoveToPos(arrow);
                ctx->MouseClick(0);
            }
            ctx->Yield(2);

            // B 现在是 A 的子，绝对路径含父前缀。
            char b_child_ref[160];
            std::snprintf(b_child_ref, sizeof(b_child_ref),
                          "//Hierarchy/Scene/$$(ptr)0x%llx/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(a)),
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(b)));

            // Step 2：把 A 拖到其子孙 B 上 → 形成环，应被 dsengine_entity_reparent 的环检测拒绝。
            ctx->ScrollToItemY(a_ref);
            ctx->Yield(2);
            {
                const ImGuiTestItemInfo ai = ctx->ItemInfo(a_ref);
                const ImGuiTestItemInfo bci = ctx->ItemInfo(b_child_ref, ImGuiTestOpFlags_NoError);
                IM_CHECK(ai.ID != 0 && bci.ID != 0);
                const ImVec2 src(ai.RectFull.GetCenter().x, ai.RectFull.Min.y + ai.RectFull.GetHeight() * 0.5f);
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield();
                ManualMouseDrag(ctx, src, NodeDropPos(bci));
            }

            // 层级未变：A 仍为根，B 仍是 A 的子。
            IM_CHECK(!reg.all_of<ParentComponent>(a) ||
                     reg.get<ParentComponent>(a).parent == entt::null);
            IM_CHECK(reg.all_of<ParentComponent>(b) && reg.get<ParentComponent>(b).parent == a);
        };
    }

    // dse-negative/empty_stack_undo_redo：清空撤销栈 → Ctrl+Z/Ctrl+Y 连发 → 不崩、栈仍空、实体数不变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-negative", "empty_stack_undo_redo");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& mgr = GetUndoRedoManager();
            mgr.Clear();
            IM_CHECK(!mgr.CanUndo());
            IM_CHECK(!mgr.CanRedo());

            const int before = CountValidEntities();

            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Y);
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Y);
            ctx->Yield(2);

            IM_CHECK(!mgr.CanUndo());
            IM_CHECK(!mgr.CanRedo());
            IM_CHECK_EQ(CountValidEntities(), before);  // 空栈撤销/重做不应改变任何东西
        };
    }

    // dse-negative/delete_without_selection：建实体并 Delete（计数 -1）→ 再次 Delete（选择已悬空）
    // 应安全无操作、不崩（DeleteSelectedEntity 删后已置 selected_entity = null）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-negative", "delete_without_selection");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NCreate(ctx);
            IM_CHECK(ent != entt::null);
            SelectionManager::Get().Clear();  // 走单选删除分支（DeleteSelectedEntity）

            const int before = CountValidEntities();
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before - 1);

            // 二次删除：选择已悬空 → 无操作、不崩。
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before - 1);
        };
    }

    // dse-negative/delete_referenced_parent_safe：C 挂到 P 下（P 被 C 的 ParentComponent 引用）→ 删除 P
    // → Hierarchy 连续重绘多帧（GetChildren 会遍历到引用了已删 P 的 C）。断言：P 已删、不崩；C 若仍在，
    // 则不再是“指向某个仍有效实体”的悬挂子（要么随父级联删除，要么 parent 指向已失效 id）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-negative", "delete_referenced_parent_safe");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = NReg();
            HideOptionalPanels();
            ctx->Yield(4);

            // 先建 C 再建 P：NCreate 会把最后创建者设为 context.selected_entity，于是 P 成为单选目标，
            // 后续 Delete 键即删 P（与 delete_without_selection 同一条已验证的删除路径）。
            const entt::entity c = NCreate(ctx);
            const entt::entity p = NCreate(ctx);
            IM_CHECK(p != entt::null && c != entt::null && p != c);

            // 直接建立引用关系：C.parent = P（reparent 拖拽本身已由 dragdrop 组覆盖，这里聚焦“删被引用者”）。
            reg.emplace_or_replace<ParentComponent>(c, p);
            ctx->Yield(2);
            IM_CHECK(reg.all_of<ParentComponent>(c) && reg.get<ParentComponent>(c).parent == p);

            // P 即当前单选目标（context.selected_entity）→ 按 Delete 删 P（被 C 引用）。
            SelectionManager::Get().Clear();  // 走单选删除分支（DeleteSelectedEntity）
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(6);  // 多帧重绘：Hierarchy 反复 GetChildren，验证不会因悬挂引用崩溃

            // P 已删、进程仍在。
            IM_CHECK(!reg.valid(p));
            IM_CHECK(CountValidEntities() >= 0);

            // C 的归宿：要么被级联删除，要么沦为悬挂子（parent 指向已失效 id）——都不应是“指向有效实体”的状态。
            if (reg.valid(c) && reg.all_of<ParentComponent>(c)) {
                IM_CHECK(!reg.valid(reg.get<ParentComponent>(c).parent));
            }

            if (reg.valid(c)) Services().engine->pipeline()->world().DestroyEntity(c);
            ctx->Yield();
            SelectionManager::Get().Clear();
        };
    }

    // dse-negative/extreme_field_input_safe：往 Inspector 的 Position.X 灌入极端大值 → 不崩、字段如实接收
    // （DragFloat 无 min/max，编辑器不应崩）。验证越界/极端数值输入的兜底。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-negative", "extreme_field_input_safe");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = NReg();
            const entt::entity ent = NCreate(ctx);
            IM_CHECK(ent != entt::null);
            IM_CHECK(reg.all_of<TransformComponent>(ent));
            SelectionManager::Get().Clear();  // 单选 Inspector 分支
            ctx->Yield(2);

            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 1.0e18f);  // 极端大值
            ctx->Yield(2);

            // 不崩、实体仍有效，字段如实接收（无 NaN、确为大值）。
            IM_CHECK(reg.valid(ent));
            const float x = reg.get<TransformComponent>(ent).position.x;
            IM_CHECK(!std::isnan(x));
            IM_CHECK(x > 1.0e17f);

            Services().engine->pipeline()->world().DestroyEntity(ent);
            ctx->Yield();
            GetUndoRedoManager().Clear();
        };
    }

    // dse-negative/duplicate_entity_names_safe：两个实体取同名 → Hierarchy 用指针级 ID 绘制，重名不应导致
    // 节点 ID 冲突/崩溃。断言两者都仍有效、互不相同、且都叫该同名。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-negative", "duplicate_entity_names_safe");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = NReg();
            const entt::entity a = NCreate(ctx);  // 创建即选中 a
            ctx->Yield();
            const entt::entity b = NCreate(ctx);  // 创建即选中 b
            IM_CHECK(a != entt::null && b != entt::null && a != b);

            // 经 Inspector 把当前选中的 b 改名为 "DupName"。
            SelectionManager::Get().Clear();
            ctx->Yield();
            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##Name", "DupName");
            ctx->Yield(2);
            IM_CHECK(reg.all_of<EditorNameComponent>(b));
            IM_CHECK(reg.get<EditorNameComponent>(b).name == "DupName");

            // 让 a 取同名（模拟第二个重名对象），多帧重绘 Hierarchy 不应崩。
            reg.emplace_or_replace<EditorNameComponent>(a, std::string("DupName"));
            ctx->Yield(6);

            IM_CHECK(reg.valid(a) && reg.valid(b) && a != b);
            IM_CHECK(reg.get<EditorNameComponent>(a).name == "DupName");
            IM_CHECK(reg.get<EditorNameComponent>(b).name == "DupName");

            Services().engine->pipeline()->world().DestroyEntity(a);
            Services().engine->pipeline()->world().DestroyEntity(b);
            ctx->Yield();
            GetUndoRedoManager().Clear();
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
