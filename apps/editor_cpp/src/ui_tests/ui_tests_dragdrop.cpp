/**
 * @file ui_tests_dragdrop.cpp
 * @brief 拖拽（Drag & Drop）用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 在 Hierarchy 面板里把实体 A 拖到实体 B 上做“重新父子化”（reparent），断言 A 获得
 * ParentComponent 且 parent == B，验证 ImGui 拖拽通路真实生效且不崩。
 *
 * 注：Hierarchy 的实体树节点用指针 ID（TreeNodeEx((void*)entity, ...)），无法按标签定位，
 * 故用测试引擎的 "$$(ptr)0x...." 语法按指针 ID 引用（等价 PushID(void*)）。
 * 路径为 //Hierarchy/Scene/<entity>（根实体绘制在 "Scene" 树节点下）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent / ParentComponent（全局命名空间）
#include "engine/ecs/components_3d_render.h"  // dse::MeshRendererComponent

#include "../editor_shared_components.h"  // SiblingIndexComponent（dse::editor）
#include "../editor_selection.h"          // SelectionManager
#include "../editor_icons.h"              // MDI_ICON_FILE_OUTLINE
#include "../editor_scene_tabs.h"         // SceneTabManager（新建/关闭空场景页签做隔离）

namespace dse::editor::uitest {

namespace {

// 在 Hierarchy 里建一个新的根实体（右键 → Create Empty Entity），返回新建实体。
entt::entity CreateRootEntity(ImGuiTestContext* ctx, entt::registry& reg) {
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

} // namespace

void RegisterDragDropTests(ImGuiTestEngine* e) {
    // dse-dragdrop/hierarchy_reparent：建 A、B 两个根实体，把 A 拖到 B 上 → A 成为 B 的子节点。
    ImGuiTest* t = IM_REGISTER_TEST(e, "dse-dragdrop", "hierarchy_reparent");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        entt::registry& reg = Services().engine->pipeline()->world().registry();

        auto snapshot = [&reg]() {
            std::vector<entt::entity> v;
            for (auto en : reg.storage<entt::entity>())
                if (reg.valid(en)) v.push_back(en);
            return v;
        };
        auto new_one = [](const std::vector<entt::entity>& before,
                          const std::vector<entt::entity>& after) -> entt::entity {
            for (auto en : after) {
                bool seen = false;
                for (auto b : before) if (b == en) { seen = true; break; }
                if (!seen) return en;
            }
            return entt::null;
        };

        // dse-panels/* 会把全部“可选”面板开关置真，这些面板首帧以浮动窗出现并压在 Hierarchy
        // 上（实测 "Lua Console" 浮窗盖住 Hierarchy 节点，致拖拽落点命中该浮窗而非节点）。先全部
        // 关掉并等布局收敛，确保 Hierarchy 节点不被遮挡——本用例依赖屏幕坐标投递拖拽。
        HideOptionalPanels();
        ctx->Yield(4);

        const std::vector<entt::entity> s0 = snapshot();
        OpenHierarchyContextMenu(ctx);
        ctx->ItemClick("Create Empty Entity");
        ctx->Yield();
        const std::vector<entt::entity> s1 = snapshot();
        const entt::entity a = new_one(s0, s1);

        OpenHierarchyContextMenu(ctx);
        ctx->ItemClick("Create Empty Entity");
        ctx->Yield();
        const std::vector<entt::entity> s2 = snapshot();
        const entt::entity b = new_one(s1, s2);

        IM_CHECK(a != entt::null && b != entt::null && a != b);

        // 创建会选中实体 → 视口绘制 ImGuizmo 的全屏 "gizmo" 覆盖窗，会挡住 Hierarchy 的拖拽落点。
        // 先 Edit/Deselect All 清空选择（同时清 context.selected_entity）移除该覆盖窗。
        ctx->SetRef("//DSEngineRoot");
        ctx->MenuClick("Edit/Deselect All");
        ctx->Yield(2);

        // 指针 ID 引用：值即 (void*)(uintptr_t)entity，与 editor_hierarchy_panel 的 TreeNodeEx 一致。
        char src_ref[96], dst_ref[96];
        std::snprintf(src_ref, sizeof(src_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                      static_cast<unsigned long long>(static_cast<std::uint32_t>(a)));
        std::snprintf(dst_ref, sizeof(dst_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                      static_cast<unsigned long long>(static_cast<std::uint32_t>(b)));

        ctx->WindowFocus("//Hierarchy");
        // 前序用例会持续往场景里累积实体，Hierarchy 树可能超出面板高度，导致刚新建的 A/B 节点
        // 被裁剪/滚出可视区——按屏幕坐标投递拖拽就会落空。先把目标节点滚动入视口（A/B 相邻，
        // 滚到下面那个即可让两者同时可见），再读其屏幕矩形。
        ctx->ScrollToItemY(dst_ref);
        ctx->Yield(2);
        const ImGuiTestItemInfo si = ctx->ItemInfo(src_ref);
        const ImGuiTestItemInfo di = ctx->ItemInfo(dst_ref);
        IM_CHECK(si.ID != 0 && di.ID != 0);
        // 落点取目标节点矩形“上四分之一”而非正中：节点正下方紧贴一条 InvisibleButton 的
        // “插入兄弟”落区（reorder，对根实体相当于 detach → 不产生 ParentComponent），偏上
        // 可稳稳命中节点本体的 reparent 落区。
        const ImVec2 src_pos(si.RectFull.GetCenter().x, si.RectFull.Min.y + si.RectFull.GetHeight() * 0.5f);
        const ImVec2 dst_pos(di.RectFull.GetCenter().x, di.RectFull.Min.y + di.RectFull.GetHeight() * 0.25f);

        // 清掉可能残留的 ActiveID（前序用例的输入框可能留下黏滞 active id，会吃掉本次 MouseDown）。
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield();

        ManualMouseDrag(ctx, src_pos, dst_pos);

        IM_CHECK(reg.valid(a) && reg.valid(b));
        IM_CHECK(reg.all_of<ParentComponent>(a));
        IM_CHECK(reg.get<ParentComponent>(a).parent == b);
    };

    // dse-dragdrop/hierarchy_reorder：把 A 拖到 B 节点正下方那条“插入兄弟”落区（##insert_<B>），
    // A 应与 B 同父（此处皆为根，故 A 保持根：无 ParentComponent），并写入 sibling_index = B.index+1。
    // 先把 B 的 SiblingIndexComponent.index 置为已知值，便于断言落区计算出的“目标之后”序号。
    {
        ImGuiTest* t2 = IM_REGISTER_TEST(e, "dse-dragdrop", "hierarchy_reorder");
        t2->TestFunc = [](ImGuiTestContext* ctx) {
            entt::registry& reg = Services().engine->pipeline()->world().registry();

            HideOptionalPanels();
            ctx->Yield(4);

            const entt::entity a = CreateRootEntity(ctx, reg);
            const entt::entity b = CreateRootEntity(ctx, reg);
            IM_CHECK(a != entt::null && b != entt::null && a != b);

            // 给 B 一个已知兄弟序，使“插入到 B 之后”= B.index+1 有确定预期值。
            reg.emplace_or_replace<SiblingIndexComponent>(b).index = 5;
            // A 起始不应带兄弟序（验证 reorder 落区确实写入了它）。
            if (reg.all_of<SiblingIndexComponent>(a)) reg.remove<SiblingIndexComponent>(a);

            // 清空选择移除 ImGuizmo 全屏覆盖窗，避免挡住拖拽落点。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield(2);

            char src_ref[96], ins_ref[96];
            std::snprintf(src_ref, sizeof(src_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(a)));
            // 插入落区是 InvisibleButton，id = "##insert_<uint32(entity)>"（见 editor_hierarchy_panel）。
            std::snprintf(ins_ref, sizeof(ins_ref), "//Hierarchy/Scene/##insert_%u",
                          static_cast<unsigned>(static_cast<std::uint32_t>(b)));

            ctx->WindowFocus("//Hierarchy");
            // 累积实体可能令 A/插入落区被裁剪/滚出可视区；先滚动插入落区入视口再读屏幕矩形。
            ctx->ScrollToItemY(ins_ref);
            ctx->Yield(2);
            const ImGuiTestItemInfo si = ctx->ItemInfo(src_ref);
            const ImGuiTestItemInfo ii = ctx->ItemInfo(ins_ref);
            IM_CHECK(si.ID != 0 && ii.ID != 0);
            const ImVec2 src_pos(si.RectFull.GetCenter().x, si.RectFull.Min.y + si.RectFull.GetHeight() * 0.5f);
            const ImVec2 dst_pos = ii.RectFull.GetCenter();

            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield();

            ManualMouseDrag(ctx, src_pos, dst_pos);

            IM_CHECK(reg.valid(a) && reg.valid(b));
            // 同根：A 仍为根（detach），不应获得 ParentComponent 指向某父。
            IM_CHECK(!reg.all_of<ParentComponent>(a) ||
                     reg.get<ParentComponent>(a).parent == entt::null);
            // reorder 落区显式写入兄弟序 = 目标之后。
            IM_CHECK(reg.all_of<SiblingIndexComponent>(a));
            IM_CHECK_EQ(reg.get<SiblingIndexComponent>(a).index, 6);
        };
    }

    // dse-dragdrop/hierarchy_unparent_to_root：把已是子节点的实体拖回 "Scene" 根 → 卸掉 ParentComponent。
    // 与 hierarchy_reparent（拖成子节点）互为对偶，覆盖 un-parent 落区（Scene 根节点上接 HIERARCHY_ENTITY
    // 的 BeginDragDropTarget → ReparentViaBus(parent=nullopt) → reg.remove<ParentComponent>）。
    {
        ImGuiTest* t4 = IM_REGISTER_TEST(e, "dse-dragdrop", "hierarchy_unparent_to_root");
        t4->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();
            entt::registry& reg = Services().engine->pipeline()->world().registry();

            HideOptionalPanels();
            ctx->Yield(4);

            // 新建空场景页签做隔离：拖拽按屏幕坐标投递，要求“被拖的嵌套子节点”与落区“Scene 根节点”
            // 同屏可见。全套运行时前序用例会往场景累积大量实体，长列表会把 Scene 根（列表首行）与
            // 嵌套子节点（列表末尾）错开到视口上下两端而无法同屏。清空场景后列表仅 Scene>parent>child
            // 三行，二者必然同屏。结尾关掉该页签即恢复原场景快照，无污染。
            const int n0 = tabs.GetTabCount();
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0 + 1);

            const entt::entity parent = CreateRootEntity(ctx, reg);
            const entt::entity child = CreateRootEntity(ctx, reg);
            IM_CHECK(parent != entt::null && child != entt::null && parent != child);

            // 清选择移除 ImGuizmo 全屏覆盖窗，避免挡住拖拽落点。
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Deselect All");
            ctx->Yield(2);

            char parent_ref[96], child_ref[96], nested_ref[160];
            std::snprintf(parent_ref, sizeof(parent_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(parent)));
            std::snprintf(child_ref, sizeof(child_ref), "//Hierarchy/Scene/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(child)));

            // 第一步：把 child 拖到 parent 节点上 → 经 ReparentViaBus 真正成为子节点（与 hierarchy_reparent
            // 同一产品路径，确保 parent 节点产生可展开的子项，再做后续 un-parent）。
            ctx->WindowFocus("//Hierarchy");
            ctx->ScrollToItemY(child_ref);
            ctx->Yield(2);
            {
                const ImGuiTestItemInfo pi = ctx->ItemInfo(parent_ref);
                const ImGuiTestItemInfo ci = ctx->ItemInfo(child_ref);
                IM_CHECK(pi.ID != 0 && ci.ID != 0);
                const ImVec2 src(ci.RectFull.GetCenter().x, ci.RectFull.Min.y + ci.RectFull.GetHeight() * 0.5f);
                const ImVec2 dst(pi.RectFull.GetCenter().x, pi.RectFull.Min.y + pi.RectFull.GetHeight() * 0.25f);
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield();
                ManualMouseDrag(ctx, src, dst);
            }
            IM_CHECK(reg.all_of<ParentComponent>(child));
            IM_CHECK(reg.get<ParentComponent>(child).parent == parent);

            // 第二步：展开 parent 露出嵌套 child，把它拖到 "Scene" 根节点 → detach（卸 ParentComponent）。
            // 多 Yield 几帧让 Hierarchy 以「parent 现有子节点」重绘——刚 reparent 完只过 1 帧时，
            // ImGui 测试引擎缓存的 parent 节点标志仍是上一帧的 Leaf，ItemOpen 会误判为叶子无法展开。
            ctx->Yield(4);
            ctx->ItemOpen(parent_ref);
            ctx->Yield(2);
            // child 现在嵌套在 parent 下：路径多一层 parent 的指针 ID。
            std::snprintf(nested_ref, sizeof(nested_ref), "//Hierarchy/Scene/$$(ptr)0x%llx/$$(ptr)0x%llx",
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(parent)),
                          static_cast<unsigned long long>(static_cast<std::uint32_t>(child)));
            ctx->ScrollToItemY(nested_ref);
            ctx->Yield(2);
            {
                const ImGuiTestItemInfo ni = ctx->ItemInfo(nested_ref);
                const ImGuiTestItemInfo si = ctx->ItemInfo("//Hierarchy/Scene");
                IM_CHECK(ni.ID != 0 && si.ID != 0);
                const ImVec2 src(ni.RectFull.GetCenter().x, ni.RectFull.Min.y + ni.RectFull.GetHeight() * 0.5f);
                // 落点取 "Scene" 根节点（其 BeginDragDropTarget 接 HIERARCHY_ENTITY → ReparentViaBus(nullopt)）。
                const ImVec2 dst = si.RectFull.GetCenter();
                ctx->KeyPress(ImGuiKey_Escape);
                ctx->Yield();
                ManualMouseDrag(ctx, src, dst);
            }

            IM_CHECK(reg.valid(child));
            // un-parent 后 ParentComponent 被卸掉（detach 回根）。
            IM_CHECK(!reg.all_of<ParentComponent>(child));

            // 复位：右键关掉新场景页签 → 恢复原场景快照（parent/child 随该页签一并丢弃）。
            const int last = tabs.GetTabCount() - 1;
            char tabref[80];
            std::snprintf(tabref, sizeof(tabref), "//DSEngineRoot/##SceneTabs/SceneTab%d", last);
            ctx->ItemClick(tabref, ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0);
        };
    }

    // dse-dragdrop/asset_drop_on_inspector_field：把 Project 列表里的 .obj 资源拖到 Inspector 的
    // MeshRenderer 组件「Mesh Path」字段（ASSET_PATH 投放目标）→ 断言 MeshRendererComponent.mesh_path 被写为该资源路径。
    // 覆盖“资源 → Inspector 字段”这条拖放链路（此前只覆盖了资源 → Hierarchy/Scene）。
    {
        ImGuiTest* t3 = IM_REGISTER_TEST(e, "dse-dragdrop", "asset_drop_on_inspector_field");
        t3->TestFunc = [](ImGuiTestContext* ctx) {
            namespace fs = std::filesystem;
            entt::registry& reg = Services().engine->pipeline()->world().registry();

            // 在项目资产根目录放一个 .obj 资源（Mesh Path 字段接受 .obj/.fbx/.gltf/.glb/.dae）。
            const char* fname = "ui_test_drag.obj";
            const fs::path asset = fs::path(ProjectAssetBaseDir()) / fname;
            {
                std::ofstream ofs(asset.string(), std::ios::trunc);
                ofs << "# ui test drag asset\n";
            }

            // 造一个带 MeshRenderer 的实体并单选它（创建即选中 → Inspector 渲染 Mesh Renderer 区）。
            // 注意：这里“先建实体（经右键 Hierarchy 菜单，会遗留非根 ref）再浮动 Project”正是此前
            // 触发 UndockWindow 空指针崩溃的复现序——MakeProjectPanelFloating 已修为先复位 ref+绝对名解析，
            // 故本用例同时充当该崩溃的回归（修复前此序必崩 0xC0000005，修复后稳定通过）。
            const entt::entity ent = CreateRootEntity(ctx, reg);
            IM_CHECK(ent != entt::null);
            reg.emplace_or_replace<dse::MeshRendererComponent>(ent).mesh_path = "";
            SelectionManager::Get().Clear();  // 单选 Inspector 分支（context.selected_entity 仍为 ent）
            ctx->Yield(2);

            // 浮动 Project 列表并挪到左下，避免压住右侧停靠的 Inspector（拖放落点须可命中 Inspector 字段）。
            MakeProjectPanelFloating(ctx);
            ctx->WindowMove("//Project", ImVec2(10.0f, 360.0f));
            ctx->WindowResize("//Project", ImVec2(380.0f, 340.0f));
            ctx->Yield(2);

            // 源：Project 列表项（Table("project_list") -> PushID(filename) -> Selectable("<icon>  <filename>")）。
            const std::string asset_ref =
                std::string("//Project/project_list/") + fname + "/" +
                MDI_ICON_FILE_OUTLINE + "  " + fname;
            // 目标：Inspector 里 Mesh Renderer 组件的「Mesh Path」输入框（其上挂 ASSET_PATH 投放目标）。
            const char* field_ref = "//Inspector/##mesh_path";

            const ImGuiTestItemInfo ai = ctx->ItemInfo(asset_ref.c_str());
            const ImGuiTestItemInfo fi = ctx->ItemInfo(field_ref);
            IM_CHECK(ai.ID != 0);
            IM_CHECK(fi.ID != 0);

            ctx->ItemDragAndDrop(asset_ref.c_str(), field_ref);
            ctx->Yield(2);

            // 落放后：mesh_path 被写为该资源的相对路径（Project 投放 payload 携带相对 base 的路径），且以 .obj 结尾。
            IM_CHECK(reg.valid(ent) && reg.all_of<dse::MeshRendererComponent>(ent));
            const std::string& mp = reg.get<dse::MeshRendererComponent>(ent).mesh_path;
            IM_CHECK(!mp.empty());
            IM_CHECK(mp.find(fname) != std::string::npos);

            RestoreProjectPanelDock(ctx);
            std::error_code ec;
            fs::remove(asset, ec);
            // 经编辑器正规删除路径清掉实体（会把 selected_entity 置空，避免悬挂选择残留到收尾）。
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(2);
            IM_CHECK(!reg.valid(ent));
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
