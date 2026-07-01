/**
 * @file lua_binding_video.cpp
 * @brief Lua 绑定：视频播放系统
 *
 * 全局表 "dse.video":
 *   dse.video.create_player() → player_id
 *   dse.video.destroy_player(player_id)
 *   dse.video.play(player_id, path, config?)
 *   dse.video.pause(player_id)
 *   dse.video.resume(player_id)
 *   dse.video.stop(player_id)
 *   dse.video.seek(player_id, time_sec)
 *   dse.video.set_loop(player_id, bool)
 *   dse.video.set_playback_rate(player_id, rate)
 *   dse.video.update(player_id, delta_time) → texture_id
 *   dse.video.get_state(player_id) → string
 *   dse.video.get_time(player_id) → number
 *   dse.video.get_duration(player_id) → number
 *   dse.video.get_info(player_id) → {width, height, fps, duration, codec, ...}
 *   dse.video.get_texture(player_id) → texture_id
 */

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include "engine/video/video_player.h"
#include <unordered_map>
#include <cstdint>

using namespace dse::video;

namespace {

static uint32_t s_next_player_id = 1;
static std::unordered_map<uint32_t, std::unique_ptr<VideoPlayer>> s_players;

VideoPlayer* GetPlayer(lua_State* L, int arg_index = 1) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, arg_index));
    auto it = s_players.find(id);
    if (it == s_players.end()) {
        luaL_error(L, "Invalid video player id: %u", id);
        return nullptr;
    }
    return it->second.get();
}

// dse.video.create_player() → player_id
int l_create_player(lua_State* L) {
    uint32_t id = s_next_player_id++;
    s_players[id] = std::make_unique<VideoPlayer>();
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// dse.video.destroy_player(player_id)
int l_destroy_player(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    s_players.erase(id);
    return 0;
}

// dse.video.play(player_id, path, config?)
int l_play(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    const char* path = luaL_checkstring(L, 2);

    VideoPlayConfig config{};
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "loop");
        if (!lua_isnil(L, -1)) config.loop = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        lua_getfield(L, 3, "playback_rate");
        if (!lua_isnil(L, -1)) config.playback_rate = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 3, "decode_audio");
        if (!lua_isnil(L, -1)) config.decode_audio = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        lua_getfield(L, 3, "prefetch_frames");
        if (!lua_isnil(L, -1)) config.prefetch_frames = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 3, "backend");
        if (lua_isstring(L, -1)) {
            const char* b = lua_tostring(L, -1);
            if (std::string(b) == "ffmpeg") config.backend = DecoderBackend::FFmpeg;
            else if (std::string(b) == "plmpeg") config.backend = DecoderBackend::PlMpeg;
            else config.backend = DecoderBackend::Auto;
        }
        lua_pop(L, 1);
    }

    player->Play(path, config);
    return 0;
}

// dse.video.pause(player_id)
int l_pause(lua_State* L) {
    GetPlayer(L, 1)->Pause();
    return 0;
}

// dse.video.resume(player_id)
int l_resume(lua_State* L) {
    GetPlayer(L, 1)->Resume();
    return 0;
}

// dse.video.stop(player_id)
int l_stop(lua_State* L) {
    GetPlayer(L, 1)->Stop();
    return 0;
}

// dse.video.seek(player_id, time_sec)
int l_seek(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    double t = luaL_checknumber(L, 2);
    player->Seek(t);
    return 0;
}

// dse.video.set_loop(player_id, bool)
int l_set_loop(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    bool loop = lua_toboolean(L, 2) != 0;
    player->SetLoop(loop);
    return 0;
}

// dse.video.set_playback_rate(player_id, rate)
int l_set_playback_rate(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    float rate = static_cast<float>(luaL_checknumber(L, 2));
    player->SetPlaybackRate(rate);
    return 0;
}

// dse.video.update(player_id, delta_time) → texture_id
int l_update(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    float dt = static_cast<float>(luaL_checknumber(L, 2));
    uint32_t tex = player->Update(dt);
    lua_pushinteger(L, static_cast<lua_Integer>(tex));
    return 1;
}

// dse.video.get_state(player_id) → string
int l_get_state(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    lua_pushstring(L, VideoStateToString(player->GetState()));
    return 1;
}

// dse.video.get_time(player_id) → number
int l_get_time(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    lua_pushnumber(L, player->GetCurrentTime());
    return 1;
}

// dse.video.get_duration(player_id) → number
int l_get_duration(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    lua_pushnumber(L, player->GetDuration());
    return 1;
}

// dse.video.get_info(player_id) → table
int l_get_info(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    const auto& info = player->GetInfo();

    lua_newtable(L);
    lua_pushinteger(L, info.width); lua_setfield(L, -2, "width");
    lua_pushinteger(L, info.height); lua_setfield(L, -2, "height");
    lua_pushnumber(L, info.fps); lua_setfield(L, -2, "fps");
    lua_pushnumber(L, info.duration); lua_setfield(L, -2, "duration");
    lua_pushinteger(L, static_cast<lua_Integer>(info.total_frames)); lua_setfield(L, -2, "total_frames");
    lua_pushboolean(L, info.has_audio); lua_setfield(L, -2, "has_audio");
    lua_pushinteger(L, info.audio_sample_rate); lua_setfield(L, -2, "audio_sample_rate");
    lua_pushinteger(L, info.audio_channels); lua_setfield(L, -2, "audio_channels");
    lua_pushstring(L, info.codec_name.c_str()); lua_setfield(L, -2, "codec");

    return 1;
}

// dse.video.get_texture(player_id) → texture_id
int l_get_texture(lua_State* L) {
    auto* player = GetPlayer(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(player->GetCurrentTexture()));
    return 1;
}

static const luaL_Reg video_funcs[] = {
    {"create_player",    l_create_player},
    {"destroy_player",   l_destroy_player},
    {"play",             l_play},
    {"pause",            l_pause},
    {"resume",           l_resume},
    {"stop",             l_stop},
    {"seek",             l_seek},
    {"set_loop",         l_set_loop},
    {"set_playback_rate", l_set_playback_rate},
    {"update",           l_update},
    {"get_state",        l_get_state},
    {"get_time",         l_get_time},
    {"get_duration",     l_get_duration},
    {"get_info",         l_get_info},
    {"get_texture",      l_get_texture},
    {nullptr, nullptr}
};

} // anonymous namespace

extern "C" int luaopen_dse_video(lua_State* L) {
    // Create dse table if not exists
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, "dse");
        lua_getglobal(L, "dse");
    }

    // Create dse.video table
    lua_newtable(L);
    luaL_setfuncs(L, video_funcs, 0);
    lua_setfield(L, -2, "video");
    lua_pop(L, 1);

    return 0;
}

// Cleanup function for shutdown
void dse_video_lua_shutdown() {
    s_players.clear();
    s_next_player_id = 1;
}

namespace dse::runtime::lua_binding {
void ShutdownVideoBindings() {
    dse_video_lua_shutdown();
}
} // namespace dse::runtime::lua_binding
