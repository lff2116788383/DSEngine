/**
 * @file cutscene_test.cpp
 * @brief 过场/导演系统单元测试
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include "engine/cutscene/cutscene_player.h"
#include "engine/cutscene/cutscene_track.h"
#include "engine/cutscene/cutscene_serialize.h"

using namespace dse::cutscene;

// ============================================================
// CameraTrack Tests
// ============================================================

class CameraTrackTest : public ::testing::Test {
protected:
    std::shared_ptr<CameraTrack> track;
    glm::vec3 last_pos{0.0f};
    glm::vec3 last_look{0.0f};
    float last_fov = 0.0f;

    void SetUp() override {
        track = std::make_shared<CameraTrack>("TestCam");
        track->SetApplyCallback([this](const glm::vec3& pos, const glm::vec3& look, float fov) {
            last_pos = pos;
            last_look = look;
            last_fov = fov;
        });
    }
};

TEST_F(CameraTrackTest, SingleKeyframe) {
    CameraKeyframe kf;
    kf.time = 0.0f;
    kf.position = glm::vec3(1.0f, 2.0f, 3.0f);
    kf.look_at = glm::vec3(0.0f, 0.0f, -1.0f);
    kf.fov = 90.0f;
    track->AddKeyframe(kf);

    track->Evaluate(0.0f);
    EXPECT_FLOAT_EQ(last_pos.x, 1.0f);
    EXPECT_FLOAT_EQ(last_pos.y, 2.0f);
    EXPECT_FLOAT_EQ(last_pos.z, 3.0f);
    EXPECT_FLOAT_EQ(last_fov, 90.0f);
}

TEST_F(CameraTrackTest, LinearInterpolation) {
    CameraKeyframe kf1;
    kf1.time = 0.0f;
    kf1.position = glm::vec3(0.0f, 0.0f, 0.0f);
    kf1.look_at = glm::vec3(0.0f);
    kf1.fov = 60.0f;

    CameraKeyframe kf2;
    kf2.time = 2.0f;
    kf2.position = glm::vec3(10.0f, 0.0f, 0.0f);
    kf2.look_at = glm::vec3(10.0f, 0.0f, 0.0f);
    kf2.fov = 90.0f;

    track->AddKeyframe(kf1);
    track->AddKeyframe(kf2);

    track->Evaluate(1.0f); // midpoint
    EXPECT_NEAR(last_pos.x, 5.0f, 0.01f);
    EXPECT_NEAR(last_fov, 75.0f, 0.01f);
}

TEST_F(CameraTrackTest, ClampAtEnd) {
    CameraKeyframe kf;
    kf.time = 1.0f;
    kf.position = glm::vec3(5.0f, 5.0f, 5.0f);
    kf.look_at = glm::vec3(0.0f);
    kf.fov = 45.0f;
    track->AddKeyframe(kf);

    track->Evaluate(10.0f); // past end
    EXPECT_FLOAT_EQ(last_pos.x, 5.0f);
}

// ============================================================
// PropertyTrack Tests
// ============================================================

class PropertyTrackTest : public ::testing::Test {
protected:
    std::shared_ptr<PropertyTrack> track;
    float last_value = 0.0f;

    void SetUp() override {
        track = std::make_shared<PropertyTrack>("Opacity");
        track->SetApplyCallback([this](float v) { last_value = v; });
    }
};

TEST_F(PropertyTrackTest, LinearInterpolation) {
    track->AddKeyframe(0.0f, 0.0f);
    track->AddKeyframe(1.0f, 1.0f);
    track->Evaluate(0.5f);
    EXPECT_NEAR(last_value, 0.5f, 0.01f);
}

TEST_F(PropertyTrackTest, StepInterpolation) {
    track->AddKeyframe(0.0f, 0.0f, InterpMode::Step);
    track->AddKeyframe(1.0f, 1.0f);
    track->Evaluate(0.5f);
    EXPECT_NEAR(last_value, 0.0f, 0.01f); // step holds first value
}

TEST_F(PropertyTrackTest, ClampBeforeFirstKeyframe) {
    track->AddKeyframe(1.0f, 5.0f);
    track->Evaluate(0.0f);
    EXPECT_FLOAT_EQ(last_value, 5.0f);
}

// ============================================================
// EventTrack Tests
// ============================================================

class EventTrackTest : public ::testing::Test {
protected:
    std::shared_ptr<EventTrack> track;
    std::vector<std::string> fired_events;

    void SetUp() override {
        track = std::make_shared<EventTrack>("Events");
        track->SetFireCallback([this](const std::string& name, const std::string& /*payload*/) {
            fired_events.push_back(name);
        });
    }
};

TEST_F(EventTrackTest, FiresAtCorrectTime) {
    track->AddEvent(0.5f, "explosion");
    track->AddEvent(1.5f, "dialogue");

    track->Evaluate(0.3f); // before first event
    EXPECT_EQ(fired_events.size(), 0u);

    track->Evaluate(0.6f); // past first event
    EXPECT_EQ(fired_events.size(), 1u);
    EXPECT_EQ(fired_events[0], "explosion");

    track->Evaluate(1.6f); // past second event
    EXPECT_EQ(fired_events.size(), 2u);
    EXPECT_EQ(fired_events[1], "dialogue");
}

TEST_F(EventTrackTest, DoesNotRefire) {
    track->AddEvent(0.5f, "once");
    track->Evaluate(1.0f);
    EXPECT_EQ(fired_events.size(), 1u);
    track->Evaluate(2.0f);
    EXPECT_EQ(fired_events.size(), 1u); // should not fire again
}

TEST_F(EventTrackTest, ResetAllowsRefire) {
    track->AddEvent(0.5f, "event");
    track->Evaluate(1.0f);
    EXPECT_EQ(fired_events.size(), 1u);
    track->Reset();
    track->Evaluate(1.0f);
    EXPECT_EQ(fired_events.size(), 2u);
}

// ============================================================
// AudioTrack Tests
// ============================================================

class AudioTrackTest : public ::testing::Test {
protected:
    std::shared_ptr<AudioTrack> track;
    std::vector<std::string> played_paths;

    void SetUp() override {
        track = std::make_shared<AudioTrack>("Audio");
        track->SetPlayCallback([this](const std::string& path, float, bool) {
            played_paths.push_back(path);
        });
    }
};

TEST_F(AudioTrackTest, PlaysAtCorrectTime) {
    track->AddCue(1.0f, "bgm.ogg", 0.8f, true);
    track->Evaluate(0.5f);
    EXPECT_EQ(played_paths.size(), 0u);
    track->Evaluate(1.5f);
    EXPECT_EQ(played_paths.size(), 1u);
    EXPECT_EQ(played_paths[0], "bgm.ogg");
}

// ============================================================
// CutsceneSequence Tests
// ============================================================

class CutsceneSequenceTest : public ::testing::Test {
protected:
    std::shared_ptr<CutsceneSequence> seq;
    float prop_value = 0.0f;
    std::vector<std::string> events;

    void SetUp() override {
        seq = std::make_shared<CutsceneSequence>("TestSeq", 5.0f);

        auto prop_track = std::make_shared<PropertyTrack>("Alpha");
        prop_track->AddKeyframe(0.0f, 0.0f);
        prop_track->AddKeyframe(5.0f, 1.0f);
        prop_track->SetApplyCallback([this](float v) { prop_value = v; });
        seq->AddTrack(prop_track);

        auto evt_track = std::make_shared<EventTrack>("Events");
        evt_track->AddEvent(2.5f, "midpoint");
        evt_track->SetFireCallback([this](const std::string& name, const std::string&) {
            events.push_back(name);
        });
        seq->AddTrack(evt_track);
    }
};

TEST_F(CutsceneSequenceTest, EvaluateAllTracks) {
    seq->Evaluate(2.5f);
    EXPECT_NEAR(prop_value, 0.5f, 0.01f);
    EXPECT_EQ(events.size(), 1u);
}

TEST_F(CutsceneSequenceTest, Reset) {
    seq->Evaluate(3.0f);
    seq->Reset();
    events.clear();
    seq->Evaluate(3.0f);
    EXPECT_EQ(events.size(), 1u); // midpoint fires again after reset
}

// ============================================================
// CutscenePlayer Tests
// ============================================================

class CutscenePlayerTest : public ::testing::Test {
protected:
    CutscenePlayer player;
    float prop_value = 0.0f;
    std::string finished_seq;

    void SetUp() override {
        auto seq = std::make_shared<CutsceneSequence>("intro", 2.0f);
        auto prop_track = std::make_shared<PropertyTrack>("fade");
        prop_track->AddKeyframe(0.0f, 0.0f);
        prop_track->AddKeyframe(2.0f, 1.0f);
        prop_track->SetApplyCallback([this](float v) { prop_value = v; });
        seq->AddTrack(prop_track);
        player.AddSequence(seq);

        player.SetFinishCallback([this](const std::string& name) {
            finished_seq = name;
        });
    }
};

TEST_F(CutscenePlayerTest, PlayAndUpdate) {
    player.Play("intro");
    EXPECT_EQ(player.GetState(), PlayState::Playing);
    player.Update(1.0f);
    EXPECT_NEAR(prop_value, 0.5f, 0.01f);
    EXPECT_NEAR(player.GetCurrentTime(), 1.0f, 0.01f);
}

TEST_F(CutscenePlayerTest, PauseAndResume) {
    player.Play("intro");
    player.Update(0.5f);
    player.Pause();
    EXPECT_EQ(player.GetState(), PlayState::Paused);
    player.Update(1.0f); // should not advance
    EXPECT_NEAR(player.GetCurrentTime(), 0.5f, 0.01f);
    player.Resume();
    player.Update(0.5f);
    EXPECT_NEAR(player.GetCurrentTime(), 1.0f, 0.01f);
}

TEST_F(CutscenePlayerTest, Stop) {
    player.Play("intro");
    player.Update(0.5f);
    player.Stop();
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.0f);
}

TEST_F(CutscenePlayerTest, FinishCallback) {
    player.Play("intro");
    player.Update(3.0f); // past duration
    EXPECT_EQ(finished_seq, "intro");
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
}

TEST_F(CutscenePlayerTest, PlayRate) {
    player.Play("intro");
    player.SetPlayRate(2.0f);
    player.Update(0.5f); // advances by 1.0
    EXPECT_NEAR(player.GetCurrentTime(), 1.0f, 0.01f);
}

TEST_F(CutscenePlayerTest, Seek) {
    player.Play("intro");
    player.Seek(1.5f);
    EXPECT_NEAR(player.GetCurrentTime(), 1.5f, 0.01f);
}

TEST_F(CutscenePlayerTest, Trigger) {
    bool trigger_condition = false;
    CutsceneTrigger trigger;
    trigger.sequence_name = "intro";
    trigger.condition = [&trigger_condition]() { return trigger_condition; };
    trigger.auto_remove = true;
    player.AddTrigger(trigger);

    player.Update(0.016f); // trigger not met, nothing plays
    EXPECT_EQ(player.GetState(), PlayState::Stopped);

    trigger_condition = true;
    player.Update(0.016f); // trigger fires, starts playing
    EXPECT_EQ(player.GetState(), PlayState::Playing);
    EXPECT_EQ(player.GetCurrentSequenceName(), "intro");
}

TEST_F(CutscenePlayerTest, RemoveSequence) {
    player.RemoveSequence("intro");
    player.Play("intro"); // should be no-op
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
}

TEST_F(CutscenePlayerTest, NonexistentSequence) {
    player.Play("nonexistent");
    EXPECT_EQ(player.GetState(), PlayState::Stopped);
}

// ============================================================
// .dcutscene 序列化契约
// ============================================================

namespace {

std::shared_ptr<CutsceneSequence> MakeSampleSequence() {
    auto seq = std::make_shared<CutsceneSequence>("Intro", 8.5f);

    auto cam = std::make_shared<CameraTrack>("MainCam");
    CameraKeyframe ck;
    ck.time = 0.0f;
    ck.position = glm::vec3(1.0f, 2.0f, 3.0f);
    ck.look_at = glm::vec3(0.0f, 0.0f, -1.0f);
    ck.fov = 75.0f;
    ck.interp = InterpMode::CubicBezier;
    cam->AddKeyframe(ck);
    seq->AddTrack(cam);

    auto prop = std::make_shared<PropertyTrack>("Intensity");
    prop->AddKeyframe(0.0f, 0.0f, InterpMode::Linear);
    prop->AddKeyframe(4.0f, 1.0f, InterpMode::Step);
    seq->AddTrack(prop);

    auto evt = std::make_shared<EventTrack>("Events");
    evt->AddEvent(3.0f, "PlayFX", "explosion");
    seq->AddTrack(evt);

    auto audio = std::make_shared<AudioTrack>("BGM");
    audio->AddCue(0.0f, "bgm/theme.wav", 0.8f, true);
    seq->AddTrack(audio);

    auto video = std::make_shared<VideoTrack>("Cinematic");
    VideoCue vc;
    vc.time = 1.0f;
    vc.video_path = "vid/intro.mp4";
    vc.fullscreen = false;
    vc.opacity = 0.5f;
    vc.fade_in = 0.25f;
    vc.fade_out = 0.75f;
    video->AddCue(vc);
    seq->AddTrack(video);

    return seq;
}

const CutsceneTrack* FindTrack(const CutsceneSequence& seq, const std::string& name) {
    for (const auto& t : seq.GetTracks()) {
        if (t && t->GetName() == name) return t.get();
    }
    return nullptr;
}

}  // namespace

TEST(CutsceneSerializeTest, RoundTrip) {
    auto src = MakeSampleSequence();
    std::string text = SerializeSequence(*src);
    ASSERT_FALSE(text.empty());

    CutsceneDiagnostics diag;
    auto dst = DeserializeSequence(text, diag);
    ASSERT_NE(dst, nullptr);
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, kCutsceneSchemaVersion);

    EXPECT_EQ(dst->GetName(), "Intro");
    EXPECT_FLOAT_EQ(dst->GetDuration(), 8.5f);
    ASSERT_EQ(dst->GetTracks().size(), src->GetTracks().size());

    const auto* cam = static_cast<const CameraTrack*>(FindTrack(*dst, "MainCam"));
    ASSERT_NE(cam, nullptr);
    ASSERT_EQ(cam->GetKeyframes().size(), 1u);
    EXPECT_FLOAT_EQ(cam->GetKeyframes()[0].fov, 75.0f);
    EXPECT_EQ(cam->GetKeyframes()[0].interp, InterpMode::CubicBezier);
    EXPECT_FLOAT_EQ(cam->GetKeyframes()[0].position.z, 3.0f);

    const auto* prop = static_cast<const PropertyTrack*>(FindTrack(*dst, "Intensity"));
    ASSERT_NE(prop, nullptr);
    ASSERT_EQ(prop->GetKeyframes().size(), 2u);
    EXPECT_EQ(prop->GetKeyframes()[1].interp, InterpMode::Step);
    EXPECT_FLOAT_EQ(prop->GetKeyframes()[1].value, 1.0f);

    const auto* evt = static_cast<const EventTrack*>(FindTrack(*dst, "Events"));
    ASSERT_NE(evt, nullptr);
    ASSERT_EQ(evt->GetEvents().size(), 1u);
    EXPECT_EQ(evt->GetEvents()[0].event_name, "PlayFX");
    EXPECT_EQ(evt->GetEvents()[0].payload, "explosion");

    const auto* audio = static_cast<const AudioTrack*>(FindTrack(*dst, "BGM"));
    ASSERT_NE(audio, nullptr);
    ASSERT_EQ(audio->GetCues().size(), 1u);
    EXPECT_EQ(audio->GetCues()[0].audio_path, "bgm/theme.wav");
    EXPECT_TRUE(audio->GetCues()[0].loop);

    const auto* video = static_cast<const VideoTrack*>(FindTrack(*dst, "Cinematic"));
    ASSERT_NE(video, nullptr);
    ASSERT_EQ(video->GetCues().size(), 1u);
    EXPECT_EQ(video->GetCues()[0].video_path, "vid/intro.mp4");
    EXPECT_FALSE(video->GetCues()[0].fullscreen);
    EXPECT_FLOAT_EQ(video->GetCues()[0].fade_out, 0.75f);
}

TEST(CutsceneSerializeTest, FileRoundTrip) {
    auto src = MakeSampleSequence();
    std::string path = std::string(::testing::TempDir()) + "dse_cutscene_roundtrip.dcutscene";

    CutsceneDiagnostics save_diag;
    ASSERT_TRUE(SaveSequenceToFile(*src, path, save_diag));

    CutsceneDiagnostics load_diag;
    auto dst = LoadSequenceFromFile(path, load_diag);
    ASSERT_NE(dst, nullptr);
    EXPECT_EQ(dst->GetTracks().size(), src->GetTracks().size());
    std::remove(path.c_str());
}

TEST(CutsceneSerializeTest, ForwardCompatibleVersion) {
    std::string json =
        "{\"version\":999,\"sequence\":{\"name\":\"A\",\"duration\":2.0,\"tracks\":[]}}";
    CutsceneDiagnostics diag;
    auto dst = DeserializeSequence(json, diag);
    ASSERT_NE(dst, nullptr);
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.warnings.empty());
    EXPECT_EQ(dst->GetName(), "A");
}

TEST(CutsceneSerializeTest, LegacyNoVersionMigration) {
    std::string json = "{\"name\":\"A\",\"duration\":2.0,\"tracks\":[]}";
    CutsceneDiagnostics diag;
    auto dst = DeserializeSequence(json, diag);
    ASSERT_NE(dst, nullptr);
    EXPECT_EQ(diag.source_version, 0);
    EXPECT_TRUE(diag.migrated);
}

TEST(CutsceneSerializeTest, UnknownTrackTypeWarns) {
    std::string json =
        "{\"version\":1,\"sequence\":{\"name\":\"A\",\"duration\":1.0,"
        "\"tracks\":[{\"name\":\"X\",\"type\":\"Bogus\"}]}}";
    CutsceneDiagnostics diag;
    auto dst = DeserializeSequence(json, diag);
    ASSERT_NE(dst, nullptr);
    EXPECT_TRUE(dst->GetTracks().empty());
    EXPECT_FALSE(diag.warnings.empty());
}

TEST(CutsceneSerializeTest, CorruptedJsonFails) {
    CutsceneDiagnostics diag;
    auto dst = DeserializeSequence("{not valid", diag);
    EXPECT_EQ(dst, nullptr);
    EXPECT_FALSE(diag.errors.empty());
}

TEST(CutsceneSerializeTest, NonObjectRootFails) {
    CutsceneDiagnostics diag;
    auto dst = DeserializeSequence("[1,2,3]", diag);
    EXPECT_EQ(dst, nullptr);
    EXPECT_FALSE(diag.errors.empty());
}
