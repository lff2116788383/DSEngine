/**
 * @file input_recorder.cpp
 * @brief 输入录制与回放系统实现
 */

#include "engine/input/input_recorder.h"
#include "engine/input/input.h"
#include <sstream>
#include <iomanip>

namespace dse {
namespace input {

// ============================================================
// InputRecorder
// ============================================================

void InputRecorder::StartRecording() {
    recording_ = true;
}

void InputRecorder::StopRecording() {
    recording_ = false;
}

void InputRecorder::RecordEvent(unsigned short key_code, unsigned short key_action, double timestamp) {
    if (!recording_) return;
    InputEvent evt;
    evt.timestamp = timestamp;
    evt.key_code = key_code;
    evt.key_action = key_action;
    events_.push_back(evt);
}

void InputRecorder::Clear() {
    events_.clear();
    recording_ = false;
}

std::string InputRecorder::ExportJSON() const {
    std::ostringstream oss;
    oss << "[\n";
    bool first = true;
    for (const auto& evt : events_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "{\"ts\":" << std::fixed << std::setprecision(3) << evt.timestamp
            << ",\"key\":" << evt.key_code
            << ",\"action\":" << evt.key_action
            << "}";
    }
    oss << "\n]";
    return oss.str();
}

bool InputRecorder::ImportJSON(const std::string& json) {
    events_.clear();
    size_t pos = 0;
    while ((pos = json.find("{\"ts\":", pos)) != std::string::npos) {
        InputEvent evt;
        size_t ts_start = pos + 6; // after {"ts":
        size_t ts_end = json.find(',', ts_start);
        if (ts_end == std::string::npos) return false;
        evt.timestamp = std::stod(json.substr(ts_start, ts_end - ts_start));

        size_t key_start = json.find("\"key\":", ts_end);
        if (key_start == std::string::npos) return false;
        key_start += 6;
        size_t key_end = json.find(',', key_start);
        if (key_end == std::string::npos) return false;
        evt.key_code = static_cast<unsigned short>(std::stoi(json.substr(key_start, key_end - key_start)));

        size_t action_start = json.find("\"action\":", key_end);
        if (action_start == std::string::npos) return false;
        action_start += 9;
        size_t action_end = json.find('}', action_start);
        if (action_end == std::string::npos) return false;
        evt.key_action = static_cast<unsigned short>(std::stoi(json.substr(action_start, action_end - action_start)));

        events_.push_back(evt);
        pos = action_end + 1;
    }
    return true;
}

// ============================================================
// InputPlayer
// ============================================================

void InputPlayer::Load(const std::vector<InputEvent>& events) {
    events_ = events;
    current_index_ = 0;
    playing_ = false;
}

void InputPlayer::Load(const InputRecorder& recorder) {
    Load(recorder.GetEvents());
}

void InputPlayer::Start(double current_time) {
    current_index_ = 0;
    start_time_ = current_time;
    playing_ = true;
}

void InputPlayer::Stop() {
    playing_ = false;
}

void InputPlayer::Update(double current_time) {
    if (!playing_ || events_.empty()) return;
    double elapsed = current_time - start_time_;
    while (current_index_ < events_.size()) {
        const auto& evt = events_[current_index_];
        if (evt.timestamp > elapsed) break;
        Input::RecordKey(evt.key_code, evt.key_action);
        ++current_index_;
    }
    if (current_index_ >= events_.size()) {
        playing_ = false;
    }
}

bool InputPlayer::IsFinished() const {
    return !playing_ && current_index_ >= events_.size() && !events_.empty();
}

} // namespace input
} // namespace dse
