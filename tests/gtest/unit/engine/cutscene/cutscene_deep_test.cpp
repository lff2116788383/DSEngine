/**
 * @file cutscene_deep_test.cpp
 * @brief P5: 过场系统深度测试 — 轨道边界场景、播放器状态机、触发器链
 */

#include <gtest/gtest.h>
#include "engine/cutscene/cutscene_player.h"
#include "engine/cutscene/cutscene_track.h"
#include <string>
#include <vector>

using namespace dse::cutscene;

// ═══════════════════════════════════════════════════════════
// VideoTrack 深度测试
// ═══════════════════════════════════════════════════════════

class VideoTrackDeepTest : public ::testing::Test {
protected:
    std::shared_ptr<VideoTrack> track;
    std::string last_path;
    float last_opacity = 0.0f;
    bool stopped = false;

    void SetUp() override {
        track = std::make_shared<VideoTrack>("Video");
        track->SetPlayCallback([this](const std::string& path, bool, float opacity) {
            last_path = path;
            last_opacity = opacity;
        });
        track->SetStopCallback([this]() { stopped = true; });
    }
};

TEST_F(VideoTrackDeepTest, PlaysAtCorrectTime) {
    VideoCue cue;
    cue.time = 1.0f;
    cue.video_path = "intro.mp4";
    cue.opacity = 0.9f;
    track->AddCue(cue);

    track->Evaluate(0.5f);
    EXPECT_TRUE(last_path.empty());

    track->Evaluate(1.5f);
    EXPECT_EQ(last_path, "intro.mp4");
    EXPECT_FLOAT_EQ(last_opacity, 0.9f);
}

TEST_F(VideoTrackDeepTest, ResetClearsState) {
    VideoCue cue;
    cue.time = 0.5f;
    cue.video_path = "test.mp4";
    track->AddCue(cue);

    track->Evaluate(1.0f);
    EXPECT_FALSE(last_path.empty());

    track->Reset();
    last_path.clear();
    track->Evaluate(1.0f);
    EXPECT_EQ(last_path, "test.mp4");
}

// ═══════════════════════════════════════════════════════════
// EventTrack 边界深度
// ═══════════════════════════════════════════════════════════

class EventTrackDeepTest : public ::testing::Test {
protected:
    std::shared_ptr<EventTrack> track;
    std::vector<std::pair<std::string, std::string>> fired;

    void SetUp() override {
        track = std::make_shared<EventTrack>("Events");
        track->SetFireCallback([this](const std::string& name, const std::string& payload) {
            fired.emplace_back(name, payload);
        });
    }
};

TEST_F(EventTrackDeepTest, PayloadDelivered) {
    track->AddEvent(0.5f, "show_text", "Hello World");
    track->Evaluate(1.0f);
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0].first, "show_text");
    EXPECT_EQ(fired[0].second, "Hello World");
}

TEST_F(EventTrackDeepTest, MultipleEventsInSameFrame) {
    track->AddEvent(0.5f, "A");
    track->AddEvent(0.6f, "B");
    track->AddEvent(0.7f, "C");
    track->Evaluate(1.0f);
    ASSERT_EQ(fired.size(), 3u);
    EXPECT_EQ(fired[0].first, "A");
    EXPECT_EQ(fired[1].first, "B");
    EXPECT_EQ(fired[2].first, "C");
}

TEST_F(EventTrackDeepTest, BackwardSeekDoesNotRefire) {
    track->AddEvent(0.5f, "once");
    track->Evaluate(1.0f);
    EXPECT_EQ(fired.size(), 1u);

    track->Evaluate(0.3f);
    EXPECT_EQ(fired.size(), 1u);
}

TEST_F(EventTrackDeepTest, EmptyTrackEvaluateSafe) {
    track->Evaluate(0.0f);
    track->Evaluate(100.0f);
    EXPECT_EQ(fired.size(), 0u);
}

// ═══════════════════════════════════════════════════════════
// CameraTrack 边界深度
// ═══════════════════════════════════════════════════════════

TEST(CameraTrackDeepTest, EmptyTrackNoCallback) {
    CameraTrack track("Empty");
    bool called = false;
    track.SetApplyCallback([&](const glm::vec3&, const glm::vec3&, float) {
        called = true;
    });
    track.Evaluate(0.0f);
    EXPECT_FALSE(called);
}

TEST(CameraTrackDeepTest, ThreeKeyframeInterpolation) {
    CameraTrack track("ThreeKF");
    glm::vec3 result_pos{0.0f};
    track.SetApplyCallback([&](const glm::vec3& pos, const glm::vec3&, float) {
        result_pos = pos;
    });

    CameraKeyframe kf0{0.0f, glm::vec3(0, 0, 0), glm::vec3(0), 60.0f, InterpMode::Linear};
    CameraKeyframe kf1{1.0f, glm::vec3(10, 0, 0), glm::vec3(0), 60.0f, InterpMode::Linear};
    CameraKeyframe kf2{2.0f, glm::vec3(10, 10, 0), glm::vec3(0), 60.0f, InterpMode::Linear};
    track.AddKeyframe(kf0);
    track.AddKeyframe(kf1);
    track.AddKeyframe(kf2);

    track.Evaluate(0.5f);
    EXPECT_NEAR(result_pos.x, 5.0f, 0.1f);

    track.Evaluate(1.5f);
    EXPECT_NEAR(result_pos.x, 10.0f, 0.1f);
    EXPECT_NEAR(result_pos.y, 5.0f, 0.1f);
}

// ═══════════════════════════════════════════════════════════
// PropertyTrack 边界深度
// ═══════════════════════════════════════════════════════════

TEST(PropertyTrackDeepTest, ManyKeyframes) {
    PropertyTrack track("Many");
    float result = 0.0f;
    track.SetApplyCallback([&](float v) { result = v; });

    for (int i = 0; i <= 10; ++i) {
        track.AddKeyframe(static_cast<float>(i), static_cast<float>(i * 10));
    }

    track.Evaluate(5.0f);
    EXPECT_NEAR(result, 50.0f, 0.1f);

    track.Evaluate(10.0f);
    EXPECT_NEAR(result, 100.0f, 0.1f);
}

TEST(PropertyTrackDeepTest, NegativeValues) {
    PropertyTrack track("Negative");
    float result = 0.0f;
    track.SetApplyCallback([&](float v) { result = v; });

    track.AddKeyframe(0.0f, -100.0f);
    track.AddKeyframe(1.0f, 100.0f);

    track.Evaluate(0.5f);
    EXPECT_NEAR(result, 0.0f, 0.5f);
}

// ═══════════════════════════════════════════════════════════
// CutscenePlayer 状态机深度
// ═══════════════════════════════════════════════════════════

class CutscenePlayerDeepTest : public ::testing::Test {
protected:
    CutscenePlayer player;
    float prop_val = 0.0f;
    std::vector<std::string> finished;

    void SetUp() override {
        auto seq = std::make_shared<CutsceneSequence>("A", 3.0f);
        auto pt = std::make_shared<PropertyTrack>("fade");
        pt->AddKeyframe(0.0f, 0.0f);
        pt->AddKeyframe(3.0f, 1.0f);
        pt->SetApplyCallback([this](float v) { prop_val = v; });
        seq->AddTrack(pt);
        player.AddSequence(seq);

        auto seq2 = std::make_shared<CutsceneSequence>("B", 2.0f);
        player.AddSequence(seq2);

        player.SetFinishCallback([this](const std::string& name) {
            finished.push_back(name);
        });
    }
};

TEST_F(CutscenePlayerDeepTest, SwitchSequenceMidPlay) {
    player.Play("A");
    player.Update(1.0f);
    EXPECT_EQ(player.GetState(), PlayState::Playing);
    EXPECT_EQ(player.GetCurrentSequenceName(), "A");

    player.Play("B");
    EXPECT_EQ(player.GetCurrentSequenceName(), "B");
    EXPECT_NEAR(player.GetCurrentTime(), 0.0f, 0.01f);
}

TEST_F(CutscenePlayerDeepTest, PlayRateZero) {
    player.Play("A");
    player.SetPlayRate(0.0f);
    player.Update(10.0f);
    EXPECT_NEAR(player.GetCurrentTime(), 0.0f, 0.01f);
}

TEST_F(CutscenePlayerDeepTest, NegativePlayRate) {
    player.Play("A");
    player.SetPlayRate(-1.0f);
    player.Update(1.0f);
    float t = player.GetCurrentTime();
    EXPECT_LE(t, 0.0f);
}

TEST_F(CutscenePlayerDeepTest, SeekBeyondDuration) {
    player.Play("A");
    player.Seek(100.0f);
    EXPECT_GE(player.GetCurrentTime(), 3.0f);
}

TEST_F(CutscenePlayerDeepTest, PauseWhileStopped) {
    player.Pause();
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
}

TEST_F(CutscenePlayerDeepTest, ResumeWhileStopped) {
    player.Resume();
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
}

TEST_F(CutscenePlayerDeepTest, MultipleTriggers) {
    bool t1 = false, t2 = false;
    CutsceneTrigger trigger1;
    trigger1.sequence_name = "A";
    trigger1.condition = [&]() { return t1; };
    trigger1.auto_remove = true;
    player.AddTrigger(trigger1);

    CutsceneTrigger trigger2;
    trigger2.sequence_name = "B";
    trigger2.condition = [&]() { return t2; };
    trigger2.auto_remove = true;
    player.AddTrigger(trigger2);

    player.Update(0.016f);
    EXPECT_EQ(player.GetState(), PlayState::Stopped);

    t1 = true;
    player.Update(0.016f);
    EXPECT_EQ(player.GetCurrentSequenceName(), "A");
}

TEST_F(CutscenePlayerDeepTest, ClearTriggers) {
    bool t = false;
    CutsceneTrigger trigger;
    trigger.sequence_name = "A";
    trigger.condition = [&]() { return t; };
    player.AddTrigger(trigger);

    player.ClearTriggers();
    t = true;
    player.Update(0.016f);
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
}

// ═══════════════════════════════════════════════════════════
// CutsceneSequence 深度
// ═══════════════════════════════════════════════════════════

TEST(CutsceneSequenceDeepTest, EmptySequenceEvaluateSafe) {
    CutsceneSequence seq("empty", 5.0f);
    seq.Evaluate(2.5f);
    seq.Reset();
}

TEST(CutsceneSequenceDeepTest, ManyTracks) {
    CutsceneSequence seq("multi", 10.0f);
    int call_count = 0;
    for (int i = 0; i < 10; ++i) {
        auto pt = std::make_shared<PropertyTrack>("Track" + std::to_string(i));
        pt->AddKeyframe(0.0f, 0.0f);
        pt->AddKeyframe(10.0f, 1.0f);
        pt->SetApplyCallback([&call_count](float) { ++call_count; });
        seq.AddTrack(pt);
    }

    seq.Evaluate(5.0f);
    EXPECT_EQ(call_count, 10);
}

// ═══════════════════════════════════════════════════════════
// AudioTrack 深度
// ═══════════════════════════════════════════════════════════

TEST(AudioTrackDeepTest, MultipleCuesOrdered) {
    AudioTrack track("Multi");
    std::vector<std::string> played;
    track.SetPlayCallback([&](const std::string& path, float, bool) {
        played.push_back(path);
    });

    track.AddCue(0.5f, "sfx1.ogg");
    track.AddCue(1.5f, "sfx2.ogg");
    track.AddCue(2.5f, "sfx3.ogg");

    track.Evaluate(1.0f);
    EXPECT_EQ(played.size(), 1u);

    track.Evaluate(2.0f);
    EXPECT_EQ(played.size(), 2u);

    track.Evaluate(3.0f);
    EXPECT_EQ(played.size(), 3u);
}

TEST(AudioTrackDeepTest, ResetAndReplay) {
    AudioTrack track("Replay");
    int count = 0;
    track.SetPlayCallback([&](const std::string&, float, bool) { ++count; });

    track.AddCue(0.5f, "sfx.ogg");
    track.Evaluate(1.0f);
    EXPECT_EQ(count, 1);

    track.Reset();
    track.Evaluate(1.0f);
    EXPECT_EQ(count, 2);
}

TEST(AudioTrackDeepTest, VolumeAndLoopParams) {
    AudioTrack track("Params");
    float recv_vol = 0.0f;
    bool recv_loop = false;
    track.SetPlayCallback([&](const std::string&, float vol, bool loop) {
        recv_vol = vol;
        recv_loop = loop;
    });

    track.AddCue(0.0f, "bgm.ogg", 0.7f, true);
    track.Evaluate(0.1f);
    EXPECT_FLOAT_EQ(recv_vol, 0.7f);
    EXPECT_TRUE(recv_loop);
}
