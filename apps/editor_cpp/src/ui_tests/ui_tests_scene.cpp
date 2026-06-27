/**
 * @file ui_tests_scene.cpp
 * @brief 场景级基础操作用例：新建场景 / 保存场景（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 与项目操作同属“绝不能崩”的关键路径，走真实 File 菜单控件：
 *   - new_scene ：File → New Scene（无对话框）→ registry 清空、页签 +1、当前页签为 Untitled。
 *   - save_scene：先把当前页签路径设到临时文件（避免触发原生“另存为”对话框），
 *                 再 File → Save Scene 落盘，断言文件确实写出。
 * 断言以 SceneTabManager 真实状态 + 磁盘文件为准；落临时目录，跑前清场。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_scene_tabs.h"
#include "../editor_scene_io.h"           // SaveScene / LoadScene
#include "../editor_shared_components.h"  // EditorNameComponent
#include "../editor_icons.h"

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent（全局命名空间）

namespace dse::editor::uitest {

namespace {
namespace fs = std::filesystem;

#define DSE_MENU_NEW_SCENE  "File/" MDI_ICON_PLUS         "  New Scene"
#define DSE_MENU_SAVE_SCENE "File/" MDI_ICON_CONTENT_SAVE "  Save Scene"

entt::registry& SReg() { return Services().engine->pipeline()->world().registry(); }

} // namespace

void RegisterSceneTests(ImGuiTestEngine* e) {
    // dse-scene/new_scene：File → New Scene → 新建空场景页签（清空 registry）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "new_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();
            const int tabs0 = tabs.GetTabCount();
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), tabs0 + 1);
            IM_CHECK(tabs.GetActiveFilePath().empty());  // 新场景为 Untitled（无路径）
            IM_CHECK_EQ(CountValidEntities(), 0);        // 新空场景：registry 已清空
        };
    }

    // dse-scene/save_scene：把当前页签路径设到临时文件后 File → Save Scene，断言文件写出。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "save_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path dir   = fs::temp_directory_path() / "dse_ui_tests";
            const fs::path scene = dir / "ui_test_save_scene.json";
            std::error_code ec;
            fs::create_directories(dir, ec);
            fs::remove(scene, ec);

            // 设定当前页签路径 → Save Scene 走“直接落盘”分支（不弹原生对话框）。
            SceneTabManager::Get().SetCurrentPath(scene.string());

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_SAVE_SCENE);
            ctx->Yield(2);

            IM_CHECK(fs::exists(scene));
        };
    }

    // dse-scene/persistence_roundtrip：新建空场景 → 造一个带特征名/位置的实体 → Save Scene 落盘 →
    // 再 New Scene 清空（确认实体没了）→ LoadScene 重新加载 → 断言该实体的名字与 position.x 被如实还原。
    // 验证“保存→新建→重开”不仅文件存在，而是内容真的被正确序列化/反序列化。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "persistence_roundtrip");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path dir   = fs::temp_directory_path() / "dse_ui_tests";
            const fs::path scene = dir / "ui_test_roundtrip.json";
            std::error_code ec;
            fs::create_directories(dir, ec);
            fs::remove(scene, ec);
            fs::remove(fs::path(scene.string() + ".bin"), ec);  // 清掉二进制旁路缓存

            auto& tabs = SceneTabManager::Get();

            // 干净起点：新建空场景。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);

            // 造一个特征实体（名字 + position.x），作为序列化往返的探针。
            entt::registry& reg = SReg();
            const entt::entity probe = reg.create();
            reg.emplace<EditorNameComponent>(probe, std::string("DSERoundTripProbe"));
            auto& tf = reg.emplace<TransformComponent>(probe);
            tf.position.x = 12.25f;
            ctx->Yield();

            // 落盘（设当前页签路径 → Save Scene 直接写盘，不弹原生对话框）。
            tabs.SetCurrentPath(scene.string());
            ctx->MenuClick(DSE_MENU_SAVE_SCENE);
            ctx->Yield(2);
            IM_CHECK(fs::exists(scene));

            // 新建空场景，确认探针实体已不在当前 registry。
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);
            {
                bool still_there = false;
                entt::registry& r2 = SReg();
                for (auto en : r2.view<EditorNameComponent>())
                    if (r2.get<EditorNameComponent>(en).name == "DSERoundTripProbe") { still_there = true; break; }
                IM_CHECK(!still_there);
            }

            // 重新加载刚才保存的场景，断言探针被如实还原。
            entt::registry& r3 = SReg();
            LoadScene(r3, scene.string());
            ctx->Yield(2);

            bool restored = false;
            for (auto en : r3.view<EditorNameComponent, TransformComponent>()) {
                if (r3.get<EditorNameComponent>(en).name == "DSERoundTripProbe" &&
                    std::abs(r3.get<TransformComponent>(en).position.x - 12.25f) < 0.01f) {
                    restored = true;
                    break;
                }
            }
            IM_CHECK(restored);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
