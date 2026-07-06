/**
 * @file lua_binding_video.cpp
 * @brief Lua 薄包装：视频播放系统 (dse.video)
 *
 * 仅做 Lua 栈 ↔ C ABI 参数转换，所有逻辑委托 dse_video_* 函数。
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
 *   dse.video.get_info(player_id) → {width, height, fps, duration, ...}
 *   dse.video.get_texture(player_id) → texture_id
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// C ABI state enum → Lua string
static const char* StateToString(int state) {
    switch (state) {
        case 0: return "idle";
        case 1: return "playing";
        case 2: return "paused";
        case 3: return "stopped";
        case 4: return "ended";
        case 5: return "error";
        default: return "unknown";
    }
}

// dse.video.create_player() → player_id
int l_create_player(lua_State* L) {
    uint32_t id = dse_video_create_player();
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// dse.video.destroy_player(player_id)
int l_destroy_player(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_video_destroy_player(id);
    return 0;
}

// dse.video.play(player_id, path, config?)
int l_play(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);

    int loop = 0;
    float playback_rate = 1.0f;
    int decode_audio = 0;
    int prefetch_frames = 3;
    int backend = 0;  // Auto

    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "loop");
        if (!lua_isnil(L, -1)) loop = lua_toboolean(L, -1) ? 1 : 0;
        lua_pop(L, 1);

        lua_getfield(L, 3, "playback_rate");
        if (!lua_isnil(L, -1)) playback_rate = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 3, "decode_audio");
        if (!lua_isnil(L, -1)) decode_audio = lua_toboolean(L, -1) ? 1 : 0;
        lua_pop(L, 1);

        lua_getfield(L, 3, "prefetch_frames");
        if (!lua_isnil(L, -1)) prefetch_frames = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 3, "backend");
        if (lua_isstring(L, -1)) {
            const char* b = lua_tostring(L, -1);
            if (std::string(b) == "ffmpeg") backend = 1;
            else if (std::string(b) == "plmpeg") backend = 2;
            else backend = 0;
        }
        lua_pop(L, 1);
    }

    dse_video_play(id, path, loop, playback_rate, decode_audio, prefetch_frames, backend);
    return 0;
}

// dse.video.pause(player_id)
int l_pause(lua_State* L) {
    dse_video_pause(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

// dse.video.resume(player_id)
int l_resume(lua_State* L) {
    dse_video_resume(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

// dse.video.stop(player_id)
int l_stop(lua_State* L) {
    dse_video_stop(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

// dse.video.seek(player_id, time_sec)
int l_seek(lua_State* L) {
    dse_video_seek(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                   static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

// dse.video.set_loop(player_id, bool)
int l_set_loop(lua_State* L) {
    dse_video_set_loop(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                       lua_toboolean(L, 2) ? 1 : 0);
    return 0;
}

// dse.video.set_playback_rate(player_id, rate)
int l_set_playback_rate(lua_State* L) {
    dse_video_set_playback_rate(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

// dse.video.update(player_id, delta_time) → texture_id
int l_update(lua_State* L) {
    uint32_t tex = dse_video_update(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                    static_cast<float>(luaL_checknumber(L, 2)));
    lua_pushinteger(L, static_cast<lua_Integer>(tex));
    return 1;
}

// dse.video.get_state(player_id) → string
int l_get_state(lua_State* L) {
    int state = dse_video_get_state(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    lua_pushstring(L, StateToString(state));
    return 1;
}

// dse.video.get_time(player_id) → number
int l_get_time(lua_State* L) {
    float t = dse_video_get_time(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    lua_pushnumber(L, static_cast<lua_Number>(t));
    return 1;
}

// dse.video.get_duration(player_id) → number
int l_get_duration(lua_State* L) {
    float d = dse_video_get_duration(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    lua_pushnumber(L, static_cast<lua_Number>(d));
    return 1;
}

// dse.video.get_info(player_id) → table
int l_get_info(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int w = 0, h = 0, total_frames = 0, has_audio = 0, sample_rate = 0, channels = 0;
    float fps = 0, duration = 0;
    char codec[64] = {0};

    dse_video_get_info(id, &w, &h, &fps, &duration, &total_frames,
                       &has_audio, &sample_rate, &channels, codec, sizeof(codec));

    lua_newtable(L);
    lua_pushinteger(L, w);              lua_setfield(L, -2, "width");
    lua_pushinteger(L, h);              lua_setfield(L, -2, "height");
    lua_pushnumber(L, fps);             lua_setfield(L, -2, "fps");
    lua_pushnumber(L, duration);        lua_setfield(L, -2, "duration");
    lua_pushinteger(L, total_frames);   lua_setfield(L, -2, "total_frames");
    lua_pushboolean(L, has_audio);      lua_setfield(L, -2, "has_audio");
    lua_pushinteger(L, sample_rate);    lua_setfield(L, -2, "audio_sample_rate");
    lua_pushinteger(L, channels);       lua_setfield(L, -2, "audio_channels");
    lua_pushstring(L, codec);           lua_setfield(L, -2, "codec");
    return 1;
}

// dse.video.get_texture(player_id) → texture_id
int l_get_texture(lua_State* L) {
    uint32_t tex = dse_video_get_texture(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    lua_pushinteger(L, static_cast<lua_Integer>(tex));
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

    lua_newtable(L);
    luaL_setfuncs(L, video_funcs, 0);
    lua_setfield(L, -2, "video");
    lua_pop(L, 1);
    return 0;
}

// C ABI 管理所有状态，Lua 侧无需清理
void ShutdownVideoBindings() {}

} // namespace dse::runtime::lua_binding
