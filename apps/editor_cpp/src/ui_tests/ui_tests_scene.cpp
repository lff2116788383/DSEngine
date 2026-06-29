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
#include "../editor_settings.h"           // LoadEditorSettings / SaveEditorSettings / AddRecentFile
#include "../editor_icons.h"

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent（全局命名空间）
#include "engine/ecs/components_3d_render.h"  // dse::MeshRendererComponent / dse::PointLightComponent

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

    // dse-scene/persistence_multi_component_roundtrip：在一个干净场景里造两个实体、各带不同组件类型
    // （A=Transform+Name+MeshRenderer.metallic；B=Transform+Name+PointLight.intensity）→ Save Scene 落盘
    // → New Scene 清空 → LoadScene 重开 → 断言两个实体的名字/坐标/各自组件字段都被如实还原。
    // 比 persistence_roundtrip（单实体、单字段）更进一步：多实体 + 多组件类型一起序列化往返。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "persistence_multi_component_roundtrip");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path dir   = fs::temp_directory_path() / "dse_ui_tests";
            const fs::path scene = dir / "ui_test_multi_roundtrip.json";
            std::error_code ec;
            fs::create_directories(dir, ec);
            fs::remove(scene, ec);
            fs::remove(fs::path(scene.string() + ".bin"), ec);  // 清掉二进制旁路缓存，强制走 JSON 往返

            auto& tabs = SceneTabManager::Get();

            // 干净起点。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);

            // 探针 A：Name + Transform + MeshRenderer（metallic 是会被序列化的字段）。
            entt::registry& reg = SReg();
            const entt::entity a = reg.create();
            reg.emplace<EditorNameComponent>(a, std::string("DSEMultiA"));
            auto& ta = reg.emplace<TransformComponent>(a);
            ta.position = glm::vec3(1.0f, 2.0f, 3.0f);
            auto& mr = reg.emplace<dse::MeshRendererComponent>(a);
            mr.metallic = 0.66f;

            // 探针 B：Name + Transform + PointLight（intensity 会被序列化）。
            const entt::entity b = reg.create();
            reg.emplace<EditorNameComponent>(b, std::string("DSEMultiB"));
            auto& tb = reg.emplace<TransformComponent>(b);
            tb.position = glm::vec3(4.0f, 5.0f, 6.0f);
            auto& pl = reg.emplace<dse::PointLightComponent>(b);
            pl.intensity = 3.5f;
            ctx->Yield();

            // 落盘。
            tabs.SetCurrentPath(scene.string());
            ctx->MenuClick(DSE_MENU_SAVE_SCENE);
            ctx->Yield(2);
            IM_CHECK(fs::exists(scene));

            // 清场：New Scene，确认两个探针都不在当前 registry。
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);
            {
                entt::registry& r2 = SReg();
                bool any = false;
                for (auto en : r2.view<EditorNameComponent>()) {
                    const std::string& n = r2.get<EditorNameComponent>(en).name;
                    if (n == "DSEMultiA" || n == "DSEMultiB") { any = true; break; }
                }
                IM_CHECK(!any);
            }

            // 重开并断言：A 的坐标 + metallic、B 的坐标 + intensity 都如实还原。
            entt::registry& r3 = SReg();
            LoadScene(r3, scene.string());
            ctx->Yield(2);

            bool a_ok = false, b_ok = false;
            for (auto en : r3.view<EditorNameComponent, TransformComponent>()) {
                const std::string& n = r3.get<EditorNameComponent>(en).name;
                const auto& tf = r3.get<TransformComponent>(en);
                if (n == "DSEMultiA" &&
                    std::abs(tf.position.x - 1.0f) < 0.01f &&
                    std::abs(tf.position.z - 3.0f) < 0.01f &&
                    r3.all_of<dse::MeshRendererComponent>(en) &&
                    std::abs(r3.get<dse::MeshRendererComponent>(en).metallic - 0.66f) < 0.01f) {
                    a_ok = true;
                }
                if (n == "DSEMultiB" &&
                    std::abs(tf.position.x - 4.0f) < 0.01f &&
                    std::abs(tf.position.y - 5.0f) < 0.01f &&
                    r3.all_of<dse::PointLightComponent>(en) &&
                    std::abs(r3.get<dse::PointLightComponent>(en).intensity - 3.5f) < 0.01f) {
                    b_ok = true;
                }
            }
            IM_CHECK(a_ok);
            IM_CHECK(b_ok);
        };
    }

    // dse-scene/open_via_recent_scenes_menu：保存一个带探针实体的场景 → 写入「最近场景」列表 →
    // 新建空场景清掉探针 → 走 File → Recent Scenes → <文件名> 菜单真实打开 → 断言当前页签切到该
    // 场景且探针被如实还原。覆盖此前未测的“通过菜单打开已有场景”链路（Open Scene 走原生对话框
    // 无法自动化，Recent Scenes 是可自动化的等效打开路径）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "open_via_recent_scenes_menu");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path dir   = fs::temp_directory_path() / "dse_ui_tests";
            const fs::path scene = dir / "ui_test_recent_open.json";
            std::error_code ec;
            fs::create_directories(dir, ec);
            fs::remove(scene, ec);
            fs::remove(fs::path(scene.string() + ".bin"), ec);

            auto& tabs = SceneTabManager::Get();
            const int tab_count0 = tabs.GetTabCount();  // 记录起始页签数，收尾恢复

            // 干净起点 → 造探针实体 → 直接 SaveScene 落盘（不经 SetCurrentPath，避免让该路径
            // 成为“已打开页签”——否则 Recent 点击会走 OpenScene 的快照复用分支而非真正从磁盘加载，
            // 全套下受页签切换时序影响而不稳）。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);
            {
                entt::registry& reg = SReg();
                const entt::entity probe = reg.create();
                reg.emplace<EditorNameComponent>(probe, std::string("DSERecentProbe"));
                reg.emplace<TransformComponent>(probe).position.x = 8.75f;
                SaveScene(reg, scene.string());
            }
            ctx->Yield();
            IM_CHECK(fs::exists(scene));

            // 写入「最近场景」并持久化（File 菜单每帧从设置读取该列表）。
            EditorSettings settings = LoadEditorSettings();
            AddRecentFile(settings, scene.string());
            SaveEditorSettings(settings);

            // 新建空场景清掉探针，确认它确实已不在当前 registry。
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);
            {
                bool still = false;
                entt::registry& r = SReg();
                for (auto en : r.view<EditorNameComponent>())
                    if (r.get<EditorNameComponent>(en).name == "DSERecentProbe") { still = true; break; }
                IM_CHECK(!still);
            }

            // 走 File → Recent Scenes → <文件名> 菜单真实打开该场景。
            const std::string recent_item =
                std::string("File/Recent Scenes/") + scene.filename().string();
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(recent_item.c_str());

            // 断言：Recent 菜单触发 OpenScene(path)，场景被打开、激活并从磁盘加载出探针。
            // 页签 ID 已稳定化（###SceneTab<stableId>），多页签下打开场景内容不再串台；轮询数帧让
            // 打开/激活落定后即应稳定见到探针。
            bool restored = false;
            for (int i = 0; i < 12 && !restored; ++i) {
                ctx->Yield();
                entt::registry& r = SReg();
                for (auto en : r.view<EditorNameComponent, TransformComponent>()) {
                    if (r.get<EditorNameComponent>(en).name == "DSERecentProbe" &&
                        std::abs(r.get<TransformComponent>(en).position.x - 8.75f) < 0.01f) {
                        restored = true;
                        break;
                    }
                }
            }
            IM_CHECK(tabs.FindTabByPath(scene.string()) >= 0);
            IM_CHECK(fs::path(tabs.GetActiveFilePath()).filename() == scene.filename());
            IM_CHECK(restored);

            // 收尾：关掉本用例新增的页签恢复起始页签数（从末尾关，不动前面索引），
            // 否则会抬高全套页签总数、触发依赖绝对页签索引的 dse-tabs 用例在多页签下的切换不稳。
            while (tabs.GetTabCount() > tab_count0) {
                tabs.CloseTab(tabs.GetTabCount() - 1, SReg());
            }
            ctx->Yield(2);

            // 删除临时场景文件及二进制旁路缓存。
            fs::remove(scene, ec);
            fs::remove(fs::path(scene.string() + ".bin"), ec);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
