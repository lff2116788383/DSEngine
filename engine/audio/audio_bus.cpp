/**
 * @file audio_bus.cpp
 * @brief 混音总线 + DSP 效果链实现
 *
 * miniaudio 中 ma_sound_group 就是 ma_sound 的 typedef，
 * 因此直接使用 ma_sound 配合 MA_SOUND_FLAG_NO_SPATIALIZATION 创建
 * 无音频数据的 "group" 节点，通过 node graph 路由子声音。
 */

#include "audio_bus.h"
#include <miniaudio/miniaudio.h>
#include <algorithm>

#ifdef PlaySound
#undef PlaySound
#endif

namespace dse {
namespace gameplay2d {

// miniaudio 中 sound group 实质是 ma_sound（无数据源），创建/销毁使用专用 API
static ma_sound* AllocGroup(ma_engine* engine, ma_sound* parent) {
    auto* group = new ma_sound();
    ma_result r = ma_sound_group_init(engine, 0, parent, group);
    if (r != MA_SUCCESS) {
        delete group;
        return nullptr;
    }
    return group;
}

static void FreeGroup(ma_sound* group) {
    if (!group) return;
    ma_sound_group_uninit(group);
    delete group;
}

AudioBusManager::AudioBusManager() = default;

AudioBusManager::~AudioBusManager() {
    Shutdown();
}

bool AudioBusManager::Initialize(ma_engine* engine) {
    if (initialized_ || !engine) return false;
    engine_ = engine;

    // master 总线
    auto master = std::make_unique<AudioBus>();
    master->name = "master";
    master->volume = 1.0f;
    master->parent_name = "";
    master->group_handle = AllocGroup(engine_, nullptr);
    if (!master->group_handle) return false;
    buses_["master"] = std::move(master);

    // 子总线
    CreateBus("music", "master", 1.0f);
    CreateBus("sfx", "master", 1.0f);
    CreateBus("voice", "master", 1.0f);

    initialized_ = true;
    return true;
}

void AudioBusManager::Shutdown() {
    if (!initialized_) return;

    // 子先于父
    for (auto& [name, bus] : buses_) {
        if (name == "master") continue;
        if (bus && bus->group_handle) {
            FreeGroup(static_cast<ma_sound*>(bus->group_handle));
            bus->group_handle = nullptr;
        }
    }
    auto it = buses_.find("master");
    if (it != buses_.end() && it->second && it->second->group_handle) {
        FreeGroup(static_cast<ma_sound*>(it->second->group_handle));
        it->second->group_handle = nullptr;
    }

    buses_.clear();
    engine_ = nullptr;
    initialized_ = false;
}

bool AudioBusManager::CreateBus(const std::string& name, const std::string& parent, float volume) {
    if (!engine_) return false;
    if (name.empty() || buses_.count(name)) return false;

    ma_sound* parent_group = nullptr;
    if (!parent.empty()) {
        auto pit = buses_.find(parent);
        if (pit == buses_.end() || !pit->second || !pit->second->group_handle) return false;
        parent_group = static_cast<ma_sound*>(pit->second->group_handle);
    }

    auto bus = std::make_unique<AudioBus>();
    bus->name = name;
    bus->volume = std::clamp(volume, 0.0f, 1.0f);
    bus->parent_name = parent;

    ma_sound* group = AllocGroup(engine_, parent_group);
    if (!group) return false;
    ma_sound_group_set_volume(group, bus->volume);
    bus->group_handle = group;
    buses_[name] = std::move(bus);
    return true;
}

bool AudioBusManager::RemoveBus(const std::string& name) {
    if (name == "master" || name == "music" || name == "sfx" || name == "voice") {
        return false;
    }
    auto it = buses_.find(name);
    if (it == buses_.end()) return false;

    if (it->second && it->second->group_handle) {
        FreeGroup(static_cast<ma_sound*>(it->second->group_handle));
    }
    buses_.erase(it);
    return true;
}

AudioBus* AudioBusManager::GetBus(const std::string& name) {
    auto it = buses_.find(name);
    return (it != buses_.end()) ? it->second.get() : nullptr;
}

const AudioBus* AudioBusManager::GetBus(const std::string& name) const {
    auto it = buses_.find(name);
    return (it != buses_.end()) ? it->second.get() : nullptr;
}

void AudioBusManager::SetBusVolume(const std::string& name, float volume) {
    auto* bus = GetBus(name);
    if (!bus) return;
    bus->volume = std::clamp(volume, 0.0f, 1.0f);
    ApplyBusVolume(*bus);
}

void AudioBusManager::SetBusMuted(const std::string& name, bool muted) {
    auto* bus = GetBus(name);
    if (!bus) return;
    bus->muted = muted;
    ApplyBusVolume(*bus);
}

bool AudioBusManager::AddEffect(const std::string& bus_name, const DspEffectParams& params) {
    auto* bus = GetBus(bus_name);
    if (!bus) return false;
    bus->effects.push_back(params);
    return true;
}

bool AudioBusManager::RemoveEffect(const std::string& bus_name, size_t index) {
    auto* bus = GetBus(bus_name);
    if (!bus || index >= bus->effects.size()) return false;
    bus->effects.erase(bus->effects.begin() + static_cast<ptrdiff_t>(index));
    return true;
}

bool AudioBusManager::SetEffectParams(const std::string& bus_name, size_t index, const DspEffectParams& params) {
    auto* bus = GetBus(bus_name);
    if (!bus || index >= bus->effects.size()) return false;
    bus->effects[index] = params;
    return true;
}

size_t AudioBusManager::GetEffectCount(const std::string& bus_name) const {
    const auto* bus = GetBus(bus_name);
    return bus ? bus->effects.size() : 0;
}

void* AudioBusManager::GetGroupHandle(const std::string& bus_name) {
    auto* bus = GetBus(bus_name);
    return bus ? bus->group_handle : nullptr;
}

std::vector<std::string> AudioBusManager::GetBusNames() const {
    std::vector<std::string> names;
    names.reserve(buses_.size());
    for (const auto& [name, _] : buses_) {
        names.push_back(name);
    }
    return names;
}

void AudioBusManager::ApplyBusVolume(AudioBus& bus) {
    if (!bus.group_handle) return;
    float effective = bus.muted ? 0.0f : bus.volume;
    ma_sound_group_set_volume(static_cast<ma_sound*>(bus.group_handle), effective);
}

void AudioBusManager::RebuildEffectChain(AudioBus& /*bus*/) {
    // 预留：将 DspEffectParams 转换为 ma_biquad_node / ma_lpf_node 链
    // 插入到 node graph（miniaudio node graph 高级 API）
}

} // namespace gameplay2d
} // namespace dse
