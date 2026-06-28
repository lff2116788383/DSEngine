/**
 * @file ui_tests_tabs.cpp
 * @brief 多场景页签用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 走 DSEngineRoot 内的 "##SceneTabs" 真实 TabBar：
 *   - create_switch_close：用 "+" 新建两页签 → 点页签切换（断言 active index）→ 右键 "Close" 关闭一页。
 *   - close_others       ：多页签下右键 "Close Others" → 仅剩当前一页。
 * 页签 ID 用稳定后缀（label "###SceneTabN"），断言以 SceneTabManager 真实状态为准。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_scene_tabs.h"
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent（全局命名空间）

namespace dse::editor::uitest {

void RegisterSceneTabTests(ImGuiTestEngine* e) {
    // dse-tabs/create_switch_close
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-tabs", "create_switch_close");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();

            // 用 "+" 按钮新建两个空场景页签。
            const int n0 = tabs.GetTabCount();
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0 + 2);

            // 切到第 0 个页签。
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/SceneTab0");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetActiveIndex(), 0);

            // 切到最后一个页签。
            const int last = tabs.GetTabCount() - 1;
            char ref[80];
            std::snprintf(ref, sizeof(ref), "//DSEngineRoot/##SceneTabs/SceneTab%d", last);
            ctx->ItemClick(ref);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetActiveIndex(), last);

            // 右键最后一个页签 → Close → 页签 -1。
            const int before_close = tabs.GetTabCount();
            ctx->ItemClick(ref, ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), before_close - 1);
        };
    }

    // dse-tabs/scene_switch_state_isolation：多页签“场景内容隔离与各自保持”。
    // 在新页签 A 里造探针实体 X（特征名 + Transform.X）→ "+" 新建空页签 B（A 的内容不泄漏：B 为空）
    // → 切回 A：X 经页签快照如实还原（名字 + 坐标）→ 再切 B：仍为空。验证每页签场景相互隔离且各自保持。
    // 注：选中态/撤销栈按设计在切页签时由 RestoreTabSnapshot 重置（每页签切入都是干净的编辑现场），
    // 故本用例职责限于“场景内容”维度的隔离/保持，不断言选中/undo 跨页签存活。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-tabs", "scene_switch_state_isolation");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();
            entt::registry& reg = Services().engine->pipeline()->world().registry();

            const char* kProbe = "DSETabIsoProbe";
            auto has_probe = [&](float x) {
                for (auto en : reg.view<EditorNameComponent, TransformComponent>()) {
                    if (reg.get<EditorNameComponent>(en).name == kProbe &&
                        std::abs(reg.get<TransformComponent>(en).position.x - x) < 0.01f)
                        return true;
                }
                return false;
            };
            auto count_probe = [&]() {
                int c = 0;
                for (auto en : reg.view<EditorNameComponent>())
                    if (reg.get<EditorNameComponent>(en).name == kProbe) ++c;
                return c;
            };

            const int n0 = tabs.GetTabCount();

            // 新建页签 A（干净空场景，隔离前序用例累积的实体）。
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            const int a_idx = tabs.GetActiveIndex();
            IM_CHECK_EQ(CountValidEntities(), 0);  // 新空场景

            // 在 A 里造探针实体 X：特征名 + Transform.X=4.5。
            const entt::entity x = reg.create();
            reg.emplace<EditorNameComponent>(x, std::string(kProbe));
            reg.emplace<TransformComponent>(x).position.x = 4.5f;
            ctx->Yield();
            IM_CHECK(has_probe(4.5f));

            // "+" 新建空页签 B：切到 B 前会快照 A；B 应为空（A 的探针不泄漏到 B）。
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            const int b_idx = tabs.GetActiveIndex();
            IM_CHECK(b_idx != a_idx);
            IM_CHECK_EQ(CountValidEntities(), 0);
            IM_CHECK_EQ(count_probe(), 0);

            // 切回 A：探针随页签快照如实还原。
            char a_ref[80];
            std::snprintf(a_ref, sizeof(a_ref), "//DSEngineRoot/##SceneTabs/SceneTab%d", a_idx);
            ctx->ItemClick(a_ref);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetActiveIndex(), a_idx);
            IM_CHECK(has_probe(4.5f));

            // 再切回 B：仍为空（互不影响）。
            char b_ref[80];
            std::snprintf(b_ref, sizeof(b_ref), "//DSEngineRoot/##SceneTabs/SceneTab%d", b_idx);
            ctx->ItemClick(b_ref);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetActiveIndex(), b_idx);
            IM_CHECK_EQ(count_probe(), 0);

            // 收尾：逐个右键关掉新增的 B、A 两页签，恢复进入前的页签数（B 在 A 之后，先关 B）。
            ctx->ItemClick(b_ref, ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close");
            ctx->Yield(2);
            ctx->ItemClick(a_ref, ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0);
        };
    }

    // dse-tabs/close_others：确保 ≥3 页签后右键某页 → "Close Others" → 仅剩一页。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-tabs", "close_others");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();

            // 补足到至少 3 个页签。
            while (tabs.GetTabCount() < 3) {
                ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
                ctx->Yield(2);
            }
            IM_CHECK(tabs.GetTabCount() >= 3);

            // 右键第 0 个页签 → Close Others → 仅剩它自己。
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/SceneTab0", ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close Others");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), 1);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
