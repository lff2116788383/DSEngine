/**
 * @file ui_tests_graph.cpp
 * @brief ⑥ Shader Graph / Visual Script 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 两个节点图编辑器（editor_visual_script.cpp / editor_shader_graph.cpp）都用 ImDrawList
 * 自绘节点+引脚，交互靠画布内的鼠标命中而非 ImGui item，所以用例走「真实控件 + 屏幕坐标」驱动：
 *   - 加节点：右键画布空白弹「创建节点」菜单（真实 ImGui::MenuItem）→ 点选模板。
 *     两个编辑器都把新节点放在 create_menu_pos（= 右键处 - 画布偏移），故新节点左上角恰好
 *     落在右键屏幕坐标处；引脚相对节点左上角的偏移是确定的，可据此精确点中引脚/节点体。
 *   - 连线：Visual Script 是「点出引脚 → 点入引脚」两次左键；Shader Graph 是「从出引脚拖到入引脚」。
 *   - 删节点：点中节点体选中 → Delete 键。
 * 断言走产品真实状态（节点/连线计数、生成的 Lua、磁盘 GLSL 文件），而非「能画出来」。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <filesystem>
#include <string>
#include <system_error>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_visual_script.h"  // VisualScript* 测试访问器 + GetVisualScriptLuaOutput
#include "../editor_shader_graph.h"   // ShaderGraph* 测试访问器

namespace dse::editor::uitest {

namespace {
namespace fs = std::filesystem;

// 右键画布某屏幕坐标弹出「创建节点」菜单，并把 ref 指向弹窗。
void OpenCanvasCreateMenu(ImGuiTestContext* ctx, const ImVec2& pos) {
    ctx->MouseMoveToPos(pos);
    ctx->MouseClick(ImGuiMouseButton_Right);
    ctx->Yield(2);
    ctx->SetRef("//$FOCUSED");
}
} // namespace

void RegisterGraphTests(ImGuiTestEngine* e) {
    // ── ⑥-1 Visual Script：菜单加节点 → 连线 → 编译出 Lua → 选中删节点 ────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-graph", "visual_script_node_link_compile");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            VisualScriptResetGraph();
            ShowFloatingPanel(ctx, Services().show_visual_script, "//Visual Script");
            ctx->Yield(2);
            IM_CHECK(VisualScriptNodeCount() == 0);
            IM_CHECK(VisualScriptLinkCount() == 0);

            // 三个节点：事件 On Update（供编译出函数体）、Float 常量、Sin。
            const ImVec2 m_event(380.0f, 430.0f);
            const ImVec2 m_float(380.0f, 230.0f);
            const ImVec2 m_sin  (660.0f, 230.0f);

            OpenCanvasCreateMenu(ctx, m_event);
            ctx->MenuClick("Event/On Update");
            ctx->Yield(2);
            OpenCanvasCreateMenu(ctx, m_float);
            ctx->MenuClick("Variable/Float Constant");
            ctx->Yield(2);
            OpenCanvasCreateMenu(ctx, m_sin);
            ctx->MenuClick("Math/Sin");
            ctx->Yield(2);
            ctx->SetRef("");
            IM_CHECK(VisualScriptNodeCount() == 3);

            // 连线：Float.Value(输出, 节点左上+(160,28)) → Sin.X(输入, 节点左上+(0,28))。
            ctx->MouseMoveToPos(ImVec2(m_float.x + 160.0f, m_float.y + 28.0f));
            ctx->MouseClick(ImGuiMouseButton_Left);
            ctx->Yield(2);
            ctx->MouseMoveToPos(ImVec2(m_sin.x + 0.0f, m_sin.y + 28.0f));
            ctx->MouseClick(ImGuiMouseButton_Left);
            ctx->Yield(2);
            IM_CHECK(VisualScriptLinkCount() == 1);

            // 编译到 Lua：On Update 事件应生成 on_update 函数。
            ctx->SetRef("//Visual Script");
            ctx->ItemClick("Compile to Lua");
            ctx->Yield(2);
            IM_CHECK(!GetVisualScriptLuaOutput().empty());
            IM_CHECK(GetVisualScriptLuaOutput().find("on_update") != std::string::npos);

            // 选中事件节点（点其标题栏，避开引脚）→ Delete 删除。
            ctx->MouseMoveToPos(ImVec2(m_event.x + 80.0f, m_event.y + 12.0f));
            ctx->MouseClick(ImGuiMouseButton_Left);
            ctx->Yield(2);
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(2);
            IM_CHECK(VisualScriptNodeCount() == 2);

            VisualScriptResetGraph();
            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── ⑥-2 Shader Graph：菜单加节点 → 编译写出 GLSL 文件 → 选中删节点 ────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-graph", "shader_graph_add_compile_delete");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ShaderGraphResetGraph();  // 默认图：4 节点 + 1 连线
            ShowFloatingPanel(ctx, Services().show_shader_graph, "//Shader Graph");
            ctx->Yield(2);
            IM_CHECK(ShaderGraphNodeCount() == 4);

            // 右键画布空白 → AddNodeMenu → Add（Math 类模板）。
            const ImVec2 m_add(740.0f, 180.0f);
            OpenCanvasCreateMenu(ctx, m_add);
            ctx->ItemClick("Add");
            ctx->Yield(2);
            ctx->SetRef("");
            IM_CHECK(ShaderGraphNodeCount() == 5);

            // 编译 → 落盘 shader_graph_output.frag。
            const fs::path frag = fs::current_path() / "shader_graph_output.frag";
            std::error_code ec;
            fs::remove(frag, ec);
            ctx->SetRef("//Shader Graph");
            ctx->ItemClick("Compile");
            ctx->Yield(2);
            IM_CHECK(fs::exists(frag));
            IM_CHECK(fs::file_size(frag, ec) > 0);

            // 选中刚加的节点（其左上角=右键处）→ Delete 键删除（需画布悬停）。
            ctx->MouseMoveToPos(ImVec2(m_add.x + 20.0f, m_add.y + 12.0f));
            ctx->MouseClick(ImGuiMouseButton_Left);
            ctx->Yield(2);
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(2);
            IM_CHECK(ShaderGraphNodeCount() == 4);

            fs::remove(frag, ec);
            ShaderGraphResetGraph();
            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── ⑥-3 Shader Graph：拖拽出引脚→入引脚建立连线 ──────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-graph", "shader_graph_connect_pins");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ShaderGraphResetGraph();
            ShowFloatingPanel(ctx, Services().show_shader_graph, "//Shader Graph");
            ctx->Yield(2);
            const int base_links = ShaderGraphLinkCount();

            // Float（仅 1 输出 Value）+ Add（2 输入 A/B、1 输出）。
            const ImVec2 m_float(300.0f, 360.0f);
            const ImVec2 m_add  (620.0f, 360.0f);
            OpenCanvasCreateMenu(ctx, m_float);
            ctx->ItemClick("Float");
            ctx->Yield(2);
            OpenCanvasCreateMenu(ctx, m_add);
            ctx->ItemClick("Add");
            ctx->Yield(2);
            ctx->SetRef("");
            IM_CHECK(ShaderGraphNodeCount() == 6);

            // 拖拽：Float.Value(输出, 左上+(180,39)) → Add.A(输入, 左上+(0,39))。
            ManualMouseDrag(ctx,
                            ImVec2(m_float.x + 180.0f, m_float.y + 39.0f),
                            ImVec2(m_add.x + 0.0f,    m_add.y + 39.0f));
            ctx->Yield(2);
            IM_CHECK(ShaderGraphLinkCount() == base_links + 1);

            ShaderGraphResetGraph();
            HideOptionalPanels();
            ctx->Yield(2);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
