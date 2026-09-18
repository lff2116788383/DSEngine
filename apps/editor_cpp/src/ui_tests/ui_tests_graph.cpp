/**
 * @file ui_tests_graph.cpp
 * @brief ⑥ Shader Graph 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 节点图编辑器（editor_shader_graph.cpp）用 ImDrawList 自绘节点+引脚，
 * 交互靠画布内的鼠标命中而非 ImGui item，所以用例走「真实控件 + 屏幕坐标」驱动：
 *   - 加节点：右键画布空白弹「创建节点」菜单（真实 ImGui::MenuItem）→ 点选模板。
 *     新节点放在 create_menu_pos（= 右键处 - 画布偏移），故新节点左上角恰好
 *     落在右键屏幕坐标处；引脚相对节点左上角的偏移是确定的，可据此精确点中引脚/节点体。
 *   - 连线：从出引脚拖到入引脚。
 *   - 删节点：点中节点体选中 → Delete 键。
 * 断言走产品真实状态（节点/连线计数、磁盘 GLSL 文件），而非「能画出来」。
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
    // ── ⑥-1 Shader Graph：菜单加节点 → 编译写出 GLSL 文件 → 选中删节点 ────────────────
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
            IM_CHECK(ShaderGraphNodeCount() == 4);   // 默认图：PBR Output / Texture Sample / Color / Float

            // 连线目标用**默认图里既有的两个节点**，不再经由"右键菜单建节点"：
            // 建节点会让画布逐帧缓动取景，可视区与坐标都在变，是这条用例长期不稳定的主因。
            // 选 node2 «Color».Color（Color 输出） → node0 «PBR Output».Emission（Color 输入）：
            // 类型一致、不像 Base Color 那样已被默认连线占用，且两节点都在画布可视区内。
            const int src_node = 2, src_pin = 0;      // Color.Color（输出）
            const int dst_node = 0, dst_pin = 4;      // PBR Output.Emission（输入）

            // 画布取景是缓动的：轮询到引脚屏幕坐标**连续 6 帧不变**再拖，避免用过期坐标。
            float fx = 0.0f, fy = 0.0f, ax = 0.0f, ay = 0.0f;
            bool have_pos = false;
            int stable_frames = 0;
            for (int i = 0; i < 80; ++i) {
                float nfx = 0.0f, nfy = 0.0f, nax = 0.0f, nay = 0.0f;
                const bool got = ShaderGraphPinScreenPos(src_node, /*is_output=*/true, src_pin, &nfx, &nfy) &&
                                 ShaderGraphPinScreenPos(dst_node, /*is_output=*/false, dst_pin, &nax, &nay);
                if (got) {
                    const bool same = std::abs(nfx - fx) < 0.5f && std::abs(nfy - fy) < 0.5f &&
                                      std::abs(nax - ax) < 0.5f && std::abs(nay - ay) < 0.5f;
                    stable_frames = same ? stable_frames + 1 : 0;
                    fx = nfx; fy = nfy; ax = nax; ay = nay;
                    have_pos = true;
                    if (stable_frames >= 6) break;
                }
                ctx->Yield();
            }
            IM_CHECK(have_pos);
            {
                ImGuiWindow* gw = FindActiveWindow("Shader Graph");
                bool canvas_ok = false;
                if (gw) {
                    ctx->SetRef((std::string("//") + gw->Name).c_str());
                    const ImGuiTestItemInfo ci = ctx->ItemInfo("canvas", ImGuiTestOpFlags_NoError);
                    if (ci.ID != 0) {
                        const ImRect cr(ci.RectClipped.Min, ci.RectClipped.Max);
                        canvas_ok = cr.Contains(ImVec2(fx, fy)) && cr.Contains(ImVec2(ax, ay));
                    }
                    ctx->SetRef("");
                }
                UiDiagLog("[graph] src=%d/%d=(%.0f,%.0f) dst=%d/%d=(%.0f,%.0f) canvas_ok=%d links=%d",
                          src_node, src_pin, fx, fy, dst_node, dst_pin, ax, ay,
                          canvas_ok ? 1 : 0, ShaderGraphLinkCount());
                // 两个引脚都必须在画布可视区内，否则按下/松开判定会被"画布不 hover"吞掉
                IM_CHECK(canvas_ok);
            }
            ManualMouseDrag(ctx, ImVec2(fx, fy), ImVec2(ax, ay));
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
