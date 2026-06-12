#include "editor_anim_retarget_core.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace dse::editor::retarget {

namespace {

std::string ToLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// 去掉常见骨架命名前缀（不区分大小写），返回剥离后的尾部。
std::string StripPrefix(const std::string& lower) {
    static const char* kPrefixes[] = {"mixamorig:", "mixamorig", "armature/", "armature|",
                                      "armature_", "armature", "root/", "bip01", "bip001"};
    for (const char* p : kPrefixes) {
        const size_t n = std::char_traits<char>::length(p);
        if (lower.size() > n && lower.compare(0, n, p) == 0) {
            return lower.substr(n);
        }
    }
    return lower;
}

// 检测左右侧并返回去侧后的核心 token 与侧别（'l'/'r'/0）。
char DetectSide(std::string& core) {
    // 形如 left/right 前后缀，或 _l/_r/.l/.r/ l/ r 结尾。
    auto starts = [&](const char* p) {
        const size_t n = std::char_traits<char>::length(p);
        return core.size() >= n && core.compare(0, n, p) == 0;
    };
    auto ends = [&](const char* p) {
        const size_t n = std::char_traits<char>::length(p);
        return core.size() >= n && core.compare(core.size() - n, n, p) == 0;
    };
    char side = 0;
    if (starts("left"))  { side = 'l'; core = core.substr(4); }
    else if (starts("right")) { side = 'r'; core = core.substr(5); }
    else if (ends("left"))  { side = 'l'; core = core.substr(0, core.size() - 4); }
    else if (ends("right")) { side = 'r'; core = core.substr(0, core.size() - 5); }
    else if (ends("l")) { side = 'l'; core = core.substr(0, core.size() - 1); }
    else if (ends("r")) { side = 'r'; core = core.substr(0, core.size() - 1); }
    return side;
}

}  // namespace

std::string NormalizeBoneName(const std::string& name) {
    std::string s = StripPrefix(ToLower(name));
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(c);
        // 去掉空白、':' '_' '-' '.' '|' '/' 等分隔符
    }
    return out;
}

std::string HumanoidCanonical(const std::string& name) {
    std::string core = NormalizeBoneName(name);
    if (core.empty()) return "";
    const char side = DetectSide(core);

    // 核心部位同义词 → 规范名（不含侧别）。
    struct Syn { const char* canonical; std::array<const char*, 5> tokens; };
    static const Syn kSyns[] = {
        {"hips",      {"hips", "pelvis", "hip", nullptr, nullptr}},
        {"spine",     {"spine", "spine0", nullptr, nullptr, nullptr}},
        {"spine1",    {"spine1", "chest", nullptr, nullptr, nullptr}},
        {"spine2",    {"spine2", "upperchest", "chestupper", nullptr, nullptr}},
        {"neck",      {"neck", nullptr, nullptr, nullptr, nullptr}},
        {"head",      {"head", nullptr, nullptr, nullptr, nullptr}},
        {"shoulder",  {"shoulder", "clavicle", nullptr, nullptr, nullptr}},
        {"upperarm",  {"upperarm", "arm", "armupper", nullptr, nullptr}},
        {"lowerarm",  {"lowerarm", "forearm", "armlower", "elbow", nullptr}},
        {"hand",      {"hand", "wrist", nullptr, nullptr, nullptr}},
        {"upperleg",  {"upperleg", "thigh", "upleg", "legupper", nullptr}},
        {"lowerleg",  {"lowerleg", "calf", "shin", "leglower", "knee"}},
        {"foot",      {"foot", "ankle", nullptr, nullptr, nullptr}},
        {"toe",       {"toe", "toebase", "ball", nullptr, nullptr}},
    };

    for (const Syn& syn : kSyns) {
        for (const char* tok : syn.tokens) {
            if (!tok) break;
            if (core == tok) {
                std::string canonical = syn.canonical;
                if (side) { canonical.push_back('.'); canonical.push_back(side); }
                return canonical;
            }
        }
    }
    return "";
}

BoneMap AutoMapBones(const std::vector<std::string>& source_bones,
                     const std::vector<std::string>& target_bones) {
    // 预计算目标查找表。
    std::unordered_map<std::string, int> by_exact, by_norm, by_humanoid;
    for (int i = 0; i < static_cast<int>(target_bones.size()); ++i) {
        by_exact.emplace(ToLower(target_bones[i]), i);
        by_norm.emplace(NormalizeBoneName(target_bones[i]), i);
        const std::string h = HumanoidCanonical(target_bones[i]);
        if (!h.empty()) by_humanoid.emplace(h, i);
    }

    BoneMap map;
    map.matches.resize(source_bones.size());
    for (int i = 0; i < static_cast<int>(source_bones.size()); ++i) {
        BoneMatch& m = map.matches[i];
        m.source_index = i;

        if (auto it = by_exact.find(ToLower(source_bones[i])); it != by_exact.end()) {
            m.target_index = it->second; m.type = MatchType::Exact; continue;
        }
        if (auto it = by_norm.find(NormalizeBoneName(source_bones[i])); it != by_norm.end()) {
            m.target_index = it->second; m.type = MatchType::Normalized; continue;
        }
        const std::string h = HumanoidCanonical(source_bones[i]);
        if (!h.empty()) {
            if (auto it = by_humanoid.find(h); it != by_humanoid.end()) {
                m.target_index = it->second; m.type = MatchType::Humanoid; continue;
            }
        }
        m.target_index = -1;
        m.type = MatchType::None;
    }
    return map;
}

void SetManualMapping(BoneMap& map, int source_index, int target_index) {
    if (source_index < 0 || source_index >= static_cast<int>(map.matches.size())) return;
    BoneMatch& m = map.matches[source_index];
    m.target_index = target_index;
    m.type = (target_index < 0) ? MatchType::None : MatchType::Manual;
}

int MappedCount(const BoneMap& map) {
    int n = 0;
    for (const auto& m : map.matches) if (m.target_index >= 0) ++n;
    return n;
}

asset::compiler::RawAnimation RetargetAnimation(
    const asset::compiler::RawAnimation& source_anim,
    const std::vector<std::string>& source_bones,
    const std::vector<std::string>& target_bones,
    const BoneMap& map) {
    using asset::compiler::RawAnimationChannel;

    asset::compiler::RawAnimation out;
    out.name = source_anim.name + "_retargeted";
    out.duration = source_anim.duration;

    // 源骨骼下标 → 目标下标 的快速表。
    std::unordered_map<int, int> src_to_tgt;
    for (const auto& m : map.matches) {
        if (m.target_index >= 0) src_to_tgt[m.source_index] = m.target_index;
    }
    // 源骨骼名 → 源下标（通道可能只带名字）。
    std::unordered_map<std::string, int> src_name_to_index;
    for (int i = 0; i < static_cast<int>(source_bones.size()); ++i) {
        src_name_to_index.emplace(source_bones[i], i);
    }

    for (const RawAnimationChannel& ch : source_anim.channels) {
        int src_idx = ch.target_node_index;
        if (src_idx < 0) {
            auto it = src_name_to_index.find(ch.target_node_name);
            if (it != src_name_to_index.end()) src_idx = it->second;
        }
        auto it = src_to_tgt.find(src_idx);
        if (it == src_to_tgt.end()) continue;  // 未映射 → 丢弃

        RawAnimationChannel nc = ch;
        nc.target_node_index = it->second;
        if (it->second < static_cast<int>(target_bones.size())) {
            nc.target_node_name = target_bones[it->second];
        }
        out.channels.push_back(std::move(nc));
    }
    return out;
}

}  // namespace dse::editor::retarget
