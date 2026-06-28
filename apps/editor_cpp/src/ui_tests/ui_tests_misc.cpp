/**
 * @file ui_tests_misc.cpp
 * @brief ⑨ 杂项编辑器特性补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 三个真实控件级 + 真断言用例，覆盖 相机导航 / 碰撞体编辑 / Gizmo 操作切换：
 *   - scene_camera_focus_selected：菜单 Entity/Focus Selected → 编辑器相机
 *     focal_point 飞到选中实体位置、distance 收到 5（断言 EditorCamera 状态）。
 *   - collider_box3d_edit_fields：Inspector 加 Box Collider 3D，改 Bounciness/Friction
 *     拖条、翻 Is Trigger 勾选框 → 断言 BoxCollider3DComponent 字段被写回。
 *   - view_menu_gizmo_op_mode：View 菜单切 Gizmo 操作（Translate/Rotate/Scale）与
 *     坐标系（Local/World）→ 断言编辑器 current_gizmo_operation / current_gizmo_mode。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_selection.h"        // SelectionManager
#include "../editor_scene_camera.h"     // EditorCamera / GetEditorCamera / FocusEditorCamera
#include "../editor_settings.h"         // Load/SaveEditorSettings（autosave 开关/间隔）
#include "../editor_scene_tabs.h"       // SceneTabManager（New Scene / MarkDirty / 活动页签名）
#include "../editor_project.h"          // ProjectManager（autosave 目录 / Build 资产目录）
#include "../editor_autosave_core.h"    // MakeAutoSaveFileName
#include "../editor_shared_components.h" // EditorNameComponent
#include "../editor_icons.h"            // MDI_ICON_PLUS / MDI_ICON_EXPORT

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"               // TransformComponent（全局命名空间）
#include "engine/ecs/components_3d_physics.h"   // dse::BoxCollider3DComponent

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 右键 Hierarchy → Create Empty Entity（创建即选中），按 registry 差集取回新实体。
entt::entity NewSelectedEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield();
    SelectionManager::Get().Clear();
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) return en;
    }
    return entt::null;
}

// Inspector「Add Component」按钮 → 弹窗里点对应 MenuItem（label 即组件注册名）。
void AddComponent(ImGuiTestContext* ctx, const char* component_name) {
    ctx->WindowFocus("//Inspector");
    ctx->SetRef("//Inspector");
    ctx->ItemClick("Add Component");
    ctx->Yield();
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick(component_name);
    ctx->Yield(2);
    ctx->SetRef("//Inspector");
}

// 经 Hierarchy 右键「Delete Entity」删掉当前选中实体（编辑器正规删除路径）。
void DeleteSelectedEntity(ImGuiTestContext* ctx, entt::entity ent) {
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Delete Entity");
    ctx->Yield(2);
    IM_CHECK(!Reg().valid(ent));
}

} // namespace

void RegisterMiscEditorTests(ImGuiTestEngine* e) {
    // ── ⑨-1 相机导航：菜单 Focus Selected 把相机飞到选中实体 ────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "scene_camera_focus_selected");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);

            entt::registry& reg = Reg();
            if (!reg.all_of<TransformComponent>(ent)) reg.emplace<TransformComponent>(ent);
            reg.get<TransformComponent>(ent).position = glm::vec3(7.0f, 8.0f, 9.0f);

            // 把相机挪到别处，确保 Focus 真的移动了它。
            EditorCamera& cam = GetEditorCamera();
            cam.focal_point = glm::vec3(0.0f);
            cam.distance = 42.0f;

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Entity/Focus Selected");
            ctx->Yield(2);

            IM_CHECK(glm::length(cam.focal_point - glm::vec3(7.0f, 8.0f, 9.0f)) < 0.01f);
            IM_CHECK(std::abs(cam.distance - 5.0f) < 0.01f);

            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ⑨-2 碰撞体编辑：Box Collider 3D 字段在 Inspector 真实控件里改 → 写回 ECS ──
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "collider_box3d_edit_fields");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Box Collider 3D");
            IM_CHECK(Reg().all_of<dse::BoxCollider3DComponent>(ent));

            const bool trig_before = Reg().get<dse::BoxCollider3DComponent>(ent).is_trigger;

            ctx->ItemInputValue("//Inspector/##boxcol3d_bounce", 0.5f);
            ctx->ItemInputValue("//Inspector/##boxcol3d_fric", 0.2f);
            ctx->ItemClick("//Inspector/##boxcol3d_trigger");
            ctx->Yield(2);

            const auto& col = Reg().get<dse::BoxCollider3DComponent>(ent);
            IM_CHECK(std::abs(col.bounciness - 0.5f) < 0.01f);
            IM_CHECK(std::abs(col.friction - 0.2f) < 0.01f);
            IM_CHECK(col.is_trigger != trig_before);

            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ⑥-a autosave 真触发：置脏 + 推进模拟时钟跨过间隔 → .editor/autosave/<scene> 落盘且含场景数据 ─
    // AutoSaveManager::Tick 每帧由 editor_app 调用：从盘读设置、读活动页签 dirty、以 ImGui::GetTime()
    // 为时钟。靠 ctx->SleepNoSkip 注入模拟 DeltaTime 推进 g.Time 跨过间隔（下限 10s）触发一次自动保存。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "autosave_triggers_and_writes_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            namespace fs = std::filesystem;
            auto& proj = ProjectManager::Get();
            IM_CHECK(proj.HasOpenProject());  // UI 测试模式下总有空工程打开
            auto& tabs = SceneTabManager::Get();

            // 起个干净空场景页签，造一个特征命名实体（用于验证 autosave 文件“含场景数据”）。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("File/" MDI_ICON_PLUS "  New Scene");
            ctx->Yield(2);
            entt::registry& reg = Reg();
            const entt::entity probe = reg.create();
            reg.emplace<EditorNameComponent>(probe, std::string("DSEAutoSaveProbe"));
            reg.emplace<TransformComponent>(probe);

            // 期望落盘路径：<project>/.editor/autosave/<sceneName>.autosave.dscene（与管理器同法推导）。
            const fs::path expected = fs::path(proj.GetProjectRoot()) / ".editor" / "autosave" /
                                      MakeAutoSaveFileName(tabs.GetActiveDisplayName());
            std::error_code ec;
            fs::remove(expected, ec);  // 清旧产物，确保断言的是这次触发

            // 开 autosave 并把间隔设到下限（Tick 每帧从盘 LoadEditorSettings）。
            const EditorSettings orig = LoadEditorSettings();
            EditorSettings st = orig;
            st.auto_save_enabled = true;
            st.auto_save_interval_sec = 10;  // ClampAutoSaveInterval 下限
            SaveEditorSettings(st);

            // 置脏 → 过一帧让计时器 InitTimer → 注入约 12s 模拟时钟跨过 10s 间隔 → Tick 落盘。
            tabs.MarkDirty();
            ctx->Yield(2);
            ctx->SleepNoSkip(12.0f, 1.0f);
            ctx->Yield(2);

            // 断言：autosave 文件落盘且含场景数据（特征实体名出现在序列化内容里）。
            IM_CHECK(fs::exists(expected, ec));
            std::string content;
            {
                std::ifstream f(expected, std::ios::binary);
                std::stringstream ss; ss << f.rdbuf(); content = ss.str();
            }
            IM_CHECK(!content.empty());
            IM_CHECK(content.find("DSEAutoSaveProbe") != std::string::npos);

            // 收尾：复原设置、删 autosave 文件（含 .bin 旁车）、新建空场景复位。
            SaveEditorSettings(orig);
            fs::remove(expected, ec);
            fs::remove(fs::path(expected.string() + ".bin"), ec);
            ctx->MenuClick("File/" MDI_ICON_PLUS "  New Scene");
            ctx->Yield(2);
        };
    }

    // ── ⑥-b Build Game 对话框出包：设标题/输出目录 → Build → 轮询完成 → 断言产物落盘 ──────
    // DoBuild 是“拷贝现成 runtime exe”（非编译），在 cwd（仓库根）找 dsengine_game*.exe 拷为
    // <title>.exe。测试环境未构建 dse_standalone，故放一个最小桩 exe 供拷贝（不执行它，只验
    // 编辑器出包编排：找 exe→拷为<title>.exe→写 manifest→打包/拷 data）；另在项目资产目录放
    // 一个探针资产，使 data 打包有可断言内容。构建在后台线程跑，以产物落盘为完成信号。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "build_game_dialog_produces_artifacts");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            namespace fs = std::filesystem;
            auto& proj = ProjectManager::Get();
            IM_CHECK(proj.HasOpenProject());

            const fs::path asset_dir   = proj.GetAssetDir();
            const fs::path probe_asset = asset_dir / "dse_buildtest_probe.txt";
            const fs::path stub_exe    = fs::current_path() / "dsengine_game.exe";
            const fs::path out_dir     = fs::temp_directory_path() / "dse_ui_tests" / "build_out";

            std::error_code ec;
            // 预清（自愈上次可能中途中断的残留）。
            fs::remove_all(out_dir, ec);
            fs::create_directories(asset_dir, ec);
            { std::ofstream(probe_asset, std::ios::binary) << "DSE_BUILD_PROBE"; }
            { std::ofstream(stub_exe, std::ios::binary) << "MZ"; }  // 最小桩，仅供拷贝
            ctx->Yield();

            // 开 Build Game 对话框（File 菜单，editable && has_project 才可点）。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("File/" MDI_ICON_EXPORT "  Build Game...");
            ctx->Yield(2);
            // 模态弹窗用 //$FOCUSED 定位（与本文件 AddComponent 同法）；确认窗口已开。
            IM_CHECK(FindActiveWindow("Build Game") != nullptr);
            ctx->SetRef("//$FOCUSED");
            ctx->ItemInputValue("##title", "TestGame");
            ctx->ItemInputValue("##outdir", out_dir.string().c_str());
            ctx->Yield(2);
            ctx->ItemClick("Build");

            // 轮询完成：构建在后台线程，以编辑器产物（含晚写的 data/）落盘为准。
            const fs::path exe_out  = out_dir / "TestGame.exe";
            const fs::path manifest = out_dir / "game.dsmanifest";
            const fs::path data_out = out_dir / "data";
            bool done = false;
            for (int i = 0; i < 600 && !done; ++i) {
                ctx->Yield();
                done = fs::exists(exe_out, ec) && fs::exists(manifest, ec) && fs::exists(data_out, ec);
            }

            IM_CHECK(fs::exists(exe_out, ec));   // <title>.exe 落盘（由桩拷贝而来）
            IM_CHECK(fs::exists(manifest, ec));  // game.dsmanifest 落盘
            IM_CHECK(fs::exists(data_out, ec));  // data/ 落盘
            IM_CHECK(fs::exists(data_out / "dse_buildtest_probe.txt", ec));  // 资产进了 data
            IM_CHECK(fs::exists(out_dir / "game.dpak", ec));                 // 非空资产 → 写出 pak

            // build_done 后出现 Close 按钮，点它关对话框。
            ctx->ItemClick("Close");
            ctx->Yield(2);

            // 清理：探针资产（含 .meta 旁车）、桩 exe、输出目录。
            fs::remove(probe_asset, ec);
            fs::remove(fs::path(probe_asset.string() + ".meta"), ec);
            fs::remove(stub_exe, ec);
            fs::remove_all(out_dir, ec);
        };
    }

    // ── ⑨-3 View 菜单切 Gizmo 操作/坐标系 → 断言编辑器 gizmo op/mode 状态 ─────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "view_menu_gizmo_op_mode");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            IM_CHECK(Services().current_gizmo_operation != nullptr);
            IM_CHECK(Services().current_gizmo_mode != nullptr);

            // 已知初始态：Translate / Local。
            *Services().current_gizmo_operation = 0;
            *Services().current_gizmo_mode = 0;

            ctx->SetRef("//DSEngineRoot");

            ctx->MenuClick("View/Gizmo: Rotate");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_operation == 1);

            ctx->MenuClick("View/Gizmo: Scale");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_operation == 2);

            ctx->MenuClick("View/World Space");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_mode == 1);

            ctx->MenuClick("View/Local Space");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_mode == 0);

            // 复位到 Translate，避免污染后续依赖默认 gizmo 操作的用例。
            ctx->MenuClick("View/Gizmo: Translate");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_operation == 0);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
