#ifndef DSE_SCENE_JSON_CODEC_CUSTOM_H
#define DSE_SCENE_JSON_CODEC_CUSTOM_H

// Hand-written companions for components whose scene serialization cannot be
// fully expressed through reflection. Invoked from the generated
// scene_json_codec.gen.h for components flagged `custom_scene_codec` in
// binding_defs.json (in addition to, not instead of, the reflected fields).

#include "engine/ecs/components_3d_render.h"
#include "engine/scripting/native_api/dse_api_core.h"

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

// Sprite3DComponent stores a TextureRef, which reflection intentionally does
// not expose (it is a managed handle, not a POD field). Persist the raw RHI
// handle alongside the reflected Sprite3D fields so scene JSON round-trips the
// component without dropping texture assignment. Note: raw handles are only
// stable within a session; a future .dsprite/asset-path field should replace
// this when the Sprite3D asset pipeline lands (M5).
inline void SerializeExtra(const dse::Sprite3DComponent& c,
                           rapidjson::Value& json,
                           rapidjson::Document::AllocatorType& alloc) {
    json.AddMember("texture_handle", c.texture_handle.raw(), alloc);
    json.AddMember("normal_handle", c.normal_handle.raw(), alloc);
    json.AddMember("emissive_handle", c.emissive_handle.raw(), alloc);
    if (!c.atlas_path.empty())
        json.AddMember("atlas_path", rapidjson::Value(c.atlas_path.c_str(), alloc), alloc);
    if (!c.clip_name.empty())
        json.AddMember("clip_name", rapidjson::Value(c.clip_name.c_str(), alloc), alloc);
    json.AddMember("anim_fps", c.anim_fps, alloc);
    json.AddMember("anim_loop", c.anim_loop, alloc);
}

inline void DeserializeExtra(dse::Sprite3DComponent& c,
                             const rapidjson::Value& json) {
    if (json.HasMember("texture_handle") && json["texture_handle"].IsUint()) {
        c.texture_handle = dse::render::TextureHandle::from_raw(json["texture_handle"].GetUint());
    }
    if (json.HasMember("normal_handle") && json["normal_handle"].IsUint()) {
        c.normal_handle = dse::render::TextureHandle::from_raw(json["normal_handle"].GetUint());
    }
    if (json.HasMember("emissive_handle") && json["emissive_handle"].IsUint()) {
        c.emissive_handle = dse::render::TextureHandle::from_raw(json["emissive_handle"].GetUint());
    }
    if (json.HasMember("atlas_path") && json["atlas_path"].IsString()) {
        c.atlas_path = json["atlas_path"].GetString();
    }
    if (json.HasMember("clip_name") && json["clip_name"].IsString()) {
        c.clip_name = json["clip_name"].GetString();
    }
    if (json.HasMember("anim_fps") && json["anim_fps"].IsNumber()) {
        c.anim_fps = json["anim_fps"].GetFloat();
    }
    if (json.HasMember("anim_loop")) {
        if (json["anim_loop"].IsBool()) c.anim_loop = json["anim_loop"].GetBool();
        else if (json["anim_loop"].IsInt()) c.anim_loop = json["anim_loop"].GetInt() != 0;
    }

    // M5: re-resolve .dsprite asset paths so scene load never depends on stale
    // raw RHI handles. The runtime atlas registry owns the texture references.
    if (!c.atlas_path.empty()) {
        const int atlas = dse_sprite_atlas_load(c.atlas_path.c_str(), 0, 1);
        if (atlas >= 0) {
            c.atlas_handle = atlas;
            const uint32_t texture = dse_sprite_atlas_texture(atlas);
            if (texture) c.texture_handle = dse::render::TextureHandle::from_raw(texture);
            const uint32_t normal = dse_sprite_atlas_normal_texture(atlas);
            c.normal_handle = normal
                ? dse::render::TextureHandle::from_raw(normal)
                : dse::render::TextureHandle{};
            const uint32_t emissive = dse_sprite_atlas_emissive_texture(atlas);
            c.emissive_handle = emissive
                ? dse::render::TextureHandle::from_raw(emissive)
                : dse::render::TextureHandle{};

            const int frame_count = dse_sprite_atlas_clip_frame_count(atlas, c.clip_name.c_str());
            c.clip_uvs.clear();
            for (int i = 0; i < frame_count; ++i) {
                float uv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
                dse_sprite_atlas_clip_frame_uv(atlas, c.clip_name.c_str(), i, uv);
                c.clip_uvs.emplace_back(uv[0], uv[1], uv[2], uv[3]);
            }
            if (!c.clip_uvs.empty()) {
                c.uv_rect = c.clip_uvs.front();
                if (c.anim_fps <= 0.0f) c.anim_fps = dse_sprite_atlas_clip_fps(atlas, c.clip_name.c_str());
                c.anim_playing = c.anim_fps > 0.0f;
            }
        }
    }
}

} // namespace dse::scene_codec_custom

#endif // DSE_SCENE_JSON_CODEC_CUSTOM_H
