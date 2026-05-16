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

static void DestroyDspNode(DspNodeHandle& handle) {
    if (!handle.node_ptr) return;
    switch (handle.type) {
        case DspEffectType::LowPass: {
            auto* n = static_cast<ma_lpf_node*>(handle.node_ptr);
            ma_lpf_node_uninit(n, nullptr);
            delete n;
            break;
        }
        case DspEffectType::HighPass: {
            auto* n = static_cast<ma_hpf_node*>(handle.node_ptr);
            ma_hpf_node_uninit(n, nullptr);
            delete n;
            break;
        }
        case DspEffectType::BandPass: {
            auto* n = static_cast<ma_bpf_node*>(handle.node_ptr);
            ma_bpf_node_uninit(n, nullptr);
            delete n;
            break;
        }
        case DspEffectType::Delay: {
            auto* n = static_cast<ma_delay_node*>(handle.node_ptr);
            ma_delay_node_uninit(n, nullptr);
            delete n;
            break;
        }
        default: break;
    }
    handle.node_ptr = nullptr;
}

static void DestroyAllDspNodes(AudioBus& bus) {
    for (auto& h : bus.active_nodes) {
        if (h.node_ptr) {
            ma_node_detach_all_output_buses(static_cast<ma_node*>(h.node_ptr));
        }
    }
    for (auto& h : bus.active_nodes) {
        DestroyDspNode(h);
    }
    bus.active_nodes.clear();
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
        if (bus) {
            DestroyAllDspNodes(*bus);
            if (bus->group_handle) {
                FreeGroup(static_cast<ma_sound*>(bus->group_handle));
                bus->group_handle = nullptr;
            }
        }
    }
    auto it = buses_.find("master");
    if (it != buses_.end() && it->second) {
        DestroyAllDspNodes(*it->second);
        if (it->second->group_handle) {
            FreeGroup(static_cast<ma_sound*>(it->second->group_handle));
            it->second->group_handle = nullptr;
        }
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

    if (it->second) {
        DestroyAllDspNodes(*it->second);
        if (it->second->group_handle) {
            FreeGroup(static_cast<ma_sound*>(it->second->group_handle));
        }
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
    RebuildEffectChain(*bus);
    return true;
}

bool AudioBusManager::RemoveEffect(const std::string& bus_name, size_t index) {
    auto* bus = GetBus(bus_name);
    if (!bus || index >= bus->effects.size()) return false;
    bus->effects.erase(bus->effects.begin() + static_cast<ptrdiff_t>(index));
    RebuildEffectChain(*bus);
    return true;
}

bool AudioBusManager::SetEffectParams(const std::string& bus_name, size_t index, const DspEffectParams& params) {
    auto* bus = GetBus(bus_name);
    if (!bus || index >= bus->effects.size()) return false;
    bus->effects[index] = params;
    RebuildEffectChain(*bus);
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

void AudioBusManager::RebuildEffectChain(AudioBus& bus) {
    if (!engine_ || !bus.group_handle) return;

    auto* group = static_cast<ma_sound*>(bus.group_handle);
    ma_node_graph* node_graph = ma_engine_get_node_graph(engine_);
    ma_uint32 channels = ma_engine_get_channels(engine_);
    ma_uint32 sample_rate = ma_engine_get_sample_rate(engine_);

    // 1. Tear down existing DSP nodes
    DestroyAllDspNodes(bus);
    ma_node_detach_output_bus(group, 0);

    // 2. Determine target (parent group or engine endpoint)
    ma_node* target = nullptr;
    if (!bus.parent_name.empty()) {
        auto* parent_bus = GetBus(bus.parent_name);
        if (parent_bus && parent_bus->group_handle) {
            target = static_cast<ma_node*>(static_cast<ma_sound*>(parent_bus->group_handle));
        }
    }
    if (!target) {
        target = ma_node_graph_get_endpoint(node_graph);
    }

    // 3. Create nodes for each enabled effect
    for (const auto& effect : bus.effects) {
        if (!effect.enabled) continue;

        DspNodeHandle handle;
        handle.type = effect.type;

        switch (effect.type) {
            case DspEffectType::LowPass: {
                auto* node = new ma_lpf_node();
                auto config = ma_lpf_node_config_init(channels, sample_rate, effect.cutoff_hz, 2);
                if (ma_lpf_node_init(node_graph, &config, nullptr, node) == MA_SUCCESS) {
                    handle.node_ptr = node;
                } else {
                    delete node;
                }
                break;
            }
            case DspEffectType::HighPass: {
                auto* node = new ma_hpf_node();
                auto config = ma_hpf_node_config_init(channels, sample_rate, effect.cutoff_hz, 2);
                if (ma_hpf_node_init(node_graph, &config, nullptr, node) == MA_SUCCESS) {
                    handle.node_ptr = node;
                } else {
                    delete node;
                }
                break;
            }
            case DspEffectType::BandPass: {
                auto* node = new ma_bpf_node();
                auto config = ma_bpf_node_config_init(channels, sample_rate, effect.cutoff_hz, 2);
                if (ma_bpf_node_init(node_graph, &config, nullptr, node) == MA_SUCCESS) {
                    handle.node_ptr = node;
                } else {
                    delete node;
                }
                break;
            }
            case DspEffectType::Delay: {
                auto* node = new ma_delay_node();
                ma_uint32 delay_frames = static_cast<ma_uint32>(effect.delay_time_ms * static_cast<float>(sample_rate) / 1000.0f);
                if (delay_frames < 1) delay_frames = 1;
                auto config = ma_delay_node_config_init(channels, sample_rate, delay_frames, effect.feedback);
                if (ma_delay_node_init(node_graph, &config, nullptr, node) == MA_SUCCESS) {
                    ma_delay_node_set_wet(node, effect.wet_mix);
                    ma_delay_node_set_dry(node, 1.0f - effect.wet_mix);
                } else {
                    delete node;
                    node = nullptr;
                }
                if (node) handle.node_ptr = node;
                break;
            }
            default: break;
        }

        if (handle.node_ptr) {
            bus.active_nodes.push_back(handle);
        }
    }

    // 4. Wire chain: group → node[0] → node[1] → ... → target
    if (bus.active_nodes.empty()) {
        ma_node_attach_output_bus(group, 0, target, 0);
        return;
    }

    ma_node_attach_output_bus(group, 0, static_cast<ma_node*>(bus.active_nodes[0].node_ptr), 0);

    for (size_t i = 0; i + 1 < bus.active_nodes.size(); ++i) {
        ma_node_attach_output_bus(
            static_cast<ma_node*>(bus.active_nodes[i].node_ptr), 0,
            static_cast<ma_node*>(bus.active_nodes[i + 1].node_ptr), 0);
    }

    ma_node_attach_output_bus(
        static_cast<ma_node*>(bus.active_nodes.back().node_ptr), 0,
        target, 0);
}

} // namespace gameplay2d
} // namespace dse
