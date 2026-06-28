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

#include "../editor_prefab.h"            // IsPrefabInstance
#include "../editor_prefab_marker.h"     // PrefabMarkerComponent
#include "../editor_prefab_override.h"   // ComputePrefabOverrides
#include "../editor_selection.h"         // SelectionManager
#include "../editor_icons.h"             // MDI_ICON_CUBE_OUTLINE

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

    // dse-prefab/apply_to_prefab：实例化 → 改 Transform.X 制造 override → 点「Apply to Prefab」把实例
    // 当前值写回源 .dprefab → 断言①override 归零（实例已与源一致）②源文件磁盘内容确实更新到新值。
    // 这是 override_revert 的对偶闭环：Revert 拉回源值，Apply 把改动推回源文件。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-prefab", "apply_to_prefab");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const char* fname = "ui_test_apply.dprefab";
            WriteTestPrefab(fname, 5.0f, 0.0f, 0.0f);  // 源 position.x = 5
            const fs::path file = fs::path(ProjectAssetBaseDir()) / fname;

            MakeProjectPanelFloating(ctx);
            DragProjectAssetOntoScene(ctx, fname, MDI_ICON_CUBE_OUTLINE);
            RestoreProjectPanelDock(ctx);

            entt::entity inst = FindPrefabInstance("ui_test_apply");
            IM_CHECK(inst != entt::null);
            SelectionManager::Get().Clear();  // 单选 Inspector 分支
            ctx->Yield(2);

            // 改 position.x = 77 → 与源值 5 产生 override。
            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 77.0f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<TransformComponent>(inst).position.x - 77.0f) < 0.01f);
            // 此刻应存在 override（实例偏离源）。
            IM_CHECK(!ComputePrefabOverrides(Reg(), inst).overrides.empty());

            // 点「Apply to Prefab」：把实例当前值写回源 .dprefab（DrawPrefabOverrideSection 内按钮）。
            ctx->ItemClick("//Inspector/Apply to Prefab");
            ctx->Yield(2);

            // ① 应用后实例与源一致 → override 归零。
            IM_CHECK(ComputePrefabOverrides(Reg(), inst).overrides.empty());

            // ② 源文件磁盘内容已更新到新 X（写回是真落盘，不只是内存）。
            {
                std::ifstream ifs(file.string());
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                IM_CHECK(content.find("77") != std::string::npos);
                IM_CHECK(content.find("\"dprefab\"") != std::string::npos);
            }

            std::error_code ec;
            fs::remove(file, ec);
        };
    }

    // dse-prefab/override_revert_per_field：逐项 Revert（区别于 override_revert 的「Revert All」）。
    // 制造两个 Transform override（Position.x、Scale.x），只点 Scale 行的小「Revert」按钮 →
    // 断言①Scale 回退到源值②Position 覆写仍保留③override 数从 2 → 1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-prefab", "override_revert_per_field");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const char* fname = "ui_test_ovrf.dprefab";
            WriteTestPrefab(fname, 5.0f, 0.0f, 0.0f);  // 源 position.x=5, scale=(1,1,1)

            MakeProjectPanelFloating(ctx);
            DragProjectAssetOntoScene(ctx, fname, MDI_ICON_CUBE_OUTLINE);
            RestoreProjectPanelDock(ctx);

            entt::entity inst = FindPrefabInstance("ui_test_ovrf");
            IM_CHECK(inst != entt::null);
            SelectionManager::Get().Clear();  // 单选 Inspector 分支
            ctx->Yield(2);

            // Inspector 默认停靠右侧仅约 20% 宽（dock_id_right，见 editor_shell.cpp），override 表
            // 三列（Property 140 + Original 100 + Current）后留给 Current 列的宽度不足，每行末尾的小
            // 「Revert」按钮被列裁剪掉、不可命中。这里临时把 Inspector 浮动放大，确保该按钮完整可点；
            // 结尾再把 Inspector 作为标签页停回右侧节点（与 Material 同节点）复位布局。
            // UndockWindow/DockInto 的窗口名按当前 ref 解析，先 SetRef("") 复位到根并用绝对名 "//Inspector"，
            // 规避遗留非根 ref 导致的空指针崩溃（与 MakeProjectPanelFloating 同理）。
            ctx->SetRef("");
            ctx->UndockWindow("//Inspector");
            ctx->Yield(2);
            ctx->WindowMove("//Inspector", ImVec2(300.0f, 80.0f));
            ctx->WindowResize("//Inspector", ImVec2(700.0f, 600.0f));
            ctx->Yield(2);

            // 两个 Transform 字段各产生一个 override 行。ComputePrefabOverrides 固定顺序：
            // Position 在前（行 PushID(0)）、Scale 在后（行 PushID(1)）。
            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 33.0f);
            ctx->Yield();
            ctx->ItemInputValue("##scale_undo/##scale/##x", 2.0f);
            ctx->Yield(2);

            {
                auto info = ComputePrefabOverrides(Reg(), inst);
                IM_CHECK_EQ(static_cast<int>(info.overrides.size()), 2);
                IM_CHECK(info.overrides[0].property_name == "Position");
                IM_CHECK(info.overrides[1].property_name == "Scale");
            }

            // 只点第 2 行（Scale，PushID(1)）的小「Revert」按钮：仅回退该字段。
            ctx->ItemClick("//Inspector/$$1/Revert");
            ctx->Yield(2);

            const auto& tf = Reg().get<TransformComponent>(inst);
            IM_CHECK(std::abs(tf.scale.x - 1.0f) < 0.01f);     // Scale 回退到源值 1
            IM_CHECK(std::abs(tf.position.x - 33.0f) < 0.01f); // Position 覆写保留

            {
                auto info = ComputePrefabOverrides(Reg(), inst);
                IM_CHECK_EQ(static_cast<int>(info.overrides.size()), 1);
                IM_CHECK(info.overrides[0].property_name == "Position");
            }

            // 复位：把 Inspector 停回右侧停靠节点（Material 在该节点），避免污染后续布局相关用例。
            ctx->SetRef("");
            ctx->DockInto("Inspector", "Material");
            ctx->Yield(2);

            std::error_code ec;
            fs::remove(fs::path(ProjectAssetBaseDir()) / fname, ec);
        };
    }

    // dse-prefab/apply_then_reinstantiate_reflects_source：编辑器无「活实例联动」特性，源 .dprefab
    // 文件是唯一真相。改实例 #1 → Apply 回源（源文件落盘新值）→ 再实例化 #2 应从更新后的源读出新值。
    // 断言：实例 #2 的 position.x = 应用后的新值，且无 override（与新源一致）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-prefab", "apply_then_reinstantiate_reflects_source");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const char* fname = "ui_test_reinst.dprefab";
            WriteTestPrefab(fname, 5.0f, 0.0f, 0.0f);  // 源 position.x=5

            // 实例 #1。
            MakeProjectPanelFloating(ctx);
            DragProjectAssetOntoScene(ctx, fname, MDI_ICON_CUBE_OUTLINE);
            RestoreProjectPanelDock(ctx);
            entt::entity inst1 = FindPrefabInstance("ui_test_reinst");
            IM_CHECK(inst1 != entt::null);

            // 改 position.x=88 → Apply 回源 → override 归零（源文件已更新到 88）。
            SelectionManager::Get().Clear();
            ctx->Yield(2);
            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##pos_undo/##pos/##x", 88.0f);
            ctx->Yield(2);
            ctx->ItemClick("//Inspector/Apply to Prefab");
            ctx->Yield(2);
            IM_CHECK(ComputePrefabOverrides(Reg(), inst1).overrides.empty());

            // 记录现有 prefab 实例集合，再实例化 #2（从已更新的源文件）。
            std::vector<entt::entity> before;
            {
                auto view = Reg().view<dse::editor::PrefabMarkerComponent>();
                for (auto en : view) before.push_back(en);
            }

            MakeProjectPanelFloating(ctx);
            DragProjectAssetOntoScene(ctx, fname, MDI_ICON_CUBE_OUTLINE);
            RestoreProjectPanelDock(ctx);

            entt::entity inst2 = entt::null;
            {
                auto view = Reg().view<dse::editor::PrefabMarkerComponent>();
                for (auto en : view) {
                    const auto& m = Reg().get<dse::editor::PrefabMarkerComponent>(en);
                    if (m.source_path.find("ui_test_reinst") == std::string::npos) continue;
                    bool seen = false;
                    for (auto b : before) if (b == en) { seen = true; break; }
                    if (!seen) { inst2 = en; break; }
                }
            }
            IM_CHECK(inst2 != entt::null && inst2 != inst1);

            // 新实例从更新后的源读出新值（源是唯一真相）→ position.x=88 且无 override。
            IM_CHECK(std::abs(Reg().get<TransformComponent>(inst2).position.x - 88.0f) < 0.01f);
            IM_CHECK(ComputePrefabOverrides(Reg(), inst2).overrides.empty());

            std::error_code ec;
            fs::remove(fs::path(ProjectAssetBaseDir()) / fname, ec);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
