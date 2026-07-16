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
    scene::Scene loader("native_api_scene_loader");
    loader.BindWorld(w);
    const bool ok = loader.Deserialize(path);
    loader.UnbindWorld();
    return ok ? 1 : 0;
}

extern "C" int dse_scene_save(const char* path) {
    World* w = GW();
    if (!w || !path) return 0;
    scene::Scene saver("native_api_scene_saver");
    saver.BindWorld(w);
    const bool ok = saver.Serialize(path);
    saver.UnbindWorld();
    return ok ? 1 : 0;
}

extern "C" int dse_scene_save_prefab(uint32_t e, const char* path) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e)) || !path) return 0;
    return scene::SaveEntityAsPrefab(*w, TE(e), path) ? 1 : 0;
}

extern "C" uint32_t dse_scene_instantiate_prefab(const char* path, float x, float y, float z,
                                                 int use_pos) {
    World* w = GW();
    if (!w || !path) return static_cast<uint32_t>(entt::null);
    Entity e;
    if (use_pos) {
        scene::PrefabInstantiateOptions opts;
        opts.override_position = true;
        opts.position = glm::vec3(x, y, z);
        e = scene::InstantiatePrefab(*w, path, opts);
    } else {
        e = scene::InstantiatePrefab(*w, path);
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
    // 常用汉字 (CJK Unified Ideographs 中取前 800 高频字)
    std::vector<int> cjk_codepoints;
    cjk_codepoints.reserve(900);
    for (int cp = 0x4E00; cp <= 0x9FA5 && static_cast<int>(cjk_codepoints.size()) < 800; ++cp) {
        cjk_codepoints.push_back(cp);
    }
    // 常用标点
    for (int cp = 0x3000; cp <= 0x303F; ++cp) cjk_codepoints.push_back(cp);
    for (int cp = 0xFF01; cp <= 0xFF5E; ++cp) cjk_codepoints.push_back(cp);
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
