/**
 * @file ui_tests_multiselect.cpp
 * @brief ⑩ 多选 / 复制粘贴 / 跨场景复制粘贴 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 三个真实控件级 + 真断言用例，断言真实 SelectionManager / ECS / 场景页签状态：
 *   - edit_select_all_multi：Edit/Select All 把全部实体纳入多选（断言 SelectionManager
 *     Count/IsMultiSelect/Contains），Edit/Deselect All 后清空。
 *   - edit_copy_paste_same_scene：选中实体 → Edit/Copy → Edit/Paste，断言实体数 +1 且
 *     新实体名带 "(Paste)" 后缀（CopySelectedEntity/PasteEntity 真实剪贴板路径）。
 *   - cross_scene_copy_paste：A 场景 Copy → "+" 新建空场景页签（registry 清空，A 不在）
 *     → Paste 在新场景落出 "(Paste)" 实体；关页签复位、断言 A 随快照还原。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cstdio>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include <cmath>

#include "../editor_selection.h"          // SelectionManager
#include "../editor_shortcuts.h"          // HasEntityClipboard / GetUndoRedoManager
#include "../editor_scene_tabs.h"         // SceneTabManager
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"         // TransformComponent（全局命名空间）

namespace dse::editor::uitest {

namespace {

::World& GetWorld() { return Services().engine->pipeline()->world(); }
entt::registry& Reg() { return GetWorld().registry(); }

// 右键 Hierarchy → Create Empty Entity（创建即选中），按 registry 差集取回新实体。
entt::entity NewSelectedEntity(ImGuiTestContext* ctx) {
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

// 在当前场景 registry 里找首个名字以 "(Paste)" 结尾的实体（粘贴产物特征）。
entt::entity FindPastedEntity() {
    entt::registry& reg = Reg();
    const std::string suffix = "(Paste)";
    for (auto en : reg.view<EditorNameComponent>()) {
        const std::string& name = reg.get<EditorNameComponent>(en).name;
        if (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return en;
        }
    }
    return entt::null;
}

} // namespace

void RegisterMultiSelectTests(ImGuiTestEngine* e) {
    // ── ⑩-1 Edit/Select All → 多选全部实体；Deselect All → 清空 ───────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-multiselect", "edit_select_all_multi");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity a = NewSelectedEntity(ctx);
            IM_CHECK(a != entt::null);
            const entt::entity b = NewSelectedEntity(ctx);
            IM_CHECK(b != entt::null);
            IM_CHECK(a != b);

            auto& sel = SelectionManager::Get();
            ctx->SetRef("//DSEngineRoot");

            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield();
            IM_CHECK(sel.IsEmpty());

            ctx->MenuClick("Edit/Select All");
            ctx->Yield();
            IM_CHECK(sel.Count() >= 2);
            IM_CHECK(sel.IsMultiSelect());
            IM_CHECK(sel.Contains(a));
            IM_CHECK(sel.Contains(b));

            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield();
            IM_CHECK(sel.IsEmpty());

            // 复位：直接走世界删除两个测试实体，避免污染后续用例的实体计数。
            GetWorld().DestroyEntity(a);
            GetWorld().DestroyEntity(b);
            ctx->Yield();
            IM_CHECK(!Reg().valid(a));
            IM_CHECK(!Reg().valid(b));
        };
    }

    // ── ⑩-2 同场景 Edit/Copy → Edit/Paste：实体数 +1 且新实体名带 "(Paste)" ─────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-multiselect", "edit_copy_paste_same_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity a = NewSelectedEntity(ctx);  // 创建即选中
            IM_CHECK(a != entt::null);

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Copy");
            ctx->Yield();
            IM_CHECK(HasEntityClipboard());

            const int before = CountValidEntities();
            ctx->MenuClick("Edit/Paste");
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before + 1);

            const entt::entity pasted = FindPastedEntity();
            IM_CHECK(pasted != entt::null);
            IM_CHECK(pasted != a);

            // 复位：删原实体与粘贴产物。
            GetWorld().DestroyEntity(a);
            GetWorld().DestroyEntity(pasted);
            ctx->Yield();
            IM_CHECK(!Reg().valid(a));
            IM_CHECK(!Reg().valid(pasted));
        };
    }

    // ── ⑩-3 跨场景：A 场景 Copy → 新建空页签 → Paste 落在新场景 ────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-multiselect", "cross_scene_copy_paste");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();

            const entt::entity a = NewSelectedEntity(ctx);
            IM_CHECK(a != entt::null);

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Copy");
            ctx->Yield();
            IM_CHECK(HasEntityClipboard());

            // "+" 新建空场景页签：当前页签连同 A 被快照存走，registry 清空切到新场景。
            const int n0 = tabs.GetTabCount();
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0 + 1);
            IM_CHECK(!Reg().valid(a));   // A 不在新场景里

            // 在新场景粘贴 → 落出 "(Paste)" 实体。
            const int before = CountValidEntities();
            ctx->MenuClick("Edit/Paste");
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before + 1);
            const entt::entity pasted = FindPastedEntity();
            IM_CHECK(pasted != entt::null);

            // 复位：删粘贴产物，右键关掉新页签 → 恢复原页签快照（A 随同一 id 还原）。
            GetWorld().DestroyEntity(pasted);
            ctx->Yield();

            const int last = tabs.GetTabCount() - 1;
            char ref[80];
            std::snprintf(ref, sizeof(ref), "//DSEngineRoot/##SceneTabs/SceneTab%d", last);
            ctx->ItemClick(ref, ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0);
            IM_CHECK(Reg().valid(a));    // 原场景快照还原回来

            GetWorld().DestroyEntity(a);
            ctx->Yield();
            IM_CHECK(!Reg().valid(a));
        };
    }

    // ── ⑩-4 多选批量编辑：选中 A、B（不含 C）→ Inspector「Batch Move」改 Delta.X 并 Apply →
    //        断言只有 A、B 的 position.x 被加上 delta，C 保持不变；Ctrl+Z 回退、Ctrl+Y 复原。
    //        验证“多选状态下改一个字段会批量写穿到所有被选实体（且仅限被选实体）”。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-multiselect", "batch_edit_selected");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = Reg();
            auto& mgr = GetUndoRedoManager();

            HideOptionalPanels();
            ctx->Yield(4);

            const entt::entity a = NewSelectedEntity(ctx);
            const entt::entity b = NewSelectedEntity(ctx);
            const entt::entity c = NewSelectedEntity(ctx);
            IM_CHECK(a != entt::null && b != entt::null && c != entt::null);
            IM_CHECK(reg.all_of<TransformComponent>(a) &&
                     reg.all_of<TransformComponent>(b) &&
                     reg.all_of<TransformComponent>(c));

            // 已知初值，便于断言“加 delta”后的逐实体结果，并区分被选/未选。
            reg.get<TransformComponent>(a).position.x = 1.0f;
            reg.get<TransformComponent>(b).position.x = 2.0f;
            reg.get<TransformComponent>(c).position.x = 3.0f;

            // 精确多选 A、B（排除 C）。多选机制本身已由 edit_select_all_multi 覆盖，这里复用
            // SelectionManager 直接构造选择集，聚焦被测对象＝Inspector 的批量写穿逻辑。
            auto& sel = SelectionManager::Get();
            sel.Clear();
            sel.Add(a);
            sel.Add(b);
            ctx->Yield(2);
            IM_CHECK(sel.IsMultiSelect());

            mgr.Clear();  // 清栈，确保下面这步批量编辑是撤销栈里唯一一项

            // Inspector 多选区「Batch Move」默认展开：改 Move Delta.X = 10，点 Apply Move。
            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##ms_pd/##x", 10.0f);
            ctx->Yield();
            ctx->ItemClick("Apply Move##ms");
            ctx->Yield(2);

            // 仅 A、B 被加上 delta；C 未被选中，保持不变。
            IM_CHECK(std::abs(reg.get<TransformComponent>(a).position.x - 11.0f) < 0.01f);
            IM_CHECK(std::abs(reg.get<TransformComponent>(b).position.x - 12.0f) < 0.01f);
            IM_CHECK(std::abs(reg.get<TransformComponent>(c).position.x - 3.0f) < 0.01f);
            IM_CHECK_EQ(mgr.GetUndoCount(), 1);  // 整批是一个 CompoundCommand

            // Ctrl+Z：一次撤销回退整批。
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield();
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
            ctx->Yield(2);
            IM_CHECK(std::abs(reg.get<TransformComponent>(a).position.x - 1.0f) < 0.01f);
            IM_CHECK(std::abs(reg.get<TransformComponent>(b).position.x - 2.0f) < 0.01f);

            // Ctrl+Y：一次重做复原整批。
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Y);
            ctx->Yield(2);
            IM_CHECK(std::abs(reg.get<TransformComponent>(a).position.x - 11.0f) < 0.01f);
            IM_CHECK(std::abs(reg.get<TransformComponent>(b).position.x - 12.0f) < 0.01f);

            // 复位。
            sel.Clear();
            GetWorld().DestroyEntity(a);
            GetWorld().DestroyEntity(b);
            GetWorld().DestroyEntity(c);
            ctx->Yield();
            mgr.Clear();
            IM_CHECK(!Reg().valid(a) && !Reg().valid(b) && !Reg().valid(c));
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS

