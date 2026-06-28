/**
 * @file ui_tests_prefab.cpp
 * @brief Prefab 工作流真实控件级用例（仅 DSE_EDITOR_UI_TESTS 编入）。补缺口①。
 *
 *   - save_as_prefab        ：选中实体 → Hierarchy 右键「Save as Prefab」→ 断言 .dprefab 落盘且可解析。
 *   - instantiate_via_drag  ：把项目目录里的 .dprefab 从 Project 列表拖到 Hierarchy "Scene" 根 →
 *                             断言场景 +1 且新实体带 PrefabMarkerComponent（IsPrefabInstance）。
 *   - override_revert       ：实例化后改 Transform.X → Inspector 出现 override → 点「Revert All」→
 *                             断言 position 还原回源 prefab 值（override 计算+回退链路成立）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_prefab.h"          // IsPrefabInstance
#include "../editor_prefab_marker.h"   // PrefabMarkerComponent
#include "../editor_selection.h"       // SelectionManager
#include "../editor_icons.h"           // MDI_ICON_CUBE_OUTLINE

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

namespace dse::editor::uitest {

namespace {
namespace fs = std::filesystem;

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 在项目资产根目录写一个最小 .dprefab（含 name + transform.position），返回文件名。
void WriteTestPrefab(const std::string& filename, float px, float py, float pz) {
    const fs::path file = fs::path(ProjectAssetBaseDir()) / filename;
    std::ofstream ofs(file.string(), std::ios::trunc);
    ofs << "{\n"
        << "  \"type\": \"dprefab\",\n"
        << "  \"version\": 1,\n"
        << "  \"name\": \"UiTestPrefab\",\n"
        << "  \"transform\": {\n"
        << "    \"position\": [" << px << ", " << py << ", " << pz << "],\n"
        << "    \"rotation\": [1, 0, 0, 0],\n"
        << "    \"scale\": [1, 1, 1]\n"
        << "  }\n"
        << "}\n";
}

// 查找 source_path 含给定子串的 prefab 实例实体（拖拽实例化后定位新实例）。
entt::entity FindPrefabInstance(const char* path_substr) {
    entt::registry& reg = Reg();
    for (auto e : reg.view<dse::editor::PrefabMarkerComponent>()) {
        const auto& m = reg.get<dse::editor::PrefabMarkerComponent>(e);
        if (m.source_path.find(path_substr) != std::string::npos) return e;
    }
    return entt::null;
}

} // namespace

void RegisterPrefabTests(ImGuiTestEngine* e) {
    // dse-prefab/save_as_prefab：右键创建实体（即选中）→ 再右键「Save as Prefab」→ 文件落盘。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-prefab", "save_as_prefab");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");  // 创建即选中 → "New Entity"
            ctx->Yield();

            // 编辑器把 prefab 写到 <cwd>/samples/lua/data/prefabs/<name>.dprefab
            const fs::path prefab_path =
                fs::current_path() / "samples" / "lua" / "data" / "prefabs" / "New Entity.dprefab";
            std::error_code ec;
            fs::remove(prefab_path, ec);  // 清掉上轮残留，确保断言的是这次写入

            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Save as Prefab");
            ctx->Yield(2);

            IM_CHECK(fs::exists(prefab_path));
            // 内容应可解析且首字符是 JSON 对象起始（SaveEntityAsPrefab 写的是 dprefab JSON）。
            std::ifstream ifs(prefab_path.string());
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            IM_CHECK(content.find("\"dprefab\"") != std::string::npos);

            fs::remove(prefab_path, ec);
        };
    }

    // dse-prefab/instantiate_via_drag：项目目录放 .dprefab → 从 Project 列表拖到 Scene 根 → 实例化。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-prefab", "instantiate_via_drag");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const char* fname = "ui_test_inst.dprefab";
            WriteTestPrefab(fname, 1.0f, 2.0f, 3.0f);

            MakeProjectPanelFloating(ctx);

            const int before = CountValidEntities();
            DragProjectAssetOntoScene(ctx, fname, MDI_ICON_CUBE_OUTLINE);
            IM_CHECK_EQ(CountValidEntities(), before + 1);

            entt::entity inst = FindPrefabInstance("ui_test_inst");
            IM_CHECK(inst != entt::null);
            IM_CHECK(IsPrefabInstance(Reg(), inst));

            RestoreProjectPanelDock(ctx);
            std::error_code ec;
            fs::remove(fs::path(ProjectAssetBaseDir()) / fname, ec);
        };
    }

    // dse-prefab/override_revert：实例化 → 改 Transform.X → Inspector「Prefab Instance」出现 override
    // → 点「Revert All」→ position 还原回源值。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-prefab", "override_revert");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const char* fname = "ui_test_ovr.dprefab";
            WriteTestPrefab(fname, 5.0f, 0.0f, 0.0f);  // 源 position.x = 5

            MakeProjectPanelFloating(ctx);
            DragProjectAssetOntoScene(ctx, fname, MDI_ICON_CUBE_OUTLINE);
            RestoreProjectPanelDock(ctx);

            entt::entity inst = FindPrefabInstance("ui_test_ovr");
            IM_CHECK(inst != entt::null);
            // 实例创建即选中；清多选走单选 Inspector 分支（Transform 可编辑 + override 区渲染）。
            SelectionManager::Get().Clear();
            ctx->Yield(2);

            // 改 position.x → 偏离源值 5，制造一个 override。
            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 99.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<TransformComponent>(inst).position.x - 99.0f) < 0.01f);

            // override 非空时 Inspector 渲染「Revert All」按钮（DrawPrefabOverrideSection）。
            ctx->ItemClick("//Inspector/Revert All");
            ctx->Yield(2);

            // 还原后应回到源 prefab 的 position.x = 5。
            IM_CHECK(std::abs(Reg().get<TransformComponent>(inst).position.x - 5.0f) < 0.01f);

            std::error_code ec;
            fs::remove(fs::path(ProjectAssetBaseDir()) / fname, ec);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
