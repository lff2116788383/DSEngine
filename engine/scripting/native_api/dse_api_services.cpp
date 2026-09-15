/**
 * @file dse_api_services.cpp
 * @brief DSEngine Native C ABI — 服务型子系统（手写）
 *
 * 覆盖 Audio（全局 + ECS AudioSource）、Navigation（NavMesh + Agent）、
 * Localization、Scene/Prefab 序列化、UI 核心控件。
 * 供 Lua / C# 共享同一实现，语义与原 Lua 绑定逐一等价。
 *
 * 约定：
 *   - bool 参数/返回值使用 int(0/1)。
 *   - 浮点参数 NaN = 保持当前值（按声明）。
 *   - 字符串输出走 out 缓冲（null 结尾，按 cap 截断），返回写入长度。
 *   - 服务缺失（AudioSystem/NavMeshSystem/LocalizationManager 未注册）时安全返回 0/无操作。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/ui_serializer.h"
#include "engine/ecs/sprite.h"
#include "engine/audio/audio_system.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/localization_manager.h"
#include "engine/scene/scene.h"
#include "engine/core/service_locator.h"
#include "engine/render/font/font_service.h"

#ifdef DSE_ENABLE_NAVMESH
#include "engine/navigation/nav_mesh_system.h"
#include "engine/ecs/components_3d_ai.h"
#endif

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }
inline gameplay2d::AudioSystem* GAS() {
    return static_cast<gameplay2d::AudioSystem*>(dse_get_audio_system_ptr());
}
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
inline bool Keep(float v) { return std::isnan(v); }  // NaN => 保持当前值

template <typename T>
inline T* GetComp(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return nullptr;
    return w->registry().try_get<T>(TE(e));
}

// 拷贝 std::string 到 out 缓冲（null 结尾，按 cap 截断），返回写入长度。
inline int CopyStr(const std::string& s, char* out, int cap) {
    if (!out || cap <= 0) return 0;
    int n = static_cast<int>(s.size());
    if (n > cap - 1) n = cap - 1;
    std::memcpy(out, s.data(), static_cast<size_t>(n));
    out[n] = '\0';
    return n;
}

}  // namespace

// ============================================================
// Audio — 全局（AudioSystem 委托）
// ============================================================

extern "C" int dse_audio_play_bgm(const char* path, float volume, int loop) {
    auto* audio = GAS();
    if (!audio || !path) return 0;
    return audio->PlayBgm(path, volume, loop != 0) ? 1 : 0;
}

extern "C" void dse_audio_pause_bgm(void) {
    if (auto* audio = GAS()) audio->PauseBgm();
}

extern "C" void dse_audio_resume_bgm(void) {
    if (auto* audio = GAS()) audio->ResumeBgm();
}

extern "C" void dse_audio_stop_bgm(void) {
    if (auto* audio = GAS()) audio->StopBgm();
}

extern "C" int dse_audio_crossfade_bgm(const char* path, float fade_sec, float volume, int loop) {
    auto* audio = GAS();
    if (!audio || !path) return 0;
    return audio->CrossfadeBgm(path, fade_sec, volume, loop != 0) ? 1 : 0;
}

extern "C" void dse_audio_play_sfx(const char* path, float volume, int loop) {
    auto* audio = GAS();
    if (!audio || !path) return;
    audio->PlaySfx(path, volume, loop != 0);
}

extern "C" void dse_audio_stop_all_sfx(void) {
    if (auto* audio = GAS()) audio->StopAllSfx();
}

extern "C" void dse_audio_fade_out_all_sfx(float duration_sec) {
    if (auto* audio = GAS()) audio->FadeOutAllSfx(duration_sec);
}

extern "C" int dse_audio_preload(const char* path) {
    auto* audio = GAS();
    if (!audio || !path) return 0;
    return audio->PreloadAudio(path) ? 1 : 0;
}

extern "C" void dse_audio_set_master_volume(float volume) {
    if (auto* audio = GAS()) audio->SetMasterVolume(volume);
}

extern "C" void dse_audio_set_bgm_volume(float volume) {
    if (auto* audio = GAS()) audio->SetBgmVolume(volume);
}

extern "C" void dse_audio_set_sfx_volume(float volume) {
    if (auto* audio = GAS()) audio->SetSfxVolume(volume);
}

// ============================================================
// Audio — ECS AudioSource / AudioListener
// ============================================================

extern "C" void dse_audio_source_add(uint32_t e, const char* path, int play_on_awake,
                                     int loop, float volume) {
    World* w = GW();
    AssetManager* am = GAM();
    if (!w || !w->registry().valid(TE(e)) || !path) return;
    auto& audio = w->registry().emplace_or_replace<AudioSourceComponent>(TE(e));
    if (am) audio.clip = am->LoadAudioClip(path);
    audio.play_on_awake = (play_on_awake != 0);
    audio.loop = (loop != 0);
    audio.volume = volume;
}

extern "C" void dse_audio_source_set_playing(uint32_t e, int playing) {
    auto* audio = GetComp<AudioSourceComponent>(e);
    if (!audio) return;
    audio->is_playing = (playing != 0);
    if (!audio->is_playing) audio->restart_requested = false;
}

extern "C" void dse_audio_source_restart(uint32_t e) {
    auto* audio = GetComp<AudioSourceComponent>(e);
    if (!audio) return;
    audio->is_playing = true;
    audio->restart_requested = true;
}

extern "C" void dse_audio_source_set_loop(uint32_t e, int loop) {
    if (auto* audio = GetComp<AudioSourceComponent>(e)) audio->loop = (loop != 0);
}

extern "C" void dse_audio_source_set_volume(uint32_t e, float volume) {
    if (auto* audio = GetComp<AudioSourceComponent>(e)) audio->volume = volume;
}

extern "C" void dse_audio_source_set_pitch(uint32_t e, float pitch) {
    if (auto* audio = GetComp<AudioSourceComponent>(e)) audio->pitch = std::max(0.01f, pitch);
}

extern "C" void dse_audio_source_set_3d_mode(uint32_t e, int enabled) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& audio = w->registry().get_or_emplace<AudioSourceComponent>(TE(e));
    audio.spatial_enabled = (enabled != 0);
}

extern "C" void dse_audio_source_set_3d_distance(uint32_t e, float min_distance,
                                                 float max_distance, float rolloff) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& audio = w->registry().get_or_emplace<AudioSourceComponent>(TE(e));
    audio.min_distance = std::max(0.01f, min_distance);
    audio.max_distance = std::max(audio.min_distance, max_distance);
    audio.rolloff = std::max(0.0f, rolloff);
}

extern "C" void dse_audio_source_set_bus(uint32_t e, const char* bus_name) {
    auto* audio = GetComp<AudioSourceComponent>(e);
    if (audio && bus_name) audio->bus_name = bus_name;
}

extern "C" int dse_audio_source_is_playing(uint32_t e) {
    auto* audio = GetComp<AudioSourceComponent>(e);
    return (audio && audio->is_playing) ? 1 : 0;
}

extern "C" void dse_audio_listener_add(uint32_t e, int enabled) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& listener = w->registry().emplace_or_replace<AudioListenerComponent>(TE(e));
    listener.enabled = (enabled != 0);
}

// ============================================================
// Audio — SFX 随机化 / 混音总线 / 快照 / 源状态
// ============================================================

extern "C" void dse_audio_play_sfx_random(const char* path, float volume,
                                          float pitch_min, float pitch_max) {
    auto* audio = GAS();
    if (!audio || !path) return;
    audio->PlaySfxRandomized(path, volume, pitch_min, pitch_max);
}

extern "C" int dse_audio_bus_set_volume(const char* name, float volume) {
    auto* audio = GAS();
    if (!audio || !name) return 0;
    audio->GetBusManager().SetBusVolume(name, volume);
    return 1;
}

extern "C" int dse_audio_bus_set_muted(const char* name, int muted) {
    auto* audio = GAS();
    if (!audio || !name) return 0;
    audio->GetBusManager().SetBusMuted(name, muted != 0);
    return 1;
}

extern "C" int dse_audio_bus_create(const char* name, const char* parent, float volume) {
    auto* audio = GAS();
    if (!audio || !name) return 0;
    return audio->GetBusManager().CreateBus(name, parent ? parent : "master", volume) ? 1 : 0;
}

extern "C" int dse_audio_bus_remove(const char* name) {
    auto* audio = GAS();
    if (!audio || !name) return 0;
    return audio->GetBusManager().RemoveBus(name) ? 1 : 0;
}

extern "C" int dse_audio_bus_add_effect(const char* bus_name, int type, float cutoff_hz, float q,
                                        float delay_time_ms, float feedback, float wet_mix,
                                        float room_size, float damping) {
    using gameplay2d::DspEffectType;
    using gameplay2d::DspEffectParams;
    auto* audio = GAS();
    if (!audio || !bus_name) return 0;
    DspEffectParams p;
    p.type = static_cast<DspEffectType>(
        std::clamp(type, 0, static_cast<int>(DspEffectType::Count) - 1));
    p.cutoff_hz = cutoff_hz;
    p.q = q;
    p.delay_time_ms = delay_time_ms;
    p.feedback = feedback;
    p.wet_mix = wet_mix;
    p.room_size = room_size;
    p.damping = damping;
    return audio->GetBusManager().AddEffect(bus_name, p) ? 1 : 0;
}

extern "C" int dse_audio_bus_remove_effect(const char* bus_name, int index) {
    auto* audio = GAS();
    if (!audio || !bus_name || index < 0) return 0;
    return audio->GetBusManager().RemoveEffect(bus_name, static_cast<size_t>(index)) ? 1 : 0;
}

extern "C" int dse_audio_bus_get_names(char* out, int cap) {
    auto* audio = GAS();
    if (!audio) return CopyStr(std::string(), out, cap);
    const auto names = audio->GetBusManager().GetBusNames();
    std::string joined;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) joined += '\n';
        joined += names[i];
    }
    return CopyStr(joined, out, cap);
}

extern "C" int dse_audio_snapshot_save(const char* name) {
    auto* audio = GAS();
    if (!audio || !name) return 0;
    return audio->GetBusManager().SaveSnapshot(name) ? 1 : 0;
}

extern "C" int dse_audio_snapshot_load(const char* name) {
    auto* audio = GAS();
    if (!audio || !name) return 0;
    return audio->GetBusManager().LoadSnapshot(name) ? 1 : 0;
}

extern "C" int dse_audio_snapshot_list(char* out, int cap) {
    auto* audio = GAS();
    if (!audio) return CopyStr(std::string(), out, cap);
    const auto names = audio->GetBusManager().GetSnapshotNames();
    std::string joined;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) joined += '\n';
        joined += names[i];
    }
    return CopyStr(joined, out, cap);
}

extern "C" int dse_audio_source_get_state(uint32_t e, int* out_flags, float* out_params,
                                          long long* out_runtime_handle, long long* out_clip_size,
                                          char* out_path, int path_cap) {
    const auto* audio = GetComp<AudioSourceComponent>(e);
    if (!audio) return 0;
    if (out_flags) {
        out_flags[0] = audio->clip ? 1 : 0;
        out_flags[1] = audio->is_playing ? 1 : 0;
        out_flags[2] = audio->spatial_enabled ? 1 : 0;
    }
    if (out_params) {
        out_params[0] = audio->min_distance;
        out_params[1] = audio->max_distance;
        out_params[2] = audio->rolloff;
        out_params[3] = audio->volume;
        out_params[4] = audio->pitch;
    }
    if (out_runtime_handle) *out_runtime_handle = static_cast<long long>(audio->runtime_handle);
    if (out_clip_size) {
        *out_clip_size = audio->clip ? static_cast<long long>(audio->clip->GetData().size()) : 0;
    }
    CopyStr(audio->clip ? audio->clip->GetPath() : std::string(), out_path, path_cap);
    return 1;
}

// 扁平化音源状态：12 个位置返回值供 Lua 直接解包。
extern "C" int dse_audio_source_get_state_ex(uint32_t e,
        int* out_clip_loaded, int* out_is_playing, int* out_spatial,
        float* out_min_dist, float* out_max_dist, float* out_rolloff,
        float* out_volume, float* out_pitch,
        double* out_runtime_handle, double* out_clip_bytes,
        char* out_path, int path_cap) {
    if (out_path && path_cap > 0) out_path[0] = '\0';
    const auto* audio = GetComp<AudioSourceComponent>(e);
    if (!audio) return 0;
    if (out_clip_loaded) *out_clip_loaded = audio->clip ? 1 : 0;
    if (out_is_playing) *out_is_playing = audio->is_playing ? 1 : 0;
    if (out_spatial) *out_spatial = audio->spatial_enabled ? 1 : 0;
    if (out_min_dist) *out_min_dist = audio->min_distance;
    if (out_max_dist) *out_max_dist = audio->max_distance;
    if (out_rolloff) *out_rolloff = audio->rolloff;
    if (out_volume) *out_volume = audio->volume;
    if (out_pitch) *out_pitch = audio->pitch;
    if (out_runtime_handle) *out_runtime_handle = static_cast<double>(audio->runtime_handle);
    if (out_clip_bytes) *out_clip_bytes = audio->clip ? static_cast<double>(audio->clip->GetData().size()) : 0.0;
    CopyStr(audio->clip ? audio->clip->GetPath() : std::string(), out_path, path_cap);
    return 1;
}

// ============================================================
// Navigation
// ============================================================

#ifdef DSE_ENABLE_NAVMESH

namespace {
navigation::NavMeshSystem* GetNav() {
    return core::ServiceLocator::Instance().Get<navigation::NavMeshSystem>();
}
}  // namespace

extern "C" int dse_nav_is_ready(void) {
    auto* nav = GetNav();
    return (nav && nav->IsReady()) ? 1 : 0;
}

extern "C" int dse_nav_load(const char* path) {
    auto* nav = GetNav();
    if (!nav || !path) return 0;
    return nav->LoadNavMesh(path) ? 1 : 0;
}

extern "C" int dse_nav_save(const char* path) {
    auto* nav = GetNav();
    if (!nav || !path) return 0;
    return nav->SaveNavMesh(path) ? 1 : 0;
}

extern "C" int dse_nav_find_nearest(float x, float y, float z, float* out_xyz) {
    auto* nav = GetNav();
    if (!nav || !nav->IsReady()) return 0;
    glm::vec3 nearest;
    if (!nav->FindNearestPoint(glm::vec3(x, y, z), nearest)) return 0;
    if (out_xyz) {
        out_xyz[0] = nearest.x;
        out_xyz[1] = nearest.y;
        out_xyz[2] = nearest.z;
    }
    return 1;
}

extern "C" int dse_nav_raycast(float sx, float sy, float sz,
                               float ex, float ey, float ez, float* out_hit_xyz) {
    auto* nav = GetNav();
    if (!nav || !nav->IsReady()) return 0;
    glm::vec3 hit;
    bool blocked = nav->Raycast(glm::vec3(sx, sy, sz), glm::vec3(ex, ey, ez), hit);
    if (out_hit_xyz) {
        out_hit_xyz[0] = hit.x;
        out_hit_xyz[1] = hit.y;
        out_hit_xyz[2] = hit.z;
    }
    return blocked ? 1 : 0;
}

extern "C" int dse_nav_find_path(float sx, float sy, float sz,
                                 float ex, float ey, float ez,
                                 float* out_xyz, int max_points) {
    auto* nav = GetNav();
    if (!nav || !nav->IsReady()) return 0;
    std::vector<glm::vec3> path;
    if (!nav->FindPath(glm::vec3(sx, sy, sz), glm::vec3(ex, ey, ez), path)) return 0;
    int total = static_cast<int>(path.size());
    if (out_xyz && max_points > 0) {
        int n = std::min(total, max_points);
        for (int i = 0; i < n; ++i) {
            out_xyz[i * 3 + 0] = path[static_cast<size_t>(i)].x;
            out_xyz[i * 3 + 1] = path[static_cast<size_t>(i)].y;
            out_xyz[i * 3 + 2] = path[static_cast<size_t>(i)].z;
        }
    }
    return total;
}

extern "C" void dse_nav_agent_set(uint32_t e, float speed, float acceleration,
                                  float stopping_dist, float radius, float height) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& agent = w->registry().get_or_emplace<NavMeshAgentComponent>(TE(e));
    if (!Keep(speed))         agent.speed         = speed;
    if (!Keep(acceleration))  agent.acceleration  = acceleration;
    if (!Keep(stopping_dist)) agent.stopping_dist = stopping_dist;
    if (!Keep(radius))        agent.agent_radius  = radius;
    if (!Keep(height))        agent.agent_height  = height;
}

extern "C" void dse_nav_agent_set_destination(uint32_t e, float x, float y, float z) {
    auto* agent = GetComp<NavMeshAgentComponent>(e);
    if (!agent) return;
    agent->destination = glm::vec3(x, y, z);
    agent->path_pending = true;
    agent->arrived = false;
}

extern "C" void dse_nav_agent_get_destination(uint32_t e, float* out_xyz) {
    if (!out_xyz) return;
    out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
    auto* agent = GetComp<NavMeshAgentComponent>(e);
    if (!agent) return;
    out_xyz[0] = agent->destination.x;
    out_xyz[1] = agent->destination.y;
    out_xyz[2] = agent->destination.z;
}

extern "C" int dse_nav_agent_has_path(uint32_t e) {
    auto* agent = GetComp<NavMeshAgentComponent>(e);
    return (agent && agent->has_path && !agent->path_points.empty()) ? 1 : 0;
}

extern "C" int dse_nav_agent_arrived(uint32_t e) {
    auto* agent = GetComp<NavMeshAgentComponent>(e);
    return (!agent || agent->arrived) ? 1 : 0;
}

extern "C" int dse_nav_bake(const float* verts, int nverts, const int* tris, int ntris,
                            float cell_size, float cell_height,
                            float agent_height, float agent_radius,
                            float agent_max_climb, float agent_max_slope) {
    auto* nav = GetNav();
    if (!nav || !verts || !tris || nverts <= 0 || ntris <= 0) return 0;
    navigation::NavMeshBuildConfig cfg{};
    if (!Keep(cell_size))       cfg.cell_size       = cell_size;
    if (!Keep(cell_height))     cfg.cell_height     = cell_height;
    if (!Keep(agent_height))    cfg.agent_height    = agent_height;
    if (!Keep(agent_radius))    cfg.agent_radius    = agent_radius;
    if (!Keep(agent_max_climb)) cfg.agent_max_climb = agent_max_climb;
    if (!Keep(agent_max_slope)) cfg.agent_max_slope = agent_max_slope;
    return nav->BakeFromTriangles(verts, nverts, tris, ntris, cfg) ? 1 : 0;
}

extern "C" int dse_nav_agent_get(uint32_t e, float* out_params, int* out_flags) {
    auto* agent = GetComp<NavMeshAgentComponent>(e);
    if (!agent) return 0;
    if (out_params) {
        out_params[0] = agent->speed;
        out_params[1] = agent->acceleration;
        out_params[2] = agent->stopping_dist;
        out_params[3] = agent->agent_radius;
        out_params[4] = agent->agent_height;
        out_params[5] = agent->destination.x;
        out_params[6] = agent->destination.y;
        out_params[7] = agent->destination.z;
    }
    if (out_flags) {
        out_flags[0] = agent->has_path ? 1 : 0;
        out_flags[1] = agent->path_pending ? 1 : 0;
        out_flags[2] = agent->arrived ? 1 : 0;
        out_flags[3] = agent->current_waypoint;
    }
    return 1;
}

#else  // !DSE_ENABLE_NAVMESH — 安全空实现

extern "C" int dse_nav_is_ready(void) { return 0; }
extern "C" int dse_nav_load(const char*) { return 0; }
extern "C" int dse_nav_save(const char*) { return 0; }
extern "C" int dse_nav_find_nearest(float, float, float, float*) { return 0; }
extern "C" int dse_nav_raycast(float, float, float, float, float, float, float*) { return 0; }
extern "C" int dse_nav_find_path(float, float, float, float, float, float, float*, int) { return 0; }
extern "C" void dse_nav_agent_set(uint32_t, float, float, float, float, float) {}
extern "C" void dse_nav_agent_set_destination(uint32_t, float, float, float) {}
extern "C" void dse_nav_agent_get_destination(uint32_t, float* out_xyz) {
    if (out_xyz) out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
}
extern "C" int dse_nav_agent_has_path(uint32_t) { return 0; }
extern "C" int dse_nav_agent_arrived(uint32_t) { return 1; }
extern "C" int dse_nav_bake(const float*, int, const int*, int,
                            float, float, float, float, float, float) { return 0; }
extern "C" int dse_nav_agent_get(uint32_t, float*, int*) { return 0; }

#endif  // DSE_ENABLE_NAVMESH

// ============================================================
// Localization
// ============================================================

namespace {
assets::LocalizationManager* GetL10n() {
    return core::ServiceLocator::Instance().Get<assets::LocalizationManager>();
}
}  // namespace

extern "C" int dse_l10n_load(const char* path, const char* locale) {
    auto* l10n = GetL10n();
    if (!l10n || !path || !locale) return 0;
    return l10n->LoadLocale(path, locale) ? 1 : 0;
}

extern "C" void dse_l10n_set_locale(const char* locale) {
    auto* l10n = GetL10n();
    if (l10n && locale) l10n->SetCurrentLocale(locale);
}

extern "C" int dse_l10n_get_locale(char* out, int cap) {
    auto* l10n = GetL10n();
    if (!l10n) return CopyStr(std::string(), out, cap);
    return CopyStr(l10n->GetCurrentLocale(), out, cap);
}

extern "C" int dse_l10n_get(const char* key, char* out, int cap) {
    auto* l10n = GetL10n();
    if (!l10n || !key) return CopyStr(std::string(), out, cap);
    return CopyStr(l10n->Get(key), out, cap);
}

extern "C" int dse_l10n_has_key(const char* key) {
    auto* l10n = GetL10n();
    return (l10n && key && l10n->HasKey(key)) ? 1 : 0;
}

// ============================================================
// Scene / Prefab
// ============================================================

extern "C" int dse_scene_load(const char* path) {
    World* w = GW();
    if (!w || !path) return 0;
    dse::scene::Scene loader("native_api_scene_loader");
    loader.BindWorld(w);
    const bool ok = loader.Deserialize(path);
    loader.UnbindWorld();
    return ok ? 1 : 0;
}

extern "C" int dse_scene_save(const char* path) {
    World* w = GW();
    if (!w || !path) return 0;
    dse::scene::Scene saver("native_api_scene_saver");
    saver.BindWorld(w);
    const bool ok = saver.Serialize(path);
    saver.UnbindWorld();
    return ok ? 1 : 0;
}

extern "C" int dse_scene_save_prefab(uint32_t e, const char* path) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e)) || !path) return 0;
    return dse::scene::SaveEntityAsPrefab(*w, TE(e), path) ? 1 : 0;
}

extern "C" uint32_t dse_scene_instantiate_prefab(const char* path, float x, float y, float z,
                                                 int use_pos) {
    World* w = GW();
    if (!w || !path) return static_cast<uint32_t>(entt::null);
    Entity e;
    if (use_pos) {
        dse::scene::PrefabInstantiateOptions opts;
        opts.override_position = true;
        opts.position = glm::vec3(x, y, z);
        e = dse::scene::InstantiatePrefab(*w, path, opts);
    } else {
        e = dse::scene::InstantiatePrefab(*w, path);
    }
    return static_cast<uint32_t>(static_cast<entt::id_type>(e));
}

// ============================================================
// UI — 核心控件
// ============================================================

extern "C" void dse_ui_add_renderer(uint32_t e, uint32_t texture_handle,
                                    float r, float g, float b, float a,
                                    int order, float w, float h) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& ui = world->registry().emplace_or_replace<UIRendererComponent>(TE(e));
    ui.texture_handle = dse::render::TextureHandle::from_raw(texture_handle);
    ui.color = glm::vec4(r, g, b, a);
    ui.order = order;
    ui.size = glm::vec2(w, h);
}

extern "C" void dse_ui_add_panel(uint32_t e, int blocks_input) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& panel = world->registry().emplace_or_replace<UIPanelComponent>(TE(e));
    panel.blocks_input = (blocks_input != 0);
}

extern "C" void dse_ui_add_button(uint32_t e, float r, float g, float b, float a) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& button = world->registry().emplace_or_replace<UIButtonComponent>(TE(e));
    button.normal_color = glm::vec4(r, g, b, a);
    button.hover_color = button.normal_color * glm::vec4(1.1f, 1.1f, 1.1f, 1.0f);
    button.pressed_color = button.normal_color * glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
}

extern "C" void dse_ui_add_ttf_label(uint32_t e, const char* text, const char* font_id,
                                     float font_size, float r, float g, float b, float a) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e)) || !text || !font_id) return;
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
    auto& label = world->registry().emplace_or_replace<UILabelComponent>(TE(e));
    label.text = text;
    label.font_id = font_id;
    label.font_size = font_size;
    label.use_sdf = true;
    label.color = glm::vec4(r, g, b, a);
    label.dirty = true;
}

extern "C" void dse_ui_set_label_text(uint32_t e, const char* text) {
    auto* label = GetComp<UILabelComponent>(e);
    if (!label || !text) return;
    label->numeric_mode = false;
    label->text = text;
    label->dirty = true;
}

extern "C" void dse_ui_set_label_font(uint32_t e, const char* font_id, float font_size) {
    auto* label = GetComp<UILabelComponent>(e);
    if (!label || !font_id) return;
    label->font_id = font_id;
    if (font_size > 0.0f) label->font_size = font_size;
    label->dirty = true;
}

extern "C" void dse_ui_set_position(uint32_t e, float x, float y) {
    if (auto* ui = GetComp<UIRendererComponent>(e)) ui->position = glm::vec2(x, y);
}

extern "C" void dse_ui_set_size(uint32_t e, float w, float h) {
    if (auto* ui = GetComp<UIRendererComponent>(e)) ui->size = glm::vec2(w, h);
}

extern "C" void dse_ui_set_anchor(uint32_t e, float ax, float ay) {
    auto* ui = GetComp<UIRendererComponent>(e);
    if (!ui) return;
    ui->anchor_min = glm::vec2(ax, ay);
    ui->anchor_max = glm::vec2(ax, ay);
}

extern "C" void dse_ui_set_color(uint32_t e, float r, float g, float b, float a) {
    if (auto* ui = GetComp<UIRendererComponent>(e)) ui->color = glm::vec4(r, g, b, a);
}

extern "C" void dse_ui_set_visible(uint32_t e, int visible) {
    if (auto* ui = GetComp<UIRendererComponent>(e)) ui->visible = (visible != 0);
}

extern "C" int dse_ui_is_hovered(uint32_t e) {
    auto* ui = GetComp<UIRendererComponent>(e);
    return (ui && ui->is_hovered) ? 1 : 0;
}

extern "C" int dse_ui_is_pressed(uint32_t e) {
    auto* ui = GetComp<UIRendererComponent>(e);
    return (ui && ui->is_pressed) ? 1 : 0;
}

extern "C" void dse_ui_add_joystick(uint32_t e, float max_radius, int follow_pointer,
                                    int reset_on_release) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& joystick = world->registry().emplace_or_replace<UIJoystickComponent>(TE(e));
    joystick.max_radius = max_radius;
    joystick.follow_pointer = (follow_pointer != 0);
    joystick.reset_on_release = (reset_on_release != 0);
    joystick.direction = glm::vec2(0.0f);
    joystick.is_dragging = false;
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
}

extern "C" float dse_ui_get_joystick_x(uint32_t e) {
    auto* joystick = GetComp<UIJoystickComponent>(e);
    return joystick ? joystick->direction.x : 0.0f;
}

extern "C" float dse_ui_get_joystick_y(uint32_t e) {
    auto* joystick = GetComp<UIJoystickComponent>(e);
    return joystick ? joystick->direction.y : 0.0f;
}

extern "C" void dse_ui_add_slider(uint32_t e, float min_value, float max_value,
                                  float value, int whole_numbers) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& slider = world->registry().emplace_or_replace<UISliderComponent>(TE(e));
    slider.min_value = min_value;
    slider.max_value = max_value;
    slider.value = value;
    slider.whole_numbers = (whole_numbers != 0);
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
}

extern "C" void dse_ui_set_slider_value(uint32_t e, float value) {
    if (auto* slider = GetComp<UISliderComponent>(e)) slider->value = value;
}

extern "C" float dse_ui_get_slider_value(uint32_t e) {
    auto* slider = GetComp<UISliderComponent>(e);
    return slider ? slider->value : 0.0f;
}

extern "C" void dse_ui_add_toggle(uint32_t e, int is_on, int group) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& toggle = world->registry().emplace_or_replace<UIToggleComponent>(TE(e));
    toggle.is_on = (is_on != 0);
    toggle.group = group;
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
}

extern "C" void dse_ui_set_toggle(uint32_t e, int is_on) {
    if (auto* toggle = GetComp<UIToggleComponent>(e)) toggle->is_on = (is_on != 0);
}

extern "C" int dse_ui_get_toggle(uint32_t e) {
    auto* toggle = GetComp<UIToggleComponent>(e);
    return (toggle && toggle->is_on) ? 1 : 0;
}

extern "C" void dse_ui_add_progress_bar(uint32_t e, float value, float max_value) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& bar = world->registry().emplace_or_replace<UIProgressBarComponent>(TE(e));
    bar.value = value;
    bar.max_value = max_value;
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
}

extern "C" void dse_ui_set_progress(uint32_t e, float value) {
    if (auto* bar = GetComp<UIProgressBarComponent>(e)) bar->value = value;
}

extern "C" float dse_ui_get_progress(uint32_t e) {
    auto* bar = GetComp<UIProgressBarComponent>(e);
    return bar ? bar->value : 0.0f;
}

extern "C" void dse_ui_add_text_input(uint32_t e, const char* placeholder,
                                      int max_length, int is_password) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& input = world->registry().emplace_or_replace<UITextInputComponent>(TE(e));
    input.placeholder = placeholder ? placeholder : "";
    input.max_length = max_length;
    input.is_password = (is_password != 0);
    if (!world->registry().all_of<UIRendererComponent>(TE(e))) {
        world->registry().emplace<UIRendererComponent>(TE(e));
    }
}

extern "C" void dse_ui_set_text_input_text(uint32_t e, const char* text) {
    auto* input = GetComp<UITextInputComponent>(e);
    if (!input || !text) return;
    input->text = text;
    input->cursor_position = static_cast<int>(input->text.size());
}

extern "C" int dse_ui_get_text_input_text(uint32_t e, char* out, int cap) {
    auto* input = GetComp<UITextInputComponent>(e);
    if (!input) return CopyStr(std::string(), out, cap);
    return CopyStr(input->text, out, cap);
}

extern "C" void dse_ui_set_text_input_focus(uint32_t e, int focused) {
    if (auto* input = GetComp<UITextInputComponent>(e)) input->is_focused = (focused != 0);
}

namespace {
int CopyEntities(const std::vector<entt::entity>& entities, uint32_t* out_entities, int cap) {
    int total = static_cast<int>(entities.size());
    if (out_entities && cap > 0) {
        int n = std::min(total, cap);
        for (int i = 0; i < n; ++i) {
            out_entities[i] = static_cast<uint32_t>(
                static_cast<entt::id_type>(entities[static_cast<size_t>(i)]));
        }
    }
    return total;
}
}  // namespace

extern "C" int dse_ui_load_from_file(const char* path, uint32_t* out_entities, int cap) {
    World* w = GW();
    if (!w || !path) return 0;
    UISerializer serializer;
    return CopyEntities(serializer.LoadFromFile(w->registry(), path), out_entities, cap);
}

extern "C" int dse_ui_load_from_json(const char* json, uint32_t* out_entities, int cap) {
    World* w = GW();
    if (!w || !json) return 0;
    UISerializer serializer;
    return CopyEntities(serializer.LoadFromJson(w->registry(), json), out_entities, cap);
}

// ============================================================
// Localization — 补充实现
// ============================================================

extern "C" int dse_l10n_load_string(const char* json, const char* locale) {
    auto* l10n = GetL10n();
    if (!l10n || !json || !locale) return 0;
    return l10n->LoadLocaleFromString(json, locale) ? 1 : 0;
}

extern "C" int dse_l10n_get_locales(char* out, int cap) {
    auto* l10n = GetL10n();
    if (!l10n || !out || cap <= 0) return 0;
    auto locales = l10n->GetAvailableLocales();
    std::string packed;
    for (size_t i = 0; i < locales.size(); ++i) {
        if (i > 0) packed += '\0';
        packed += locales[i];
    }
    int n = static_cast<int>(packed.size());
    if (n > cap - 1) n = cap - 1;
    std::memcpy(out, packed.data(), static_cast<size_t>(n));
    out[n] = '\0';
    return static_cast<int>(locales.size());
}

// Lua 绑定薄包装：参数顺序/返回类型适配（Lua 侧 load(locale,json)/get_locale()->string/get(key,fallback)->string）
extern "C" int dse_compat_l10n_load(const char* locale, const char* json) {
    return dse_l10n_load_string(json, locale);
}

extern "C" const char* dse_compat_l10n_get_locale() {
    static thread_local char buf[128];
    buf[0] = '\0';
    dse_l10n_get_locale(buf, static_cast<int>(sizeof(buf)));
    return buf;
}

extern "C" const char* dse_compat_l10n_get(const char* key, const char* fallback) {
    static thread_local char buf[1024];
    buf[0] = '\0';
    dse_l10n_get(key, buf, static_cast<int>(sizeof(buf)));
    if (buf[0] == '\0') return fallback ? fallback : "";
    return buf;
}

// Lua 绑定薄包装：get_text_input_text 返回字符串（缓冲区版适配）
extern "C" const char* dse_compat_ui_get_text_input_text(uint32_t e) {
    static thread_local char buf[1024];
    buf[0] = '\0';
    dse_ui_get_text_input_text(e, buf, static_cast<int>(sizeof(buf)));
    return buf;
}

// ============================================================
// Font — 字体服务
// ============================================================

namespace {
render::FontService* GetFontService() {
    return core::ServiceLocator::Instance().Get<render::FontService>();
}
}  // namespace

extern "C" int dse_font_load(const char* font_id, const char* ttf_path) {
    auto* svc = GetFontService();
    if (!svc || !font_id || !ttf_path) return 0;
    return svc->LoadFont(font_id, ttf_path) ? 1 : 0;
}

extern "C" int dse_font_load_cjk(const char* font_id, const char* ttf_path) {
    auto* svc = GetFontService();
    if (!svc || !font_id || !ttf_path) return 0;
    // 常用码位集合：ASCII + CJK 标点 + 仓库实际用字（脚本扫描 templates/docs/samples 生成）
    // 说明：原实现取 U+4E00 起连续 800 个码位，常用字大量缺失（如"你/我/的/剑/天"），
    // 这里改为"项目实际使用字 + 标点 + ASCII"，保证任意项目文本都能上屏。
    static const unsigned int kProjectCjkCodepoints[] = {
    0x4E00, 0x4E01, 0x4E03, 0x4E07, 0x4E09, 0x4E0A, 0x4E0B, 0x4E0D, 0x4E0E, 0x4E11, 0x4E13, 0x4E14,
    0x4E16, 0x4E18, 0x4E1A, 0x4E1B, 0x4E1C, 0x4E1D, 0x4E22, 0x4E24, 0x4E25, 0x4E2A, 0x4E2D, 0x4E30,
    0x4E32, 0x4E34, 0x4E3A, 0x4E3B, 0x4E3D, 0x4E3E, 0x4E45, 0x4E48, 0x4E49, 0x4E4B, 0x4E4E, 0x4E4F,
    0x4E50, 0x4E58, 0x4E5D, 0x4E5F, 0x4E60, 0x4E66, 0x4E70, 0x4E71, 0x4E73, 0x4E86, 0x4E88, 0x4E89,
    0x4E8B, 0x4E8C, 0x4E8E, 0x4E8F, 0x4E91, 0x4E92, 0x4E94, 0x4E95, 0x4E9B, 0x4EA1, 0x4EA4, 0x4EA6,
    0x4EA7, 0x4EAB, 0x4EAE, 0x4EB2, 0x4EBA, 0x4EBF, 0x4EC0, 0x4EC5, 0x4ECA, 0x4ECB, 0x4ECD, 0x4ECE,
    0x4ED3, 0x4ED6, 0x4ED8, 0x4EE3, 0x4EE4, 0x4EE5, 0x4EEA, 0x4EEC, 0x4EF0, 0x4EF6, 0x4EF7, 0x4EFB,
    0x4EFD, 0x4EFF, 0x4F01, 0x4F0D, 0x4F0F, 0x4F11, 0x4F17, 0x4F18, 0x4F19, 0x4F1A, 0x4F20, 0x4F24,
    0x4F26, 0x4F2A, 0x4F2F, 0x4F30, 0x4F34, 0x4F38, 0x4F3C, 0x4F46, 0x4F4D, 0x4F4E, 0x4F4F, 0x4F50,
    0x4F53, 0x4F55, 0x4F59, 0x4F5C, 0x4F60, 0x4F63, 0x4F6C, 0x4F73, 0x4F7F, 0x4F88, 0x4F8B, 0x4F9B,
    0x4F9D, 0x4FA0, 0x4FA7, 0x4FAF, 0x4FB5, 0x4FBF, 0x4FC4, 0x4FD7, 0x4FDD, 0x4FE1, 0x4FEE, 0x4FEF,
    0x500D, 0x5012, 0x5019, 0x501F, 0x503A, 0x503C, 0x503E, 0x5047, 0x504F, 0x505A, 0x505C, 0x5065,
    0x5076, 0x5077, 0x507F, 0x5085, 0x5095, 0x50A8, 0x50CF, 0x50F5, 0x5112, 0x513F, 0x5141, 0x5143,
    0x5144, 0x5145, 0x5146, 0x5148, 0x5149, 0x514B, 0x514D, 0x5151, 0x515C, 0x5165, 0x5168, 0x516B,
    0x516C, 0x516D, 0x5170, 0x5171, 0x5173, 0x5174, 0x5175, 0x5176, 0x5177, 0x5178, 0x517B, 0x517C,
    0x517D, 0x5185, 0x518C, 0x518D, 0x5192, 0x5197, 0x5199, 0x519B, 0x519C, 0x51A0, 0x51AC, 0x51B0,
    0x51B2, 0x51B3, 0x51B5, 0x51B7, 0x51BB, 0x51C0, 0x51C6, 0x51C9, 0x51CC, 0x51CF, 0x51D1, 0x51DD,
    0x51E0, 0x51E1, 0x51ED, 0x51F6, 0x51F8, 0x51F9, 0x51FA, 0x51FB, 0x51FD, 0x5200, 0x5203, 0x5206,
    0x5207, 0x5212, 0x5217, 0x5218, 0x5219, 0x521A, 0x521B, 0x521D, 0x5220, 0x5224, 0x5229, 0x522B,
    0x5230, 0x5236, 0x5237, 0x5239, 0x523A, 0x523B, 0x524A, 0x524D, 0x5251, 0x5254, 0x5256, 0x5265,
    0x5267, 0x5269, 0x526A, 0x526F, 0x5272, 0x5288, 0x529B, 0x529D, 0x529E, 0x529F, 0x52A0, 0x52A1,
    0x52A3, 0x52A8, 0x52A9, 0x52AA, 0x52AB, 0x52B1, 0x52B2, 0x52B3, 0x52BF, 0x52C7, 0x52C9, 0x52D2,
    0x52D8, 0x52FA, 0x52FE, 0x52FF, 0x5300, 0x5305, 0x5316, 0x5317, 0x5319, 0x5320, 0x5339, 0x533A,
    0x533F, 0x5341, 0x5343, 0x5347, 0x5348, 0x534A, 0x534E, 0x534F, 0x5353, 0x5355, 0x5356, 0x5357,
    0x535A, 0x5360, 0x5361, 0x5366, 0x536B, 0x5370, 0x5371, 0x5373, 0x5374, 0x5377, 0x5378, 0x5382,
    0x5385, 0x5386, 0x5389, 0x538B, 0x5395, 0x5398, 0x539A, 0x539F, 0x53A8, 0x53BB, 0x53C2, 0x53C8,
    0x53C9, 0x53CA, 0x53CB, 0x53CC, 0x53CD, 0x53D1, 0x53D6, 0x53D7, 0x53D8, 0x53D9, 0x53E0, 0x53E3,
    0x53E4, 0x53E5, 0x53E6, 0x53EA, 0x53EB, 0x53EC, 0x53ED, 0x53EF, 0x53F0, 0x53F2, 0x53F3, 0x53F6,
    0x53F7, 0x53F8, 0x53F9, 0x5403, 0x5404, 0x5408, 0x5409, 0x540C, 0x540D, 0x540E, 0x5410, 0x5411,
    0x5413, 0x5415, 0x5417, 0x541E, 0x5426, 0x5427, 0x542B, 0x542C, 0x542F, 0x5438, 0x5439, 0x5440,
    0x5446, 0x5448, 0x544A, 0x5458, 0x545C, 0x5462, 0x5468, 0x5473, 0x5475, 0x547C, 0x547D, 0x548C,
    0x5492, 0x5494, 0x54A8, 0x54B1, 0x54B3, 0x54C1, 0x54C7, 0x54C8, 0x54CD, 0x54CE, 0x54D1, 0x54E5,
    0x54E6, 0x54E8, 0x54EA, 0x54ED, 0x54F2, 0x54FA, 0x54FC, 0x5524, 0x552F, 0x5531, 0x5543, 0x5546,
    0x554A, 0x5565, 0x5566, 0x556A, 0x5582, 0x5584, 0x5587, 0x558A, 0x559C, 0x559D, 0x55B5, 0x55B7,
    0x55BB, 0x55C5, 0x55D3, 0x55E1, 0x55EF, 0x5609, 0x561B, 0x5634, 0x5636, 0x5668, 0x5669, 0x566A,
    0x566C, 0x567C, 0x5693, 0x56A3, 0x56CA, 0x56DB, 0x56DE, 0x56E0, 0x56E2, 0x56ED, 0x56F0, 0x56F4,
    0x56FA, 0x56FD, 0x56FE, 0x5706, 0x5708, 0x571F, 0x5723, 0x5728, 0x5730, 0x573A, 0x573E, 0x5740,
    0x5747, 0x574F, 0x5750, 0x5751, 0x5757, 0x575A, 0x575B, 0x5760, 0x5761, 0x5766, 0x5768, 0x5782,
    0x5783, 0x5784, 0x578B, 0x5792, 0x57AB, 0x57C3, 0x57CB, 0x57CE, 0x57DF, 0x57F9, 0x57FA, 0x5802,
    0x5806, 0x5821, 0x5835, 0x584C, 0x5851, 0x5854, 0x585E, 0x586B, 0x5883, 0x5893, 0x5899, 0x589E,
    0x58C1, 0x58E4, 0x58EB, 0x58EE, 0x58F0, 0x58F3, 0x58F6, 0x5904, 0x5907, 0x590D, 0x590F, 0x5915,
    0x5916, 0x591A, 0x591C, 0x591F, 0x5927, 0x5929, 0x592A, 0x592B, 0x592E, 0x5931, 0x5934, 0x5939,
    0x593A, 0x5942, 0x5947, 0x594F, 0x5951, 0x5954, 0x5956, 0x5957, 0x5962, 0x5965, 0x5973, 0x5979,
    0x597D, 0x5982, 0x5986, 0x5988, 0x5999, 0x59A5, 0x59B9, 0x59BB, 0x59C6, 0x59CB, 0x59D0, 0x59D1,
    0x59D4, 0x59FF, 0x5A01, 0x5A03, 0x5A31, 0x5A92, 0x5B50, 0x5B54, 0x5B57, 0x5B58, 0x5B59, 0x5B63,
    0x5B64, 0x5B66, 0x5B69, 0x5B6A, 0x5B83, 0x5B87, 0x5B88, 0x5B89, 0x5B8C, 0x5B8F, 0x5B97, 0x5B98,
    0x5B99, 0x5B9A, 0x5B9C, 0x5B9D, 0x5B9E, 0x5BA0, 0x5BA1, 0x5BA2, 0x5BA3, 0x5BA4, 0x5BAB, 0x5BB3,
    0x5BB6, 0x5BB9, 0x5BBD, 0x5BBF, 0x5BC4, 0x5BC6, 0x5BCC, 0x5BD2, 0x5BDF, 0x5BE8, 0x5BF8, 0x5BF9,
    0x5BFB, 0x5BFC, 0x5BFF, 0x5C01, 0x5C04, 0x5C06, 0x5C0F, 0x5C11, 0x5C14, 0x5C16, 0x5C18, 0x5C1A,
    0x5C1D, 0x5C24, 0x5C31, 0x5C38, 0x5C3A, 0x5C3C, 0x5C3D, 0x5C3E, 0x5C40, 0x5C42, 0x5C45, 0x5C48,
    0x5C49, 0x5C4A, 0x5C4B, 0x5C4F, 0x5C51, 0x5C55, 0x5C5E, 0x5C71, 0x5C94, 0x5C96, 0x5C9B, 0x5CA9,
    0x5CAD, 0x5CB8, 0x5CED, 0x5CF0, 0x5D0E, 0x5D16, 0x5D1B, 0x5D29, 0x5D4C, 0x5DE1, 0x5DE2, 0x5DE5,
    0x5DE6, 0x5DE7, 0x5DE8, 0x5DEB, 0x5DEE, 0x5DF1, 0x5DF2, 0x5DF4, 0x5DFE, 0x5E01, 0x5E02, 0x5E03,
    0x5E05, 0x5E08, 0x5E0C, 0x5E10, 0x5E16, 0x5E18, 0x5E1C, 0x5E26, 0x5E27, 0x5E2D, 0x5E2E, 0x5E38,
    0x5E3D, 0x5E42, 0x5E45, 0x5E55, 0x5E72, 0x5E73, 0x5E74, 0x5E76, 0x5E78, 0x5E7B, 0x5E7C, 0x5E7D,
    0x5E7F, 0x5E84, 0x5E8A, 0x5E8F, 0x5E93, 0x5E94, 0x5E95, 0x5E97, 0x5E9E, 0x5E9F, 0x5EA6, 0x5EA7,
    0x5EB7, 0x5ECA, 0x5ED3, 0x5EF6, 0x5EFA, 0x5F00, 0x5F02, 0x5F03, 0x5F04, 0x5F0A, 0x5F0F, 0x5F13,
    0x5F15, 0x5F1F, 0x5F20, 0x5F25, 0x5F26, 0x5F27, 0x5F2F, 0x5F31, 0x5F39, 0x5F3A, 0x5F52, 0x5F53,
    0x5F55, 0x5F62, 0x5F69, 0x5F6A, 0x5F71, 0x5F7B, 0x5F7C, 0x5F80, 0x5F81, 0x5F84, 0x5F85, 0x5F88,
    0x5F8B, 0x5F90, 0x5F97, 0x5FA1, 0x5FAA, 0x5FAE, 0x5FB7, 0x5FC3, 0x5FC5, 0x5FC6, 0x5FCD, 0x5FD7,
    0x5FD8, 0x5FD9, 0x5FE0, 0x5FE7, 0x5FEB, 0x5FF5, 0x5FFD, 0x6000, 0x6001, 0x600E, 0x6012, 0x6015,
    0x6016, 0x601C, 0x601D, 0x6025, 0x6027, 0x6028, 0x602A, 0x603B, 0x6050, 0x6052, 0x6062, 0x606D,
    0x606F, 0x6070, 0x6073, 0x6076, 0x6084, 0x6089, 0x609F, 0x60A3, 0x60AC, 0x60C5, 0x60CA, 0x60D5,
    0x60DC, 0x60EF, 0x60F0, 0x60F3, 0x60F6, 0x6108, 0x610F, 0x611F, 0x6124, 0x613F, 0x6148, 0x614C,
    0x614E, 0x6155, 0x6162, 0x6199, 0x61C2, 0x61D2, 0x61F5, 0x61FF, 0x620F, 0x6210, 0x6211, 0x6212,
    0x6216, 0x6218, 0x622A, 0x6233, 0x6234, 0x6237, 0x623F, 0x6240, 0x6241, 0x6247, 0x624B, 0x624D,
    0x624E, 0x6251, 0x6253, 0x6254, 0x6258, 0x625B, 0x6263, 0x6267, 0x6269, 0x626B, 0x626D, 0x626E,
    0x6270, 0x6273, 0x6276, 0x6279, 0x627E, 0x627F, 0x6280, 0x6284, 0x628A, 0x6291, 0x6293, 0x6295,
    0x6296, 0x6297, 0x6298, 0x629B, 0x62A2, 0x62A4, 0x62A5, 0x62AB, 0x62AC, 0x62B1, 0x62B5, 0x62B9,
    0x62BC, 0x62BD, 0x62C2, 0x62C5, 0x62C6, 0x62C7, 0x62C9, 0x62CC, 0x62CD, 0x62D0, 0x62D2, 0x62D3,
    0x62D4, 0x62D6, 0x62DB, 0x62DC, 0x62DF, 0x62E3, 0x62E5, 0x62E6, 0x62E7, 0x62E8, 0x62E9, 0x62EC,
    0x62F3, 0x62F7, 0x62FC, 0x62FD, 0x62FE, 0x62FF, 0x6301, 0x6302, 0x6307, 0x6309, 0x6311, 0x6321,
    0x6324, 0x6325, 0x632A, 0x632F, 0x633A, 0x6346, 0x634F, 0x6355, 0x635F, 0x6361, 0x6362, 0x636E,
    0x6377, 0x6388, 0x6389, 0x638C, 0x6392, 0x6398, 0x63A2, 0x63A5, 0x63A7, 0x63A8, 0x63A9, 0x63AA,
    0x63B3, 0x63B7, 0x63BA, 0x63CF, 0x63D0, 0x63D2, 0x63E1, 0x63F4, 0x6401, 0x6405, 0x6410, 0x6413,
    0x641C, 0x641E, 0x642C, 0x642D, 0x643A, 0x6444, 0x6446, 0x6447, 0x644A, 0x6454, 0x6458, 0x6467,
    0x6469, 0x6478, 0x6491, 0x6492, 0x6495, 0x649E, 0x64A4, 0x64A9, 0x64AD, 0x64B0, 0x64B8, 0x64C5,
    0x64CD, 0x64CE, 0x64E6, 0x6500, 0x652F, 0x6536, 0x6539, 0x653B, 0x653E, 0x653F, 0x6545, 0x6548,
    0x654C, 0x654F, 0x6551, 0x6559, 0x655B, 0x6562, 0x6563, 0x6570, 0x6572, 0x6574, 0x6587, 0x6591,
    0x6597, 0x6599, 0x659C, 0x65A5, 0x65A9, 0x65AD, 0x65AF, 0x65B0, 0x65B9, 0x65BD, 0x65C1, 0x65C5,
    0x65CB, 0x65CF, 0x65D7, 0x65E0, 0x65E2, 0x65E5, 0x65E6, 0x65E7, 0x65E9, 0x65F6, 0x6602, 0x6606,
    0x660E, 0x660F, 0x6613, 0x661F, 0x6620, 0x662F, 0x663C, 0x663E, 0x6643, 0x6652, 0x6655, 0x665A,
    0x6668, 0x666E, 0x666F, 0x6670, 0x6674, 0x6676, 0x667A, 0x6682, 0x6696, 0x6697, 0x66AE, 0x66B4,
    0x66D9, 0x66DD, 0x66F2, 0x66F4, 0x66F9, 0x66FE, 0x66FF, 0x6700, 0x6708, 0x6709, 0x670B, 0x670D,
    0x6717, 0x671B, 0x671D, 0x671F, 0x6728, 0x672A, 0x672B, 0x672C, 0x672F, 0x6734, 0x6735, 0x673A,
    0x6740, 0x6742, 0x6743, 0x6746, 0x674E, 0x6750, 0x6751, 0x6756, 0x675C, 0x675F, 0x6760, 0x6761,
    0x6765, 0x6768, 0x676F, 0x677E, 0x677F, 0x6781, 0x6784, 0x6790, 0x6797, 0x679A, 0x679C, 0x679D,
    0x67A2, 0x67AA, 0x67AF, 0x67B6, 0x67C4, 0x67D0, 0x67D3, 0x67D4, 0x67DC, 0x67E5, 0x67F1, 0x67F3,
    0x6805, 0x6807, 0x6808, 0x680B, 0x680F, 0x6811, 0x6817, 0x6821, 0x6837, 0x6838, 0x6839, 0x683C,
    0x6846, 0x6848, 0x684C, 0x6863, 0x6865, 0x6868, 0x6869, 0x6876, 0x6881, 0x6897, 0x68A6, 0x68AF,
    0x68B0, 0x68B3, 0x68C0, 0x68CB, 0x68D2, 0x68D5, 0x68EE, 0x68F1, 0x68F5, 0x6905, 0x690D, 0x692D,
    0x695A, 0x697C, 0x6982, 0x699C, 0x69A8, 0x69DB, 0x69FD, 0x6A0A, 0x6A21, 0x6A2A, 0x6A59, 0x6A61,
    0x6A90, 0x6B20, 0x6B21, 0x6B22, 0x6B27, 0x6B32, 0x6B3E, 0x6B47, 0x6B4C, 0x6B62, 0x6B63, 0x6B64,
    0x6B65, 0x6B66, 0x6B67, 0x6B6A, 0x6B7B, 0x6B8A, 0x6B8B, 0x6B96, 0x6BB5, 0x6BC1, 0x6BCD, 0x6BCF,
    0x6BD2, 0x6BD4, 0x6BD5, 0x6BDB, 0x6BEB, 0x6C0F, 0x6C11, 0x6C14, 0x6C1B, 0x6C34, 0x6C38, 0x6C41,
    0x6C42, 0x6C47, 0x6C49, 0x6C5C, 0x6C60, 0x6C61, 0x6C64, 0x6C70, 0x6C7D, 0x6C89, 0x6C99, 0x6C9F,
    0x6CA1, 0x6CAB, 0x6CB3, 0x6CB9, 0x6CBB, 0x6CBF, 0x6CC4, 0x6CC9, 0x6CD5, 0x6CDB, 0x6CE1, 0x6CE2,
    0x6CE3, 0x6CE5, 0x6CE8, 0x6CF3, 0x6CF5, 0x6CFD, 0x6D01, 0x6D0B, 0x6D12, 0x6D17, 0x6D1B, 0x6D1E,
    0x6D3B, 0x6D3C, 0x6D3E, 0x6D41, 0x6D45, 0x6D46, 0x6D47, 0x6D4B, 0x6D4E, 0x6D4F, 0x6D6A, 0x6D6E,
    0x6D77, 0x6D78, 0x6D82, 0x6D85, 0x6D88, 0x6D89, 0x6DA1, 0x6DAF, 0x6DB2, 0x6DB5, 0x6DC6, 0x6DD1,
    0x6DD8, 0x6DE1, 0x6DF1, 0x6DF7, 0x6DF9, 0x6DFB, 0x6E05, 0x6E0A, 0x6E10, 0x6E17, 0x6E20, 0x6E21,
    0x6E23, 0x6E29, 0x6E32, 0x6E38, 0x6E56, 0x6E7F, 0x6E83, 0x6E85, 0x6E90, 0x6EA2, 0x6EAA, 0x6EAF,
    0x6ED1, 0x6EDA, 0x6EDE, 0x6EE1, 0x6EE4, 0x6EE5, 0x6EF4, 0x6F02, 0x6F06, 0x6F0F, 0x6F14, 0x6F2B,
    0x6F31, 0x6F58, 0x6F5C, 0x6F6E, 0x6F84, 0x6FC0, 0x7011, 0x704C, 0x706B, 0x706D, 0x706F, 0x7070,
    0x7075, 0x707C, 0x707F, 0x7089, 0x7092, 0x70AB, 0x70AD, 0x70AE, 0x70B8, 0x70B9, 0x70BC, 0x70C1,
    0x70C2, 0x70C8, 0x70D8, 0x70DF, 0x70E4, 0x70E6, 0x70E7, 0x70EB, 0x70ED, 0x710A, 0x7115, 0x7119,
    0x7126, 0x7130, 0x7136, 0x7167, 0x7194, 0x719F, 0x71C3, 0x71E5, 0x7206, 0x722C, 0x7231, 0x7236,
    0x7237, 0x7238, 0x7239, 0x7247, 0x7248, 0x724C, 0x7259, 0x725B, 0x7262, 0x7269, 0x7272, 0x7275,
    0x7279, 0x727A, 0x72AC, 0x72AF, 0x72B6, 0x72C2, 0x72D7, 0x72D9, 0x72E0, 0x72EC, 0x72F1, 0x72FC,
    0x730E, 0x731B, 0x731C, 0x732A, 0x732B, 0x732E, 0x7334, 0x7387, 0x7389, 0x738B, 0x73A9, 0x73AF,
    0x73B0, 0x73BB, 0x73CD, 0x73DE, 0x73E0, 0x73ED, 0x7403, 0x7406, 0x7410, 0x7430, 0x7434, 0x7435,
    0x7436, 0x7455, 0x745C, 0x745E, 0x7483, 0x7490, 0x74E6, 0x74F6, 0x74F7, 0x751A, 0x751F, 0x7528,
    0x7529, 0x7530, 0x7531, 0x7532, 0x7533, 0x7535, 0x7537, 0x753B, 0x7545, 0x754C, 0x7559, 0x7565,
    0x756A, 0x7574, 0x7578, 0x758F, 0x7591, 0x7597, 0x75AB, 0x75AE, 0x75AF, 0x75B5, 0x75BC, 0x75C5,
    0x75D5, 0x75D8, 0x75DB, 0x75E4, 0x75F9, 0x7626, 0x767B, 0x767D, 0x767E, 0x7684, 0x7686, 0x7687,
    0x76AE, 0x76C6, 0x76C8, 0x76CA, 0x76CF, 0x76D0, 0x76D1, 0x76D2, 0x76D6, 0x76D7, 0x76D8, 0x76EE,
    0x76EF, 0x76F2, 0x76F4, 0x76F8, 0x76FE, 0x7701, 0x770B, 0x771F, 0x7720, 0x7728, 0x7729, 0x773A,
    0x773C, 0x7740, 0x775B, 0x7761, 0x7763, 0x7784, 0x778E, 0x7792, 0x77A5, 0x77AC, 0x77B0, 0x77B3,
    0x77BB, 0x77DB, 0x77E2, 0x77E5, 0x77E9, 0x77EB, 0x77ED, 0x77EE, 0x77F3, 0x7801, 0x7802, 0x780C,
    0x780D, 0x7814, 0x7816, 0x7825, 0x7834, 0x7840, 0x786C, 0x786E, 0x788C, 0x788D, 0x788E, 0x7891,
    0x7897, 0x789F, 0x78A7, 0x78B0, 0x78BE, 0x78C1, 0x78E8, 0x7901, 0x793A, 0x793C, 0x793E, 0x7956,
    0x795E, 0x7968, 0x796D, 0x7981, 0x798F, 0x79BB, 0x79C0, 0x79C1, 0x79C3, 0x79CB, 0x79CD, 0x79D1,
    0x79D2, 0x79D8, 0x79EF, 0x79F0, 0x79FB, 0x7A00, 0x7A0B, 0x7A0D, 0x7A0E, 0x7A1A, 0x7A20, 0x7A33,
    0x7A3F, 0x7A74, 0x7A76, 0x7A77, 0x7A79, 0x7A7A, 0x7A7F, 0x7A81, 0x7A83, 0x7A84, 0x7A97, 0x7A9D,
    0x7ACB, 0x7AD6, 0x7AD9, 0x7ADE, 0x7ADF, 0x7AE0, 0x7AEF, 0x7AF9, 0x7B06, 0x7B11, 0x7B14, 0x7B1B,
    0x7B26, 0x7B28, 0x7B2C, 0x7B3C, 0x7B49, 0x7B4B, 0x7B50, 0x7B51, 0x7B52, 0x7B54, 0x7B56, 0x7B5B,
    0x7B5D, 0x7B77, 0x7B7E, 0x7B80, 0x7B97, 0x7BA1, 0x7BAD, 0x7BB1, 0x7BC7, 0x7BE1, 0x7BF1, 0x7BF7,
    0x7C07, 0x7C27, 0x7C38, 0x7C4D, 0x7C73, 0x7C7B, 0x7C89, 0x7C92, 0x7C97, 0x7C98, 0x7CA5, 0x7CAE,
    0x7CB9, 0x7CBE, 0x7CCA, 0x7CD5, 0x7CD6, 0x7CD9, 0x7CDF, 0x7CFB, 0x7D20, 0x7D22, 0x7D27, 0x7D2B,
    0x7D2F, 0x7E41, 0x7EA0, 0x7EA2, 0x7EA6, 0x7EA7, 0x7EAA, 0x7EAC, 0x7EAF, 0x7EB2, 0x7EB3, 0x7EB5,
    0x7EB8, 0x7EB9, 0x7EBD, 0x7EBF, 0x7EC3, 0x7EC4, 0x7EC6, 0x7EC7, 0x7EC8, 0x7ECD, 0x7ECF, 0x7ED1,
    0x7ED3, 0x7ED5, 0x7ED8, 0x7ED9, 0x7EDC, 0x7EDD, 0x7EDF, 0x7EE7, 0x7EE9, 0x7EEA, 0x7EED, 0x7EF0,
    0x7EF3, 0x7EF4, 0x7EF5, 0x7EFC, 0x7EFD, 0x7EFF, 0x7F00, 0x7F13, 0x7F16, 0x7F18, 0x7F1D, 0x7F29,
    0x7F3A, 0x7F51, 0x7F55, 0x7F57, 0x7F69, 0x7F6A, 0x7F6E, 0x7F72, 0x7F8E, 0x7FA1, 0x7FA4, 0x7FB9,
    0x7FBD, 0x7FC5, 0x7FD4, 0x7FFB, 0x7FFC, 0x8000, 0x8001, 0x8003, 0x8005, 0x800C, 0x8010, 0x8015,
    0x8017, 0x8026, 0x8033, 0x8037, 0x804A, 0x804B, 0x804C, 0x8054, 0x8058, 0x805A, 0x806A, 0x8083,
    0x8089, 0x808C, 0x8096, 0x8098, 0x809A, 0x80A1, 0x80A2, 0x80A4, 0x80A9, 0x80B2, 0x80BF, 0x80C0,
    0x80C1, 0x80C6, 0x80CC, 0x80CE, 0x80D6, 0x80DC, 0x80DE, 0x80E1, 0x80F3, 0x80F6, 0x80F8, 0x80FD,
    0x8106, 0x8109, 0x810A, 0x810F, 0x8111, 0x8116, 0x811A, 0x8131, 0x8138, 0x8150, 0x8155, 0x8170,
    0x817B, 0x817E, 0x817F, 0x8180, 0x818A, 0x818F, 0x819C, 0x819D, 0x81A8, 0x81C2, 0x81C3, 0x81C6,
    0x81E3, 0x81EA, 0x81F3, 0x81F4, 0x820D, 0x8212, 0x821E, 0x822A, 0x822C, 0x8230, 0x8239, 0x826F,
    0x8272, 0x8273, 0x827A, 0x827E, 0x8282, 0x8292, 0x82AF, 0x82B1, 0x82E5, 0x82E6, 0x82F1, 0x82F9,
    0x8302, 0x8303, 0x8304, 0x832B, 0x8336, 0x8349, 0x8350, 0x8352, 0x8361, 0x8363, 0x8367, 0x836F,
    0x83B1, 0x83B2, 0x83B7, 0x83DC, 0x83F1, 0x83F2, 0x8404, 0x840E, 0x8424, 0x8425, 0x8428, 0x843D,
    0x8457, 0x845B, 0x8461, 0x8463, 0x8499, 0x849C, 0x84C4, 0x84DD, 0x8521, 0x852C, 0x853D, 0x8584,
    0x85CF, 0x8638, 0x864E, 0x8651, 0x865A, 0x866B, 0x8679, 0x867D, 0x8681, 0x8682, 0x86C7, 0x86CB,
    0x8702, 0x871C, 0x8721, 0x878D, 0x87BA, 0x8840, 0x884C, 0x884D, 0x8854, 0x8857, 0x8861, 0x8863,
    0x8865, 0x8868, 0x886B, 0x886C, 0x8870, 0x888D, 0x88AB, 0x88AD, 0x88C1, 0x88C2, 0x88C5, 0x88D4,
    0x88F8, 0x88F9, 0x8910, 0x897F, 0x8981, 0x8986, 0x89C1, 0x89C2, 0x89C4, 0x89C6, 0x89C8, 0x89C9,
    0x89D2, 0x89E3, 0x89E6, 0x8A00, 0x8B66, 0x8BA1, 0x8BA2, 0x8BA4, 0x8BA8, 0x8BA9, 0x8BAD, 0x8BAE,
    0x8BAF, 0x8BB0, 0x8BB2, 0x8BB6, 0x8BB8, 0x8BBA, 0x8BBE, 0x8BBF, 0x8BC1, 0x8BC4, 0x8BC6, 0x8BC9,
    0x8BCA, 0x8BCD, 0x8BD1, 0x8BD5, 0x8BDA, 0x8BDD, 0x8BDE, 0x8BE2, 0x8BE5, 0x8BE6, 0x8BED, 0x8BEF,
    0x8BF1, 0x8BF4, 0x8BF7, 0x8BF8, 0x8BFA, 0x8BFB, 0x8BFE, 0x8C01, 0x8C03, 0x8C08, 0x8C0E, 0x8C10,
    0x8C22, 0x8C28, 0x8C31, 0x8C37, 0x8C41, 0x8C46, 0x8C61, 0x8C8C, 0x8D1D, 0x8D1F, 0x8D21, 0x8D22,
    0x8D23, 0x8D25, 0x8D26, 0x8D27, 0x8D28, 0x8D2D, 0x8D2F, 0x8D34, 0x8D35, 0x8D38, 0x8D39, 0x8D3C,
    0x8D44, 0x8D4B, 0x8D4C, 0x8D56, 0x8D5A, 0x8D5B, 0x8D62, 0x8D70, 0x8D75, 0x8D76, 0x8D77, 0x8D85,
    0x8D8A, 0x8D8B, 0x8D9F, 0x8DA3, 0x8DB3, 0x8DB4, 0x8DBE, 0x8DC3, 0x8DD1, 0x8DDD, 0x8DDF, 0x8DE8,
    0x8DEA, 0x8DEF, 0x8DF3, 0x8DF5, 0x8DF7, 0x8E0F, 0x8E22, 0x8E29, 0x8E2A, 0x8E31, 0x8E48, 0x8E66,
    0x8E72, 0x8EAB, 0x8EAF, 0x8EB2, 0x8EBA, 0x8F66, 0x8F68, 0x8F6C, 0x8F6E, 0x8F6F, 0x8F74, 0x8F78,
    0x8F7B, 0x8F7D, 0x8F7F, 0x8F83, 0x8F85, 0x8F86, 0x8F89, 0x8F90, 0x8F91, 0x8F93, 0x8F9E, 0x8FA3,
    0x8FA8, 0x8FB9, 0x8FBD, 0x8FBE, 0x8FC1, 0x8FC7, 0x8FC8, 0x8FCE, 0x8FD0, 0x8FD1, 0x8FD4, 0x8FD8,
    0x8FD9, 0x8FDB, 0x8FDC, 0x8FDD, 0x8FDE, 0x8FDF, 0x8FEB, 0x8FED, 0x8FF0, 0x8FF9, 0x8FFD, 0x9000,
    0x9001, 0x9002, 0x9003, 0x9006, 0x9009, 0x900F, 0x9010, 0x9012, 0x9014, 0x901A, 0x901B, 0x901E,
    0x901F, 0x9020, 0x9038, 0x903B, 0x903C, 0x9047, 0x904D, 0x9053, 0x9057, 0x9065, 0x906E, 0x9075,
    0x907F, 0x9093, 0x90A3, 0x90A6, 0x90AE, 0x90BB, 0x90E8, 0x90ED, 0x90FD, 0x914D, 0x9152, 0x9171,
    0x9177, 0x9192, 0x91C7, 0x91CA, 0x91CC, 0x91CD, 0x91CE, 0x91CF, 0x91D1, 0x9274, 0x9488, 0x9489,
    0x9493, 0x949D, 0x949F, 0x94A2, 0x94A5, 0x94A9, 0x94AE, 0x94B1, 0x94B3, 0x94BB, 0x94C1, 0x94DC,
    0x94E0, 0x94F0, 0x94F2, 0x94F6, 0x94FA, 0x94FE, 0x9500, 0x9501, 0x9505, 0x9508, 0x950B, 0x9510,
    0x9519, 0x951A, 0x9524, 0x9525, 0x9526, 0x952E, 0x952F, 0x953B, 0x9542, 0x9547, 0x9556, 0x955C,
    0x957F, 0x95E8, 0x95EA, 0x95ED, 0x95EE, 0x95EF, 0x95F2, 0x95F4, 0x95F7, 0x95F8, 0x95FB, 0x9605,
    0x9608, 0x9614, 0x961F, 0x9631, 0x9632, 0x9633, 0x9634, 0x9635, 0x9636, 0x963B, 0x963F, 0x9644,
    0x9645, 0x9648, 0x964C, 0x964D, 0x9650, 0x9661, 0x9662, 0x9664, 0x9668, 0x9669, 0x9676, 0x9677,
    0x9686, 0x968F, 0x9690, 0x9694, 0x9699, 0x969C, 0x96A7, 0x96BE, 0x96C0, 0x96C4, 0x96C5, 0x96C6,
    0x96C7, 0x96CF, 0x96D5, 0x96E8, 0x96EA, 0x96F6, 0x96F7, 0x96FE, 0x9700, 0x9707, 0x9713, 0x971C,
    0x971E, 0x9732, 0x9738, 0x9752, 0x9759, 0x975E, 0x9760, 0x9762, 0x9769, 0x9776, 0x97AD, 0x97E6,
    0x97E7, 0x97F3, 0x9875, 0x9876, 0x9879, 0x987A, 0x987B, 0x987E, 0x987F, 0x9884, 0x9886, 0x9888,
    0x9891, 0x9897, 0x9898, 0x989C, 0x989D, 0x98A0, 0x98CE, 0x98D8, 0x98D9, 0x98DE, 0x98DF, 0x9910,
    0x9965, 0x996D, 0x9970, 0x9971, 0x997C, 0x997F, 0x9986, 0x9988, 0x9996, 0x9999, 0x9A6C, 0x9A6D,
    0x9A71, 0x9A73, 0x9A76, 0x9A7B, 0x9A7E, 0x9A81, 0x9A82, 0x9A8C, 0x9A91, 0x9A97, 0x9AA4, 0x9AA8,
    0x9ABC, 0x9ACB, 0x9AD3, 0x9AD8, 0x9AFB, 0x9B3C, 0x9B42, 0x9B54, 0x9C7C, 0x9C81, 0x9E1F, 0x9E21,
    0x9E23, 0x9E3F, 0x9E45, 0x9E70, 0x9E92, 0x9E9F, 0x9EBB, 0x9EC4, 0x9ECE, 0x9ECF, 0x9ED1, 0x9ED8,
    0x9F13, 0x9F20, 0x9F3B, 0x9F41, 0x9F50, 0x9F7F, 0x9F84, 0x9F99,
    };
    std::vector<int> cjk_codepoints;
    cjk_codepoints.reserve(4096);
    for (int cp = 0x20; cp <= 0x7E; ++cp) cjk_codepoints.push_back(cp);
    for (int cp = 0x3000; cp <= 0x303F; ++cp) cjk_codepoints.push_back(cp);
    for (int cp = 0xFF01; cp <= 0xFF5E; ++cp) cjk_codepoints.push_back(cp);
    for (unsigned int cp : kProjectCjkCodepoints) cjk_codepoints.push_back(static_cast<int>(cp));
    // CJK 字形数较多，临时提升图集尺寸为 4096x4096
    auto& cfg = svc->GetConfig();
    int old_w = cfg.default_atlas_width;
    int old_h = cfg.default_atlas_height;
    cfg.default_atlas_width = 4096;
    cfg.default_atlas_height = 4096;
    bool ok = svc->LoadFont(font_id, ttf_path, cjk_codepoints);
    cfg.default_atlas_width = old_w;
    cfg.default_atlas_height = old_h;
    return ok ? 1 : 0;
}

extern "C" void dse_font_unload(const char* font_id) {
    auto* svc = GetFontService();
    if (svc && font_id) svc->UnloadFont(font_id);
}

extern "C" int dse_font_set_default(const char* font_id) {
    auto* svc = GetFontService();
    if (!svc || !font_id) return 0;
    return svc->SetDefaultFont(font_id) ? 1 : 0;
}

extern "C" float dse_font_measure(const char* text, const char* font_id, float font_size) {
    auto* svc = GetFontService();
    if (!svc || !text) return 0.0f;
    return svc->MeasureText(text, font_id ? font_id : "", font_size);
}

extern "C" float dse_font_line_height(const char* font_id, float font_size) {
    auto* svc = GetFontService();
    if (!svc) return 0.0f;
    return svc->GetLineHeight(font_id ? font_id : "", font_size);
}

extern "C" uint32_t dse_font_get_texture(const char* font_id) {
    auto* svc = GetFontService();
    if (!svc) return 0;
    std::string fid = font_id ? font_id : "";
    if (fid.empty()) fid = svc->GetDefaultFontId();
    auto* fi = svc->GetFont(fid);
    return fi ? fi->gpu_texture_handle.raw() : 0;
}

// ============================================================
// Spine — 骨骼动画渲染组件
// ============================================================

extern "C" void dse_spine_add_renderer(uint32_t e, const char* skel_path, const char* atlas_path) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& spine = w->registry().emplace_or_replace<SpineRendererComponent>(TE(e));
    spine.skeleton_data_path = skel_path ? skel_path : "";
    spine.atlas_path = atlas_path ? atlas_path : "";
}

extern "C" void dse_spine_set_animation(uint32_t e, const char* anim_name, int loop) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto* spine = w->registry().try_get<SpineRendererComponent>(TE(e));
    if (!spine || !anim_name) return;
    spine->current_animation = anim_name;
    spine->loop = (loop != 0);
    spine->dirty_animation = true;
}
