/**
 * @file input_recorder.h
 * @brief 输入录制与回放系统，支持 JSON 导入导出
 */

#ifndef DSE_INPUT_RECORDER_H
#define DSE_INPUT_RECORDER_H

#include "engine/core/dse_export.h"
#include <string>
#include <vector>

namespace dse {
namespace input {

struct InputEvent {
    double timestamp = 0.0;
    unsigned short key_code = 0;
    unsigned short key_action = 0;
};

class DSE_EXPORT InputRecorder {
public:
    InputRecorder() = default;
    ~InputRecorder() = default;

    void StartRecording();
    void StopRecording();
    bool IsRecording() const { return recording_; }

    void RecordEvent(unsigned short key_code, unsigned short key_action, double timestamp);

    const std::vector<InputEvent>& GetEvents() const { return events_; }
    size_t GetEventCount() const { return events_.size(); }
    void Clear();

    std::string ExportJSON() const;
    bool ImportJSON(const std::string& json);

private:
    std::vector<InputEvent> events_;
    bool recording_ = false;
};

class DSE_EXPORT InputPlayer {
public:
    InputPlayer() = default;
    ~InputPlayer() = default;

    void Load(const std::vector<InputEvent>& events);
    void Load(const InputRecorder& recorder);
    void Start(double current_time);
    void Stop();
    void Update(double current_time);

    bool IsPlaying() const { return playing_; }
    bool IsFinished() const;
    size_t GetCurrentIndex() const { return current_index_; }
    size_t GetTotalEvents() const { return events_.size(); }

private:
    std::vector<InputEvent> events_;
    size_t current_index_ = 0;
    double start_time_ = 0.0;
    bool playing_ = false;
};

} // namespace input
} // namespace dse

#endif // DSE_INPUT_RECORDER_H
