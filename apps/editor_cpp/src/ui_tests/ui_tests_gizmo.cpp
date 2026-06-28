/**
 * @file ui_tests_gizmo.cpp
 * @brief ImGuizmo 视口变换控制用例（仅 DSE_EDITOR_UI_TESTS 编入）。补缺口②。
 *
 * 真实的 gizmo 拖拽（鼠标命中某根轴手柄）依赖相机投影把实体投到精确像素、且 ImGuizmo 每帧
 * 重算手柄位置，无头下不可确定、极易漂移；因此这里覆盖 gizmo 的「操作模式 / 坐标系 / 吸附步长」
 * 这些可确定的控制路径：经 View 菜单与视口键盘快捷键切换，断言编辑器实际状态（经 UiTestServices
 * 暴露的 current_gizmo_operation/mode 与 GetSnap* 持久值）真的改变。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include <string>

#include "../editor_preferences_panel.h"  // GetSnapTranslate
#include "../editor_selection.h"
#include "../editor_icons.h"              // MDI_ICON_ARROW_ALL / ROTATE_3D_VARIANT / RESIZE

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"

namespace dse::editor::uitest {

void RegisterGizmoTests(ImGuiTestEngine* e) {
    // dse-gizmo/operation_via_menu：View 菜单切换 Translate/Rotate/Scale → 断言 operation 真改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "operation_via_menu");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* op = Services().current_gizmo_operation;
            IM_CHECK(op != nullptr);
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("View/Gizmo: Rotate");
            ctx->Yield();
            IM_CHECK_EQ(*op, 1);
            ctx->MenuClick("View/Gizmo: Scale");
            ctx->Yield();
            IM_CHECK_EQ(*op, 2);
            ctx->MenuClick("View/Gizmo: Translate");
            ctx->Yield();
            IM_CHECK_EQ(*op, 0);
        };
    }

    // dse-gizmo/coordinate_space_via_menu：View 菜单切换 World/Local Space → 断言 mode 真改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "coordinate_space_via_menu");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* mode = Services().current_gizmo_mode;
            IM_CHECK(mode != nullptr);
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("View/World Space");
            ctx->Yield();
            IM_CHECK_EQ(*mode, 1);
            ctx->MenuClick("View/Local Space");
            ctx->Yield();
            IM_CHECK_EQ(*mode, 0);
        };
    }

    // dse-gizmo/operation_via_viewport_toolbar：点 Scene 视口左上的 gizmo 工具栏按钮
    // （##vp_rot/##vp_scl/##vp_trans，见 editor_viewport_panel.cpp）→ 断言 operation 真改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "operation_via_viewport_toolbar");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* op = Services().current_gizmo_operation;
            IM_CHECK(op != nullptr);

            // 工具栏只在 Scene 视口被绘制（其标签页激活）时出现，先激活 Scene 标签页。
            ctx->WindowFocus("//Scene");
            ctx->Yield(2);
            IM_CHECK(FindActiveWindow("Scene") != nullptr);

            const std::string rot_ref   = std::string("//Scene/") + MDI_ICON_ROTATE_3D_VARIANT + "##vp_rot";
            const std::string scl_ref   = std::string("//Scene/") + MDI_ICON_RESIZE + "##vp_scl";
            const std::string trans_ref = std::string("//Scene/") + MDI_ICON_ARROW_ALL + "##vp_trans";

            ctx->ItemClick(rot_ref.c_str());
            ctx->Yield();
            IM_CHECK_EQ(*op, 1);
            ctx->ItemClick(scl_ref.c_str());
            ctx->Yield();
            IM_CHECK_EQ(*op, 2);
            ctx->ItemClick(trans_ref.c_str());
            ctx->Yield();
            IM_CHECK_EQ(*op, 0);
        };
    }

    // dse-gizmo/snap_setting_edit：View 菜单里改 Translate 吸附步长 DragFloat → 断言 GetSnapTranslate 持久值改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "snap_setting_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("View");          // 打开 View 菜单（保持展开以操作其中的 DragFloat）
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemInputValue("Translate", 2.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(dse::editor::GetSnapTranslate() - 2.5f) < 0.01f);
            ctx->KeyPress(ImGuiKey_Escape);  // 关闭菜单
            ctx->Yield();
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
