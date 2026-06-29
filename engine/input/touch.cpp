/**
 * @file touch.cpp
 * @brief Touch 触摸抽象层实现
 */

#include "engine/input/touch.h"

namespace dse::input {

std::array<TouchPoint, Touch::kMaxTouchPoints> Touch::touches_{};
int Touch::touch_count_ = 0;

int Touch::FindIndexById(int finger_id) {
    for (int i = 0; i < touch_count_; ++i) {
        if (touches_[i].finger_id == finger_id) return i;
    }
    return -1;
}

void Touch::RecordTouch(int finger_id, float x, float y, TouchPhase phase) {
    if (phase == TouchPhase::None) return;

    const glm::vec2 pos{x, y};
    int idx = FindIndexById(finger_id);

    if (idx < 0) {
        // 仅在按下时新建触点；对未知 ID 的移动/抬起事件，按下若被丢弃则忽略。
        if (phase != TouchPhase::Began) return;
        if (touch_count_ >= kMaxTouchPoints) return;  // 超出上限，丢弃
        idx = touch_count_++;
        TouchPoint& tp = touches_[idx];
        tp.finger_id = finger_id;
        tp.position  = pos;
        tp.delta     = glm::vec2{0.0f, 0.0f};
        tp.phase     = TouchPhase::Began;
        return;
    }

    TouchPoint& tp = touches_[idx];
    tp.delta    = pos - tp.position;
    tp.position = pos;
    tp.phase    = phase;
}

void Touch::Update() {
    int write = 0;
    for (int read = 0; read < touch_count_; ++read) {
        TouchPoint& tp = touches_[read];
        if (tp.phase == TouchPhase::Ended || tp.phase == TouchPhase::Cancelled) {
            continue;  // 丢弃已结束触点
        }
        // 保留的触点：推进相位到 Stationary，归零本帧位移。
        tp.phase = TouchPhase::Stationary;
        tp.delta = glm::vec2{0.0f, 0.0f};
        if (write != read) touches_[write] = tp;
        ++write;
    }
    touch_count_ = write;
}

int Touch::GetTouchCount() {
    return touch_count_;
}

bool Touch::TryGetTouch(int index, TouchPoint& out) {
    if (index < 0 || index >= touch_count_) return false;
    out = touches_[index];
    return true;
}

bool Touch::GetTouchById(int finger_id, TouchPoint& out) {
    const int idx = FindIndexById(finger_id);
    if (idx < 0) return false;
    out = touches_[idx];
    return true;
}

bool Touch::IsAnyTouchDown() {
    for (int i = 0; i < touch_count_; ++i) {
        if (touches_[i].phase == TouchPhase::Began) return true;
    }
    return false;
}

bool Touch::IsAnyTouchActive() {
    return touch_count_ > 0;
}

void Touch::Reset() {
    touches_ = {};
    touch_count_ = 0;
}

} // namespace dse::input
