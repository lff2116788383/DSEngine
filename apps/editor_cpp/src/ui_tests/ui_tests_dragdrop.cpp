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

namespace dse::editor::uitest {

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

        // 手动拖拽（不走 ItemDragAndDrop）：后者会调 _MakeAimingSpaceOverPos 试图“挪开挡住落点的
        // 窗口”，而 ImGuizmo 每帧 BeginFrame 建的全屏 "gizmo" 窗（NoTitleBar 不可拖动）挪不开，落点
        // 坐标会漂移。改用 MouseMoveToPos 直接定位到目标节点矩形并分步移动：ImGui 拖拽投递需要
        // “源激活→跨帧拖动→落点悬停一帧→释放”，分步并逐帧 Yield 比单帧瞬移更可靠。
        ctx->MouseMoveToPos(src_pos);
        ctx->Yield();
        ctx->MouseDown(ImGuiMouseButton_Left);
        ctx->Yield();
        ctx->MouseLiftDragThreshold();
        ctx->Yield();
        const ImVec2 mid((src_pos.x + dst_pos.x) * 0.5f, (src_pos.y + dst_pos.y) * 0.5f);
        ctx->MouseMoveToPos(mid);
        ctx->Yield();
        ctx->MouseMoveToPos(dst_pos);
        ctx->Yield();
        ctx->MouseMoveToPos(dst_pos);  // 在落点多停一帧，确保目标 BeginDragDropTarget 命中
        ctx->Yield(2);
        ctx->MouseUp(ImGuiMouseButton_Left);
        ctx->Yield(2);

        IM_CHECK(reg.valid(a) && reg.valid(b));
        IM_CHECK(reg.all_of<ParentComponent>(a));
        IM_CHECK(reg.get<ParentComponent>(a).parent == b);
    };
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
