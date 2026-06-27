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

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

namespace dse::editor::uitest {

namespace {

/// harness 自检 GuiFunc 间共享的状态。
struct SanityVars {
    int click_count = 0;
};

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
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
