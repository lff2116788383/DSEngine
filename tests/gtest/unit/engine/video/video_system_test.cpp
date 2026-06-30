/**
 * @file video_system_test.cpp
 * @brief 视频系统 Stage 2/3 单元测试
 */

#include <gtest/gtest.h>
#include "engine/video/video_decoder_ffmpeg.h"
#include "engine/video/video_system.h"
#include "engine/video/video_audio_sync.h"
#include "engine/video/video_screen_component.h"
#include "engine/cutscene/cutscene_track.h"

using namespace dse::video;
using namespace dse::cutscene;

// ============================================================================
// FFmpegDecoder Tests (graceful degradation when FFmpeg not installed)
// ============================================================================

TEST(FFmpegDecoder, BackendType) {
    FFmpegDecoder dec;
    EXPECT_EQ(dec.GetBackend(), DecoderBackend::FFmpeg);
}

TEST(FFmpegDecoder, OpenFailsGracefully) {
    FFmpegDecoder dec;
    // FFmpeg DLLs not present on build machine - Open should fail gracefully
    bool result = dec.Open("test.mp4");
    if (!dec.IsLoaded()) {
        EXPECT_FALSE(result);
    }
}

TEST(FFmpegDecoder, IsFFmpegAvailableDoesNotCrash) {
    // Should return false if FFmpeg DLLs not present, without crashing
    bool available = IsFFmpegAvailable();
    (void)available; // Result depends on environment
}

TEST(FFmpegDecoder, CloseWithoutOpen) {
    FFmpegDecoder dec;
    dec.Close(); // Should not crash
}

TEST(FFmpegDecoder, SeekWithoutOpen) {
    FFmpegDecoder dec;
    EXPECT_FALSE(dec.Seek(5.0));
}

TEST(FFmpegDecoder, DecodeWithoutOpen) {
    FFmpegDecoder dec;
    VideoFrame frame{};
    EXPECT_FALSE(dec.DecodeNextFrame(frame));
}

TEST(FFmpegDecoder, AudioWithoutOpen) {
    FFmpegDecoder dec;
    float buf[1024];
    EXPECT_EQ(dec.DecodeAudio(buf, 1024), 0);
}

// ============================================================================
// Decoder Factory with FFmpeg fallback
// ============================================================================

TEST(DecoderFactory, AutoFallsBackToPlmpeg) {
    auto dec = CreateDecoder(DecoderBackend::Auto);
    EXPECT_NE(dec, nullptr);
    // If FFmpeg not available, should fallback to PlMpeg
    if (!IsFFmpegAvailable()) {
        EXPECT_EQ(dec->GetBackend(), DecoderBackend::PlMpeg);
    }
}

TEST(DecoderFactory, RequestFFmpegFallsBackToPlmpeg) {
    auto dec = CreateDecoder(DecoderBackend::FFmpeg);
    EXPECT_NE(dec, nullptr);
    // If FFmpeg not available, should fallback to PlMpeg
    if (!IsFFmpegAvailable()) {
        EXPECT_EQ(dec->GetBackend(), DecoderBackend::PlMpeg);
    }
}

// ============================================================================
// VideoSystem Tests
// ============================================================================

TEST(VideoSystem, CreateAndDestroyPlayer) {
    VideoSystem sys;
    EXPECT_EQ(sys.GetActivePlayerCount(), 0u);

    VideoScreenComponent config;
    config.auto_play = false;
    uint32_t h = sys.CreatePlayer(config);
    EXPECT_NE(h, 0u);
    EXPECT_EQ(sys.GetActivePlayerCount(), 1u);

    sys.DestroyPlayer(h);
    EXPECT_EQ(sys.GetActivePlayerCount(), 0u);
}

TEST(VideoSystem, MultiplePlayersIndependent) {
    VideoSystem sys;
    VideoScreenComponent config;
    config.auto_play = false;

    uint32_t h1 = sys.CreatePlayer(config);
    uint32_t h2 = sys.CreatePlayer(config);
    uint32_t h3 = sys.CreatePlayer(config);

    EXPECT_EQ(sys.GetActivePlayerCount(), 3u);
    EXPECT_NE(h1, h2);
    EXPECT_NE(h2, h3);

    sys.DestroyPlayer(h2);
    EXPECT_EQ(sys.GetActivePlayerCount(), 2u);

    sys.Shutdown();
    EXPECT_EQ(sys.GetActivePlayerCount(), 0u);
}

TEST(VideoSystem, PlayerStateTracking) {
    VideoSystem sys;
    VideoScreenComponent config;
    config.auto_play = false;
    uint32_t h = sys.CreatePlayer(config);

    EXPECT_EQ(sys.GetPlayerState(h), VideoState::Stopped);
    EXPECT_EQ(sys.GetPlayerTexture(h), 0u);
}

TEST(VideoSystem, InvalidHandleReturnsDefaults) {
    VideoSystem sys;
    EXPECT_EQ(sys.GetPlayerState(9999), VideoState::Stopped);
    EXPECT_EQ(sys.GetPlayerTexture(9999), 0u);

    // These should not crash
    sys.PlayerPause(9999);
    sys.PlayerResume(9999);
    sys.PlayerStop(9999);
    sys.PlayerSeek(9999, 5.0);
}

TEST(VideoSystem, UpdateWithNoPlayers) {
    VideoSystem sys;
    sys.Update(0.016f); // Should not crash
}

TEST(VideoSystem, UpdateWithStoppedPlayer) {
    VideoSystem sys;
    VideoScreenComponent config;
    config.auto_play = false;
    sys.CreatePlayer(config);
    sys.Update(0.016f); // Player is stopped, should be no-op
}

// ============================================================================
// VideoTrack (CutscenePlayer integration) Tests
// ============================================================================

TEST(VideoTrack, Initialization) {
    VideoTrack track("TestVideoTrack");
    EXPECT_EQ(track.GetName(), "TestVideoTrack");
    EXPECT_EQ(track.GetType(), TrackType::Video);
    EXPECT_TRUE(track.GetCues().empty());
}

TEST(VideoTrack, AddCue) {
    VideoTrack track;
    VideoCue cue;
    cue.time = 2.0f;
    cue.video_path = "cutscenes/intro.mpg";
    cue.fullscreen = true;
    cue.opacity = 1.0f;
    cue.fade_in = 0.5f;
    track.AddCue(cue);

    EXPECT_EQ(track.GetCues().size(), 1u);
    EXPECT_EQ(track.GetCues()[0].video_path, "cutscenes/intro.mpg");
    EXPECT_FLOAT_EQ(track.GetCues()[0].time, 2.0f);
}

TEST(VideoTrack, EvaluateFiresCallback) {
    VideoTrack track;
    VideoCue cue;
    cue.time = 1.0f;
    cue.video_path = "test.mpg";
    cue.fullscreen = false;
    cue.opacity = 0.8f;
    track.AddCue(cue);

    bool called = false;
    std::string received_path;
    track.SetPlayCallback([&](const std::string& path, bool fs, float op) {
        called = true;
        received_path = path;
        (void)fs; (void)op;
    });

    // Evaluate before cue time - should not fire
    track.Evaluate(0.5f);
    EXPECT_FALSE(called);

    // Evaluate past cue time - should fire
    track.Evaluate(1.5f);
    EXPECT_TRUE(called);
    EXPECT_EQ(received_path, "test.mpg");
}

TEST(VideoTrack, ResetCallsStop) {
    VideoTrack track;
    bool stop_called = false;
    track.SetStopCallback([&]() { stop_called = true; });

    track.Reset();
    EXPECT_TRUE(stop_called);
}

TEST(VideoTrack, MultipleCues) {
    VideoTrack track;
    VideoCue cue1, cue2;
    cue1.time = 1.0f;
    cue1.video_path = "video1.mpg";
    cue2.time = 5.0f;
    cue2.video_path = "video2.mpg";
    track.AddCue(cue1);
    track.AddCue(cue2);

    EXPECT_EQ(track.GetCues().size(), 2u);

    int play_count = 0;
    std::string last_path;
    track.SetPlayCallback([&](const std::string& path, bool, float) {
        play_count++;
        last_path = path;
    });

    track.Evaluate(2.0f); // Should fire cue1
    EXPECT_EQ(play_count, 1);
    EXPECT_EQ(last_path, "video1.mpg");

    track.Evaluate(6.0f); // Should fire cue2
    EXPECT_EQ(play_count, 2);
    EXPECT_EQ(last_path, "video2.mpg");
}

TEST(VideoTrack, NoDuplicateFireOnReEvaluate) {
    VideoTrack track;
    VideoCue cue;
    cue.time = 1.0f;
    cue.video_path = "test.mpg";
    track.AddCue(cue);

    int count = 0;
    track.SetPlayCallback([&](const std::string&, bool, float) { count++; });

    track.Evaluate(2.0f);
    EXPECT_EQ(count, 1);

    track.Evaluate(3.0f); // Same cue already fired, should not fire again
    EXPECT_EQ(count, 1);
}

// ============================================================================
// VideoCue struct defaults
// ============================================================================

TEST(VideoCue, DefaultValues) {
    VideoCue cue;
    EXPECT_FLOAT_EQ(cue.time, 0.0f);
    EXPECT_TRUE(cue.video_path.empty());
    EXPECT_TRUE(cue.fullscreen);
    EXPECT_FLOAT_EQ(cue.opacity, 1.0f);
    EXPECT_FLOAT_EQ(cue.fade_in, 0.0f);
    EXPECT_FLOAT_EQ(cue.fade_out, 0.0f);
}
