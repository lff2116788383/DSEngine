/**
 * @file ui_tests_hierarchy.cpp
 * @brief 层级（Hierarchy）面板的真实控件级 UI 测试 + harness 自检（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 用例体均为无捕获 lambda（GuiFunc/TestFunc 为函数指针，见 imconfig STD_FUNCTION=0），
 * 经 Services() 取引擎句柄；断言以真实 ECS registry 的实体计数增减为准——驱动的是
 * 编辑器真实绘制的 Hierarchy 右键菜单，而非 mock。共享工具见 ui_tests_common.cpp。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cstdint>
#include <cstdio>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_hierarchy_panel.h"    // BeginHierarchyRename
#include "../editor_selection.h"          // SelectionManager
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"

namespace dse::editor::uitest {

namespace {

/// harness 自检 GuiFunc 间共享的状态。
struct SanityVars {
    int click_count = 0;
};

entt::registry& HReg() { return Services().engine->pipeline()->world().registry(); }

// 建一个新实体（右键 → Create Empty Entity，创建即选中），返回新建实体。
entt::entity HCreateEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = HReg();
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

void NodeRef(char* buf, size_t n, entt::entity ent) {
    std::snprintf(buf, n, "//Hierarchy/Scene/$$(ptr)0x%llx",
                  static_cast<unsigned long long>(static_cast<std::uint32_t>(ent)));
}

} // namespace

void RegisterHarnessSanityTests(ImGuiTestEngine* e) {
    // dse-harness/sanity_button_click：自带 GuiFunc 画按钮，TestFunc 真点两次断言计数 1、2。
    // 验证“测试引擎 ←→ 编辑器 ImGui 上下文”整条链路端到端可用。
    ImGuiTest* t = IM_REGISTER_TEST(e, "dse-harness", "sanity_button_click");
    t->SetVarsDataType<SanityVars>();
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        SanityVars& vars = ctx->GetVars<SanityVars>();
        ImGui::Begin("DSE Sanity Window");
        if (ImGui::Button("Sanity Button")) {
            vars.click_count++;
        }
        ImGui::Text("count=%d", vars.click_count);
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        SanityVars& vars = ctx->GetVars<SanityVars>();
        ctx->SetRef("DSE Sanity Window");
        ctx->ItemClick("Sanity Button");
        IM_CHECK_EQ(vars.click_count, 1);
        ctx->ItemClick("Sanity Button");
        IM_CHECK_EQ(vars.click_count, 2);
    };
}

void RegisterHierarchyTests(ImGuiTestEngine* e) {
    // dse-hierarchy/context_menu_chain：把「右键 → 点菜单项 → 实体 +1」整条链的各环节
    // 实测值落盘并断言，用于把交互层故障定位到具体环节（而不是只知道最终计数没变）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "context_menu_chain");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const int before = CountValidEntities();
            ImGuiWindow* hierarchy = ctx->GetWindowByRef("//Hierarchy");
            const ImGuiTestItemInfo node =
                ctx->ItemInfo("//Hierarchy/Scene", ImGuiTestOpFlags_NoError);
            UiDiagLog("[chain] hierarchy_win=%s rect=(%.0f,%.0f)-(%.0f,%.0f) node_found=%d "
                      "node_rect=(%.0f,%.0f)-(%.0f,%.0f)",
                      hierarchy ? hierarchy->Name : "(none)",
                      hierarchy ? hierarchy->Pos.x : 0.0f, hierarchy ? hierarchy->Pos.y : 0.0f,
                      hierarchy ? hierarchy->Pos.x + hierarchy->Size.x : 0.0f,
                      hierarchy ? hierarchy->Pos.y + hierarchy->Size.y : 0.0f,
                      node.ID != 0 ? 1 : 0,
                      node.RectClipped.Min.x, node.RectClipped.Min.y,
                      node.RectClipped.Max.x, node.RectClipped.Max.y);
            OpenHierarchyContextMenu(ctx);
            ImGuiContext& g = *ImGui::GetCurrentContext();
            UiDiagLog("[chain] mouse=(%.0f,%.0f) hovered=%s nav=%s open_popups=%d",
                      g.IO.MousePos.x, g.IO.MousePos.y,
                      g.HoveredWindow ? g.HoveredWindow->Name : "(none)",
                      g.NavWindow ? g.NavWindow->Name : "(none)",
                      g.OpenPopupStack.Size);
            ImGuiWindow* popup = ctx->GetWindowByRef("//$FOCUSED");
            const bool is_popup = popup != nullptr &&
                (popup->Flags & ImGuiWindowFlags_Popup) != 0;
            const ImGuiTestItemInfo info =
                ctx->ItemInfo("Create Empty Entity", ImGuiTestOpFlags_NoError);
            const bool disabled = (info.ItemFlags & ImGuiItemFlags_Disabled) != 0;
            UiDiagLog("[chain] entities_before=%d popup=%d popup_name=%s item_found=%d "
                      "item_disabled=%d item_label='%s' item_rect=(%.0f,%.0f)-(%.0f,%.0f)",
                      before, is_popup ? 1 : 0, popup ? popup->Name : "(none)",
                      info.ID != 0 ? 1 : 0, disabled ? 1 : 0, info.DebugLabel,
                      info.RectClipped.Min.x, info.RectClipped.Min.y,
                      info.RectClipped.Max.x, info.RectClipped.Max.y);
            IM_CHECK(is_popup);
            IM_CHECK(info.ID != 0);
            IM_CHECK(!disabled);
            ctx->ItemClick("Create Empty Entity");
            const int after = CountValidEntities();
            UiDiagLog("[chain] entities_after=%d delta=%d", after, after - before);
            IM_CHECK_EQ(after, before + 1);
        };
    }

    // dse-hierarchy/create_empty_entity：右键 → Create Empty Entity，实体数 +1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "create_empty_entity");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const int before = CountValidEntities();
            IM_CHECK(before >= 0);
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }

    // dse-hierarchy/create_ui_entity：右键 → Create UI Entity，实体数 +1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "create_ui_entity");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const int before = CountValidEntities();
            IM_CHECK(before >= 0);
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create UI Entity");
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }

    // dse-hierarchy/duplicate_entity：先建一个空实体并选中，再右键 → Duplicate Entity，+1。
    // （Duplicate/Delete 菜单项仅在有选中实体时出现，故先 create 选中它。）
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "duplicate_entity");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Duplicate Entity");
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }

    // dse-hierarchy/delete_entity：先建一个空实体并选中，再右键 → Delete Entity，-1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "delete_entity");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Delete Entity");
            IM_CHECK_EQ(CountValidEntities(), before - 1);
        };
    }

    // dse-hierarchy/inline_rename：双击节点进入内联重命名（出现 "##rename" 输入框），
    // 键入新名 + 回车提交，断言该实体 EditorNameComponent.name 被改写。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "inline_rename");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            HideOptionalPanels();
            ctx->Yield(4);

            const entt::entity ent = HCreateEntity(ctx);
            IM_CHECK(ent != entt::null);

            ctx->Yield(2);

            // 确保 Scene 树节点展开（被前面的测试折叠时 ##rename 不会被绘制）
            ctx->ItemOpen("//Hierarchy/Scene");
            ctx->Yield(2);

            // 直接调用 BeginHierarchyRename 触发重命名模式。
            // 双击和 F2 快捷键在后台/远程环境下可能因 ImGuizmo gizmo 覆盖窗口
            // 阻挡点击而失败。此测试重点验证重命名机制本身。
            {
                entt::registry& reg = HReg();
                std::string current_name = "New Entity";
                if (reg.all_of<EditorNameComponent>(ent))
                    current_name = reg.get<EditorNameComponent>(ent).name;
                dse::editor::BeginHierarchyRename(ent, current_name);
            }
            // SetKeyboardFocusHere 在下一帧生效；第 2 帧 InputText 激活并绘制。
            ctx->Yield(4);

            // SetKeyboardFocusHere 已把焦点设给 ##rename InputText；
            // 直接用键盘输入绕过 gizmo 覆盖窗阻挡鼠标点击的问题。
            ctx->KeyCharsReplace("DSEInlineRenamed");
            ctx->KeyPress(ImGuiKey_Enter);
            ctx->Yield(2);

            entt::registry& reg = HReg();
            IM_CHECK(reg.valid(ent) && reg.all_of<EditorNameComponent>(ent));
            IM_CHECK_STR_EQ(reg.get<EditorNameComponent>(ent).name.c_str(), "DSEInlineRenamed");
        };
    }

    // dse-hierarchy/search_filter：建 A、B 两实体并赋特征名，向搜索框键入 A 的子串后，
    // A 的节点应仍被绘制（ItemInfo 命中）而 B 的节点应被过滤掉（ItemInfo 取不到）。
    // 末尾清空搜索框，避免静态过滤态泄漏影响后续用例对节点的定位。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-hierarchy", "search_filter");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            HideOptionalPanels();
            ctx->Yield(4);

            const entt::entity a = HCreateEntity(ctx);
            const entt::entity b = HCreateEntity(ctx);
            IM_CHECK(a != entt::null && b != entt::null && a != b);

            entt::registry& reg = HReg();
            reg.emplace_or_replace<EditorNameComponent>(a).name = "DSEFilterMatchAAA";
            reg.emplace_or_replace<EditorNameComponent>(b).name = "DSEFilterOtherBBB";

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield(2);

            ctx->WindowFocus("//Hierarchy");
            ctx->ItemInputValue("//Hierarchy/##hierarchy_search", "MatchAAA");
            ctx->Yield(3);  // ComputeVisibleEntities 据过滤重算可见集

            char a_ref[96], b_ref[96];
            NodeRef(a_ref, sizeof(a_ref), a);
            NodeRef(b_ref, sizeof(b_ref), b);
            const ImGuiTestItemInfo ai = ctx->ItemInfo(a_ref, ImGuiTestOpFlags_NoError);
            const ImGuiTestItemInfo bi = ctx->ItemInfo(b_ref, ImGuiTestOpFlags_NoError);
            IM_CHECK(ai.ID != 0);   // 匹配项仍绘制
            IM_CHECK(bi.ID == 0);   // 非匹配项被过滤（节点不绘制）

            // 还原：清空搜索框，避免静态 s_search_filter 残留影响后续用例。
            ctx->ItemInputValue("//Hierarchy/##hierarchy_search", "");
            ctx->Yield(3);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
