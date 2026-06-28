/**
 * @file ui_tests_animation.cpp
 * @brief 动画体系功能用例（仅 DSE_EDITOR_UI_TESTS 编入）。补缺口③。
 *
 *   - timeline_add_track     ：点 Timeline「+ Track」→ 轨道数 +1，且新轨道含 1 个关键帧（经测试访问器）。
 *   - timeline_play_stop     ：点播放 → playing 真；点停止 → playing 假且 playhead 归零。
 *   - state_machine_add_state：给选中实体挂 Animator3D + 状态机，画布右键「Add State」→ 真实状态机 +1 个状态。
 *   - curve_eval_logic       ：曲线核心（MakeDefaultCurve/SortKeys/Evaluate）数值断言。
 *   - retarget_auto_map / manual_override / bake：骨骼重定向纯核心（按名匹配/手动覆盖/烘焙改写通道）。
 *
 * 说明：curve_editor 的加点是画布双击、retarget 面板靠 glTF 导入驱动，二者在无头下不可确定；
 * 故对这两块测其公开的纯逻辑函数（与 *_core 拆分的初衷一致），timeline/state_machine 走真实控件。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_animation_timeline.h"   // Timeline 测试访问器
#include "../editor_curve_editor.h"         // EditorCurve / MakeDefaultCurve
#include "../editor_anim_retarget_core.h"   // 重定向纯核心
#include "../editor_selection.h"            // SelectionManager
#include "../editor_icons.h"                 // MDI_ICON_PLAY/STOP

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_animation.h"   // Animator3DComponent
#include "engine/ecs/animation_state_machine.h"   // AnimationStateMachine
#include "engine/assets/compiler/raw_scene_data.h"

namespace dse::editor::uitest {

namespace {
entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 右键 Hierarchy → Create Empty Entity（创建即选中 ctx.selected_entity），按 registry 差集取回新实体。
entt::entity CreateAndSelectEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
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

void RegisterAnimationTests(ImGuiTestEngine* e) {
    // ── ③-1 Timeline：+ Track ────────────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "timeline_add_track");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            DeselectAll(ctx);
            ShowFloatingPanel(ctx, Services().show_animation_timeline, "//Animation Timeline");
            IM_CHECK(FindActiveWindow("Animation Timeline") != nullptr);

            const int before = TimelineTrackCount();
            IM_CHECK(before >= 1);  // InitDefaultClip 播种了若干轨道
            IM_CHECK(ctx->ItemInfo("//Animation Timeline/**/+ Track").ID != 0);
            ctx->ItemClick("//Animation Timeline/**/+ Track");
            ctx->Yield(2);
            IM_CHECK_EQ(TimelineTrackCount(), before + 1);
            // 新轨道（下标 = 原数量）由「+ Track」附带创建了 1 个关键帧。
            IM_CHECK_EQ(TimelineKeyframeCount(before), 1);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── ③-2 Timeline：播放 / 停止 ────────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "timeline_play_stop");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            DeselectAll(ctx);
            ShowFloatingPanel(ctx, Services().show_animation_timeline, "//Animation Timeline");
            IM_CHECK(FindActiveWindow("Animation Timeline") != nullptr);

            const std::string play_ref = std::string("//Animation Timeline/") + MDI_ICON_PLAY + "##play";
            const std::string stop_ref = std::string("//Animation Timeline/") + MDI_ICON_STOP + "##stop";
            IM_CHECK(ctx->ItemInfo(play_ref.c_str()).ID != 0);

            IM_CHECK(!TimelinePlaying());
            ctx->ItemClick(play_ref.c_str());
            ctx->Yield(2);
            IM_CHECK(TimelinePlaying());

            // 播放后按钮变为暂停态；停止应停播并把播放头归零。
            ctx->ItemClick(stop_ref.c_str());
            ctx->Yield(2);
            IM_CHECK(!TimelinePlaying());
            IM_CHECK(std::abs(TimelinePlayhead()) < 0.001f);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── ③-3 State Machine：右键画布 Add State ────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "state_machine_add_state");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            // 先创建并选中一个实体，再给它挂上带状态机的 Animator3D（面板要求选中实体具备该组件）。
            const entt::entity ent = CreateAndSelectEntity(ctx);
            IM_CHECK(ent != entt::null);
            IM_CHECK(Reg().valid(ent));

            auto& animator = Reg().emplace<dse::Animator3DComponent>(ent);
            animator.state_machine = std::make_shared<dse::gameplay3d::AnimationStateMachine>();
            auto* sm = animator.state_machine.get();

            ShowFloatingPanel(ctx, Services().show_anim_state_machine, "//Anim State Machine");
            ctx->Yield(2);
            IM_CHECK(FindActiveWindow("Anim State Machine") != nullptr);

            const size_t before = sm->GetStates().size();

            // 右键画布空白处弹出 "##CanvasCtx" → 点 "Add State"。
            ImGuiWindow* canvas = FindActiveWindow("##ASMCanvas");
            IM_CHECK(canvas != nullptr);
            const ImVec2 center(canvas->Pos.x + canvas->Size.x * 0.5f,
                                canvas->Pos.y + canvas->Size.y * 0.5f);
            ctx->MouseMoveToPos(center);
            ctx->MouseClick(ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Add State");
            ctx->Yield(2);

            IM_CHECK_EQ(sm->GetStates().size(), before + 1);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── ③-4 Curve：曲线核心数值逻辑 ──────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "curve_eval_logic");
        t->TestFunc = [](ImGuiTestContext*) {
            // 默认线性 0→1 斜坡：端点与中点取值应符合线性插值。
            EditorCurve c = MakeDefaultCurve("test", 0.0f, 1.0f);
            c.interp = CurveInterp::Linear;
            IM_CHECK(c.keys.size() >= 2);
            IM_CHECK(std::abs(c.Evaluate(c.keys.front().time) - 0.0f) < 0.01f);
            IM_CHECK(std::abs(c.Evaluate(c.keys.back().time) - 1.0f) < 0.01f);
            const float t_mid = (c.keys.front().time + c.keys.back().time) * 0.5f;
            IM_CHECK(std::abs(c.Evaluate(t_mid) - 0.5f) < 0.05f);

            // 乱序插入的关键帧经 SortKeys 后按时间升序；插值结果随之单调。
            EditorCurve d;
            d.interp = CurveInterp::Linear;
            d.keys.push_back({1.0f, 10.0f, 0.0f, 0.0f});
            d.keys.push_back({0.0f, 0.0f, 0.0f, 0.0f});
            d.SortKeys();
            IM_CHECK(d.keys.front().time <= d.keys.back().time);
            IM_CHECK(std::abs(d.Evaluate(0.5f) - 5.0f) < 0.05f);
        };
    }

    // ── ③-5 Retarget：自动按名映射 ──────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "retarget_auto_map");
        t->TestFunc = [](ImGuiTestContext*) {
            using namespace dse::editor::retarget;
            std::vector<std::string> src = {"mixamorig:Hips", "mixamorig:Spine", "ExtraSourceBone"};
            std::vector<std::string> dst = {"Hips", "Spine"};
            BoneMap map = AutoMapBones(src, dst);
            IM_CHECK_EQ(static_cast<int>(map.matches.size()), 3);
            // Hips/Spine 应被（精确或归一化）匹配上，ExtraSourceBone 无对应 → 未映射。
            IM_CHECK(map.matches[0].target_index >= 0);
            IM_CHECK(map.matches[1].target_index >= 0);
            IM_CHECK_EQ(map.matches[2].target_index, -1);
            IM_CHECK_EQ(MappedCount(map), 2);
        };
    }

    // ── ③-6 Retarget：手动覆盖映射 ──────────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "retarget_manual_override");
        t->TestFunc = [](ImGuiTestContext*) {
            using namespace dse::editor::retarget;
            std::vector<std::string> src = {"A", "B"};
            std::vector<std::string> dst = {"X", "Y"};
            BoneMap map = AutoMapBones(src, dst);  // 名字不同 → 自动映射全失败
            IM_CHECK_EQ(MappedCount(map), 0);

            SetManualMapping(map, 0, 1);  // 手动把源 0 映射到目标 1
            IM_CHECK_EQ(map.matches[0].target_index, 1);
            IM_CHECK(map.matches[0].type == MatchType::Manual);
            IM_CHECK_EQ(MappedCount(map), 1);

            SetManualMapping(map, 0, -1);  // 清除映射
            IM_CHECK_EQ(map.matches[0].target_index, -1);
            IM_CHECK_EQ(MappedCount(map), 0);
        };
    }

    // ── ③-7 Retarget：烘焙重定向动画（改写已映射通道、丢弃未映射通道）────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-anim", "retarget_bake");
        t->TestFunc = [](ImGuiTestContext*) {
            using namespace dse::editor::retarget;
            namespace ac = dse::asset::compiler;
            std::vector<std::string> src = {"Hips", "UnmappedBone"};
            std::vector<std::string> dst = {"Hips"};
            BoneMap map = AutoMapBones(src, dst);

            ac::RawAnimation anim;
            anim.name = "clip";
            anim.duration = 1.0f;
            {
                ac::RawAnimationChannel ch0;  // 引用 Hips（会被保留+改写）
                ch0.target_node_name = "Hips";
                ch0.time_keys = {0.0f, 1.0f};
                anim.channels.push_back(ch0);
                ac::RawAnimationChannel ch1;  // 引用 UnmappedBone（应被丢弃）
                ch1.target_node_name = "UnmappedBone";
                ch1.time_keys = {0.0f};
                anim.channels.push_back(ch1);
            }

            ac::RawAnimation out = RetargetAnimation(anim, src, dst, map);
            // 仅保留映射成功的 Hips 通道，且其目标名/索引指向目标骨架。
            IM_CHECK_EQ(static_cast<int>(out.channels.size()), 1);
            IM_CHECK_STR_EQ(out.channels[0].target_node_name.c_str(), "Hips");
            IM_CHECK_EQ(out.channels[0].target_node_index, 0);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
