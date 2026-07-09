#ifndef DSE_SCENE_JSON_CODEC_CUSTOM_H
#define DSE_SCENE_JSON_CODEC_CUSTOM_H

// Hand-written companions for components whose scene serialization cannot be
// fully expressed through reflection. Invoked from the generated
// scene_json_codec.gen.h for components flagged `custom_scene_codec` in
// binding_defs.json (in addition to, not instead of, the reflected fields).

#include "engine/ecs/components_3d_render.h"

#include <rapidjson/document.h>
#include <string>

namespace dse::scene_codec_custom {

// MorphTargetComponent: persist only per-target name + weight. The delta arrays
// are large and reconstructed from the mesh asset on load (see
// LoadMorphTargetsFromDmesh), so they are intentionally not serialized here.
inline void SerializeExtra(const dse::MorphTargetComponent& c,
                           rapidjson::Value& json,
                           rapidjson::Document::AllocatorType& alloc) {
    rapidjson::Value arr(rapidjson::kArrayType);
    const size_t n = std::min(c.targets.size(), c.weights.size());
    for (size_t i = 0; i < n; ++i) {
        rapidjson::Value item(rapidjson::kObjectType);
        rapidjson::Value name;
        name.SetString(c.targets[i].name.c_str(),
                       static_cast<rapidjson::SizeType>(c.targets[i].name.size()), alloc);
        item.AddMember("name", name, alloc);
        item.AddMember("weight", c.weights[i], alloc);
        arr.PushBack(item, alloc);
    }
    json.AddMember("morph_weights", arr, alloc);
}

// Restore name+weight into a name-only target list; the mesh loader later fills
// in the deltas and preserves these weights by matching target names.
inline void DeserializeExtra(dse::MorphTargetComponent& c,
                             const rapidjson::Value& json) {
    if (!json.HasMember("morph_weights") || !json["morph_weights"].IsArray()) return;
    const auto& arr = json["morph_weights"];
    c.targets.clear();
    c.weights.clear();
    c.targets.reserve(arr.Size());
    c.weights.reserve(arr.Size());
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        const auto& item = arr[i];
        if (!item.IsObject()) continue;
        dse::MorphTargetData target;
        if (item.HasMember("name") && item["name"].IsString()) {
            target.name = item["name"].GetString();
        }
        float w = 0.0f;
        if (item.HasMember("weight") && item["weight"].IsNumber()) {
            w = item["weight"].GetFloat();
        }
        c.targets.push_back(std::move(target));
        c.weights.push_back(w);
    }
}

} // namespace dse::scene_codec_custom

#endif // DSE_SCENE_JSON_CODEC_CUSTOM_H
