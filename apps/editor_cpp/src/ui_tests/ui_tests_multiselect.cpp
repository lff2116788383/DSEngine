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

#include "../editor_selection.h"          // SelectionManager
#include "../editor_shortcuts.h"          // HasEntityClipboard
#include "../editor_scene_tabs.h"         // SceneTabManager
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"

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
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS

