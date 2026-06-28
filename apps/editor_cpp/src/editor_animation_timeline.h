#pragma once

namespace dse::editor {

struct EditorContext;

/// Draw the Animation Timeline panel with keyframe editing, play/stop controls,
/// and curve preview.
void DrawAnimationTimelinePanel(EditorContext& ctx);

#ifdef DSE_EDITOR_UI_TESTS
// 仅供无头 UI 测试观测/驱动 Timeline 的内部静态状态（state 是 .cpp 文件作用域静态）。
int   TimelineTrackCount();
int   TimelineKeyframeCount(int track);
bool  TimelinePlaying();
float TimelinePlayhead();
int   TimelineSelectedTrack();
// 选中某轨道某关键帧（等价于在画布上点中那颗菱形），使底部「+ Key / Delete」编辑控件出现。
void  TimelineSelectForTest(int track, int keyframe);
#endif

} // namespace dse::editor
