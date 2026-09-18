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

            // Float（仅 1 输出 Value）+ Add（2 输入 A/B、1 输出）。
            // 位置取画布**左下角**：Shader Graph 面板还会额外开 Node Properties / GLSL Preview /
            // Shader Preview 三个**独立窗口**，它们默认压在画布右上区域，会让画布不满足
            // IsWindowHovered()（实测 rect_hit=1 而 win_hovered=0），引脚按下/松开判定直接失效。
            auto canvas_rect_now = [&](void) -> bool {
                ImGuiWindow* gw = FindActiveWindow("Shader Graph");
                if (!gw) return false;
                ctx->SetRef((std::string("//") + gw->Name).c_str());
                const ImGuiTestItemInfo ci = ctx->ItemInfo("canvas", ImGuiTestOpFlags_NoError);
                return ci.ID != 0 && ci.RectClipped.GetWidth() > 40.0f;
            };
            ImVec2 p_float(300.0f, 500.0f);
            if (canvas_rect_now()) {
                const ImGuiTestItemInfo ci0 = ctx->ItemInfo("canvas", ImGuiTestOpFlags_NoError);
                p_float = ImVec2(ci0.RectClipped.Min.x + 60.0f, ci0.RectClipped.Max.y - 120.0f);
            }
            OpenCanvasCreateMenu(ctx, p_float);
            ctx->ItemClick("Float");
            ctx->Yield(2);
            ctx->KeyPress(ImGuiKey_Escape);
            ctx->Yield(2);
            // 第二次右键落在画布**可见矩形中心**：实测这样能稳定弹出创建菜单（改用左下角等其他
            // 位置时，建完第一个节点后的画布缓动取景会让右键落空，报
            // "Unable to locate item: //$FOCUSED/**/Add"）。
            if (canvas_rect_now()) {
                const ImGuiTestItemInfo ci1 = ctx->ItemInfo("canvas", ImGuiTestOpFlags_NoError);
                ctx->MouseMoveToPos(ci1.RectClipped.GetCenter());
                ctx->MouseClick(ImGuiMouseButton_Right);
                ctx->Yield(2);
            }
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("**/Add");
            ctx->Yield(2);
            ctx->SetRef("");
            IM_CHECK(ShaderGraphNodeCount() == 6);

            // 拖拽：Float.Value(输出引脚 0) → Add.A(输入引脚 0)。
            // 必须取**引脚的实际屏幕坐标**（ShaderGraphPinScreenPos）：引脚不是节点的固定偏移
            // （输出排在所有输入之下），且画布平移/缩放会改变位置。
            // 画布在建节点后会**自动平移取景**（实测画布矩形由 (188,143)-(1112,642) 变为
            // (-519,143)-(405,642)），若立刻读引脚坐标会拿到平移前的值、拖拽必然落空 ——
            // 先等若干帧让取景稳定下来再取坐标。
            // 画布在建节点后会**逐帧缓动平移取景**，固定等几帧仍可能没停 → 改为轮询
            // 「引脚屏幕坐标连续两帧不变」再拖，避免用过期坐标导致连线落空。
            const int n = ShaderGraphNodeCount();
            float fx = 0.0f, fy = 0.0f, ax = 0.0f, ay = 0.0f;
            bool have_pos = false;
            int stable_frames = 0;
            for (int i = 0; i < 80; ++i) {
                float nfx = 0.0f, nfy = 0.0f, nax = 0.0f, nay = 0.0f;
                const bool got = ShaderGraphPinScreenPos(n - 2, /*is_output=*/true, 0, &nfx, &nfy) &&
                                 ShaderGraphPinScreenPos(n - 1, /*is_output=*/false, 0, &nax, &nay);
                if (got) {
                    const bool same = std::abs(nfx - fx) < 0.5f && std::abs(nfy - fy) < 0.5f &&
                                      std::abs(nax - ax) < 0.5f && std::abs(nay - ay) < 0.5f;
                    stable_frames = same ? stable_frames + 1 : 0;
                    fx = nfx; fy = nfy; ax = nax; ay = nay;
                    have_pos = true;
                    // 画布取景是缓动的，要求连续 6 帧不变才算真的停下（2 帧会在缓动的短暂停顿处误判）
                    if (stable_frames >= 6) break;
                }
                ctx->Yield();
            }
            IM_CHECK(have_pos);
            UiDiagLog("[graph] float_out=(%.0f,%.0f) add_in=(%.0f,%.0f) nodes=%d links=%d",
                      fx, fy, ax, ay, ShaderGraphNodeCount(), ShaderGraphLinkCount());
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
