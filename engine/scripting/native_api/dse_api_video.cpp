/**
 * @file dse_api_video.cpp
 * @brief DSEngine C ABI - Video — 使用 VideoPlayer + 句柄表
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/video/video_player.h"
#include "engine/render/rhi/rhi_device.h"

using namespace dse;
using namespace dse_api_internal;


static std::unordered_map<uint32_t, std::unique_ptr<dse::video::VideoPlayer>> g_video_players;
static uint32_t g_video_next_id = 1;
static void* g_video_rhi_device = nullptr;

static dse::video::VideoPlayer* GetVideoPlayer(uint32_t id) {
    auto it = g_video_players.find(id);
    return it != g_video_players.end() ? it->second.get() : nullptr;
}

extern "C" void dse_video_set_rhi_device(void* rhi_device) {
    g_video_rhi_device = rhi_device;
    // 已存在的播放器同步换绑（新播放器在 create 时即注入）。
    for (auto& [id, player] : g_video_players) {
        (void)id;
        if (player) player->SetRhiDevice(static_cast<dse::render::RhiDevice*>(g_video_rhi_device));
    }
}

extern "C" uint32_t dse_video_create_player(void) {
    uint32_t id = g_video_next_id++;
    auto player = std::make_unique<dse::video::VideoPlayer>();
    if (g_video_rhi_device) {
        player->SetRhiDevice(static_cast<dse::render::RhiDevice*>(g_video_rhi_device));
    }
    g_video_players[id] = std::move(player);
    return id;
}

extern "C" void dse_video_destroy_player(uint32_t player) {
    g_video_players.erase(player);
}

extern "C" void dse_video_play(uint32_t player, const char* path, int loop, float playback_rate,
                             int decode_audio, int prefetch_frames, int backend) {
    auto* p = GetVideoPlayer(player);
    if (!p || !path) return;
    dse::video::VideoPlayConfig config{};
    config.loop = (loop != 0);
    config.playback_rate = playback_rate;
    config.decode_audio = (decode_audio != 0);
    config.prefetch_frames = prefetch_frames;
    config.backend = static_cast<dse::video::DecoderBackend>(backend);
    p->Play(path, config);
}

extern "C" void dse_video_pause(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Pause();
}

extern "C" void dse_video_resume(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Resume();
}

extern "C" void dse_video_stop(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Stop();
}

extern "C" void dse_video_seek(uint32_t player, float time) {
    auto* p = GetVideoPlayer(player);
    if (p) p->Seek(time);
}

extern "C" void dse_video_set_loop(uint32_t player, int loop) {
    auto* p = GetVideoPlayer(player);
    if (p) p->SetLoop(loop != 0);
}

extern "C" void dse_video_set_playback_rate(uint32_t player, float rate) {
    auto* p = GetVideoPlayer(player);
    if (p) p->SetPlaybackRate(rate);
}

extern "C" uint32_t dse_video_update(uint32_t player, float delta_time) {
    auto* p = GetVideoPlayer(player);
    if (!p) return 0;
    return p->Update(delta_time);
}

extern "C" int dse_video_get_state(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? static_cast<int>(p->GetState()) : 0;
}

extern "C" float dse_video_get_time(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? p->GetCurrentTime() : 0.0f;
}

extern "C" float dse_video_get_duration(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? p->GetDuration() : 0.0f;
}

extern "C" void dse_video_get_info(uint32_t player, int* out_w, int* out_h, float* out_fps,
                                 float* out_duration, int* out_total_frames, int* out_has_audio,
                                 int* out_sample_rate, int* out_channels,
                                 char* out_codec, int codec_cap) {
    auto* p = GetVideoPlayer(player);
    if (!p) return;
    const auto& info = p->GetInfo();
    if (out_w) *out_w = info.width;
    if (out_h) *out_h = info.height;
    if (out_fps) *out_fps = info.fps;
    if (out_duration) *out_duration = info.duration;
    if (out_total_frames) *out_total_frames = info.total_frames;
    if (out_has_audio) *out_has_audio = info.has_audio ? 1 : 0;
    if (out_sample_rate) *out_sample_rate = info.audio_sample_rate;
    if (out_channels) *out_channels = info.audio_channels;
    if (out_codec && codec_cap > 0) {
        strncpy(out_codec, info.codec_name.c_str(), codec_cap - 1);
        out_codec[codec_cap - 1] = '\0';
    }
}

extern "C" uint32_t dse_video_get_texture(uint32_t player) {
    auto* p = GetVideoPlayer(player);
    return p ? p->GetCurrentTexture() : 0;
}

