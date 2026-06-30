/**
 * @file video_player_test.cpp
 * @brief 视频播放系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/video/video_types.h"
#include "engine/video/video_decoder.h"
#include "engine/video/video_decoder_plmpeg.h"
#include "engine/video/video_player.h"
#include "engine/video/video_texture.h"
#include "engine/video/video_audio_sync.h"
#include "engine/video/video_screen_component.h"

using namespace dse::video;

// ============================================================================
// VideoTypes Tests
// ============================================================================

TEST(VideoTypes, StateToString) {
    EXPECT_STREQ(VideoStateToString(VideoState::Stopped), "stopped");
    EXPECT_STREQ(VideoStateToString(VideoState::Playing), "playing");
    EXPECT_STREQ(VideoStateToString(VideoState::Paused), "paused");
    EXPECT_STREQ(VideoStateToString(VideoState::Finished), "finished");
    EXPECT_STREQ(VideoStateToString(VideoState::Error), "error");
}

TEST(VideoTypes, DefaultConfig) {
    VideoPlayConfig config{};
    EXPECT_EQ(config.backend, DecoderBackend::Auto);
    EXPECT_FALSE(config.loop);
    EXPECT_FLOAT_EQ(config.playback_rate, 1.0f);
    EXPECT_TRUE(config.decode_audio);
    EXPECT_FALSE(config.hw_accel);
    EXPECT_EQ(config.prefetch_frames, 4);
    EXPECT_TRUE(config.yuv_gpu_convert);
}

TEST(VideoTypes, DefaultVideoInfo) {
    VideoInfo info{};
    EXPECT_EQ(info.width, 0);
    EXPECT_EQ(info.height, 0);
    EXPECT_DOUBLE_EQ(info.fps, 0.0);
    EXPECT_DOUBLE_EQ(info.duration, 0.0);
    EXPECT_EQ(info.total_frames, 0);
    EXPECT_FALSE(info.has_audio);
}

// ============================================================================
// VideoTexture Tests
// ============================================================================

TEST(VideoTexture, InitializeAndDestroy) {
    VideoTexture tex;
    EXPECT_FALSE(tex.IsInitialized());
    EXPECT_EQ(tex.GetTextureId(), 0u);

    tex.Initialize(1920, 1080, PixelFormat::RGBA8);
    EXPECT_TRUE(tex.IsInitialized());
    EXPECT_NE(tex.GetTextureId(), 0u);
    EXPECT_EQ(tex.GetWidth(), 1920);
    EXPECT_EQ(tex.GetHeight(), 1080);

    tex.Destroy();
    EXPECT_FALSE(tex.IsInitialized());
    EXPECT_EQ(tex.GetTextureId(), 0u);
}

TEST(VideoTexture, UploadAutoInitializes) {
    VideoTexture tex;

    VideoFrame frame{};
    frame.format = PixelFormat::RGBA8;
    frame.width = 640;
    frame.height = 480;
    uint8_t dummy[4] = {255, 0, 0, 255};
    frame.planes[0] = dummy;
    frame.strides[0] = 640 * 4;

    uint32_t id = tex.Upload(frame);
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(tex.IsInitialized());
    EXPECT_EQ(tex.GetWidth(), 640);
    EXPECT_EQ(tex.GetHeight(), 480);
}

TEST(VideoTexture, UploadResizeReinitializes) {
    VideoTexture tex;
    tex.Initialize(320, 240, PixelFormat::RGBA8);
    uint32_t old_id = tex.GetTextureId();

    VideoFrame frame{};
    frame.format = PixelFormat::RGBA8;
    frame.width = 640;
    frame.height = 480;
    uint8_t dummy[4] = {0};
    frame.planes[0] = dummy;
    frame.strides[0] = 640 * 4;

    uint32_t new_id = tex.Upload(frame);
    EXPECT_NE(new_id, old_id);
    EXPECT_EQ(tex.GetWidth(), 640);
    EXPECT_EQ(tex.GetHeight(), 480);
}

// ============================================================================
// PlmpegDecoder Tests (without actual file - test error handling)
// ============================================================================

TEST(PlmpegDecoder, OpenNonexistentFile) {
    PlmpegDecoder dec;
    EXPECT_FALSE(dec.Open("nonexistent_file.mpg"));
    // Not opened, so IsEOF is based on internal eof_ flag (false) and plm_==null
    VideoFrame frame{};
    EXPECT_FALSE(dec.DecodeNextFrame(frame));
}

TEST(PlmpegDecoder, BackendType) {
    PlmpegDecoder dec;
    EXPECT_EQ(dec.GetBackend(), DecoderBackend::PlMpeg);
}

TEST(PlmpegDecoder, CloseWithoutOpen) {
    PlmpegDecoder dec;
    dec.Close(); // Should not crash
    VideoFrame frame{};
    EXPECT_FALSE(dec.DecodeNextFrame(frame));
}

TEST(PlmpegDecoder, SeekWithoutOpen) {
    PlmpegDecoder dec;
    EXPECT_FALSE(dec.Seek(5.0));
}

TEST(PlmpegDecoder, DecodeWithoutOpen) {
    PlmpegDecoder dec;
    VideoFrame frame{};
    EXPECT_FALSE(dec.DecodeNextFrame(frame));
}

TEST(PlmpegDecoder, AudioWithoutOpen) {
    PlmpegDecoder dec;
    float buf[1024];
    EXPECT_EQ(dec.DecodeAudio(buf, 1024), 0);
}

TEST(PlmpegDecoder, DefaultInfo) {
    PlmpegDecoder dec;
    auto info = dec.GetInfo();
    EXPECT_EQ(info.width, 0);
    EXPECT_EQ(info.height, 0);
    EXPECT_DOUBLE_EQ(info.fps, 0.0);
}

// ============================================================================
// VideoPlayer Tests
// ============================================================================

TEST(VideoPlayer, InitialState) {
    VideoPlayer player;
    EXPECT_EQ(player.GetState(), VideoState::Stopped);
    EXPECT_DOUBLE_EQ(player.GetCurrentTime(), 0.0);
    EXPECT_DOUBLE_EQ(player.GetDuration(), 0.0);
    EXPECT_EQ(player.GetCurrentTexture(), 0u);
    EXPECT_FLOAT_EQ(player.GetPlaybackRate(), 1.0f);
    EXPECT_FALSE(player.IsLooping());
}

TEST(VideoPlayer, PlayNonexistentFile) {
    VideoPlayer player;
    player.Play("nonexistent.mpg");
    EXPECT_EQ(player.GetState(), VideoState::Error);
}

TEST(VideoPlayer, PauseWhileStopped) {
    VideoPlayer player;
    player.Pause(); // Should not crash, state stays Stopped
    EXPECT_EQ(player.GetState(), VideoState::Stopped);
}

TEST(VideoPlayer, ResumeWhileStopped) {
    VideoPlayer player;
    player.Resume();
    EXPECT_EQ(player.GetState(), VideoState::Stopped);
}

TEST(VideoPlayer, StopWhileStopped) {
    VideoPlayer player;
    player.Stop(); // Should not crash
    EXPECT_EQ(player.GetState(), VideoState::Stopped);
}

TEST(VideoPlayer, SetPlaybackRate) {
    VideoPlayer player;
    player.SetPlaybackRate(2.0f);
    EXPECT_FLOAT_EQ(player.GetPlaybackRate(), 2.0f);

    // Clamp to range [0.1, 10]
    player.SetPlaybackRate(0.01f);
    EXPECT_FLOAT_EQ(player.GetPlaybackRate(), 0.1f);

    player.SetPlaybackRate(100.0f);
    EXPECT_FLOAT_EQ(player.GetPlaybackRate(), 10.0f);
}

TEST(VideoPlayer, SetLoop) {
    VideoPlayer player;
    EXPECT_FALSE(player.IsLooping());
    player.SetLoop(true);
    EXPECT_TRUE(player.IsLooping());
    player.SetLoop(false);
    EXPECT_FALSE(player.IsLooping());
}

TEST(VideoPlayer, UpdateWhileStopped) {
    VideoPlayer player;
    uint32_t tex = player.Update(0.016f);
    EXPECT_EQ(tex, 0u);
}

TEST(VideoPlayer, ErrorCallback) {
    VideoPlayer player;
    bool error_called = false;
    std::string error_msg;
    player.SetOnError([&](const std::string& msg) {
        error_called = true;
        error_msg = msg;
    });

    player.Play("missing.mpg");
    EXPECT_TRUE(error_called);
    EXPECT_FALSE(error_msg.empty());
}

// ============================================================================
// CreateDecoder Factory Tests
// ============================================================================

TEST(DecoderFactory, CreateAutoReturnsPlmpeg) {
    auto dec = CreateDecoder(DecoderBackend::Auto);
    EXPECT_NE(dec, nullptr);
    EXPECT_EQ(dec->GetBackend(), DecoderBackend::PlMpeg);
}

TEST(DecoderFactory, CreatePlmpeg) {
    auto dec = CreateDecoder(DecoderBackend::PlMpeg);
    EXPECT_NE(dec, nullptr);
    EXPECT_EQ(dec->GetBackend(), DecoderBackend::PlMpeg);
}

// ============================================================================
// VideoAudioSync Tests
// ============================================================================

TEST(VideoAudioSync, InitialState) {
    VideoAudioSync sync;
    EXPECT_FALSE(sync.IsRunning());
    EXPECT_DOUBLE_EQ(sync.GetAudioClock(), 0.0);
    EXPECT_FALSE(sync.HasFrames());
}

TEST(VideoAudioSync, UpdateAudioClock) {
    VideoAudioSync sync;
    sync.UpdateAudioClock(1.5);
    EXPECT_DOUBLE_EQ(sync.GetAudioClock(), 1.5);
    sync.UpdateAudioClock(3.0);
    EXPECT_DOUBLE_EQ(sync.GetAudioClock(), 3.0);
}

TEST(VideoAudioSync, StopWithoutStart) {
    VideoAudioSync sync;
    sync.Stop(); // Should not crash
    EXPECT_FALSE(sync.IsRunning());
}

// ============================================================================
// VideoScreenComponent Tests
// ============================================================================

TEST(VideoScreenComponent, DefaultValues) {
    dse::video::VideoScreenComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.video_path.empty());
    EXPECT_TRUE(comp.auto_play);
    EXPECT_TRUE(comp.loop);
    EXPECT_FLOAT_EQ(comp.playback_rate, 1.0f);
    EXPECT_EQ(comp.backend, DecoderBackend::Auto);
    EXPECT_EQ(comp.player_handle, 0u);
    EXPECT_EQ(comp.current_texture, 0u);
}
