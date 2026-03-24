#include <napi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

namespace {
constexpr int FRAME_WIDTH = 800;
constexpr int FRAME_HEIGHT = 600;
constexpr float PIXELS_PER_WORLD_UNIT = 20.0f;

std::vector<unsigned char> g_frame_buffer(FRAME_WIDTH * FRAME_HEIGHT * 4, 255);
std::vector<unsigned char> g_external_frame_buffer;
int g_external_frame_width = 0;
int g_external_frame_height = 0;
bool g_use_external_frame = false;
std::string g_external_frame_source = "bridge";
uint32_t g_next_texture_handle = 500000;
std::unordered_map<uint32_t, std::string> g_imported_textures;
std::unordered_map<std::string, uint32_t> g_texture_path_to_handle;
struct MaterialHotUpdateEvent {
    uint64_t sequence = 0;
    uint32_t material_id = 0;
    std::string name;
    std::string shader_variant = "SPRITE_UNLIT";
    uint32_t blend_mode = 0;
    uint32_t texture_handle = 0;
    glm::vec4 tint = glm::vec4(1.0f);
    glm::vec4 uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
};
uint32_t g_next_material_instance_id = 1000;
std::unordered_map<uint32_t, Entity> g_material_entities;
std::vector<MaterialHotUpdateEvent> g_material_hot_update_events;
uint64_t g_material_hot_update_sequence = 1;
bool g_material_replay_guard = false;
const std::vector<std::string> g_shader_variants = {
    "SPRITE_UNLIT",
    "SPRITE_TINT",
    "SPRITE_ADDITIVE"
};
Phase1World* g_world = nullptr;
bool g_engine_initialized = false;
std::unordered_map<uint32_t, std::string> g_entity_names;
float g_sim_time = 0.0f;
double g_bridge_copy_ms = 0.0;
double g_bridge_latency_ms = 0.0;
double g_bridge_throughput_mbps = 0.0;
uint32_t g_bridge_last_frame_id = 0;
uint32_t g_bridge_dropped_frames = 0;
uint64_t g_bridge_last_consume_time_us = 0;
uint32_t g_bridge_draw_calls = 0;
uint32_t g_bridge_sprite_count = 0;
uint32_t g_bridge_max_batch_sprites = 0;
uint32_t g_bridge_entity_count = 0;
uint32_t g_bridge_physics_body_count = 0;

#ifdef _WIN32
struct SharedFrameHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t pixel_bytes;
    uint32_t frame_id;
    uint32_t reserved0;
    uint64_t capture_time_us;
    uint32_t sprite_count;
    uint32_t draw_calls;
    uint32_t max_batch_sprites;
    uint32_t entity_count;
    uint32_t physics_body_count;
    uint32_t ready;
};

constexpr uint32_t kSharedFrameMagic = 0x44534652;
constexpr uint32_t kSharedFrameVersion = 1;
constexpr size_t kSharedFrameMaxBytes = 16 * 1024 * 1024;
constexpr size_t kSharedFrameMappingSize = sizeof(SharedFrameHeader) + kSharedFrameMaxBytes;

HANDLE g_shared_frame_mapping = nullptr;
const unsigned char* g_shared_frame_view = nullptr;
std::string g_shared_frame_mapping_name;
uint32_t g_last_shared_frame_id = 0;

void CloseSharedFrameReader() {
    if (g_shared_frame_view) {
        UnmapViewOfFile(g_shared_frame_view);
        g_shared_frame_view = nullptr;
    }
    if (g_shared_frame_mapping) {
        CloseHandle(g_shared_frame_mapping);
        g_shared_frame_mapping = nullptr;
    }
    g_shared_frame_mapping_name.clear();
}

bool EnsureSharedFrameReader(const char* mapping_name) {
    if (!mapping_name || mapping_name[0] == '\0') {
        CloseSharedFrameReader();
        return false;
    }
    if (g_shared_frame_mapping && g_shared_frame_view && g_shared_frame_mapping_name == mapping_name) {
        return true;
    }
    CloseSharedFrameReader();
    g_shared_frame_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mapping_name);
    if (!g_shared_frame_mapping) {
        return false;
    }
    g_shared_frame_view = reinterpret_cast<const unsigned char*>(MapViewOfFile(g_shared_frame_mapping, FILE_MAP_READ, 0, 0, kSharedFrameMappingSize));
    if (!g_shared_frame_view) {
        CloseHandle(g_shared_frame_mapping);
        g_shared_frame_mapping = nullptr;
        return false;
    }
    g_shared_frame_mapping_name = mapping_name;
    return true;
}
#endif

uint64_t NowUs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count());
}

uint32_t ToId(Entity entity) {
    return static_cast<uint32_t>(entity);
}

Entity ToEntity(uint32_t id) {
    return static_cast<Entity>(id);
}

uint32_t ToBlendModeValue(SpriteBlendMode mode) {
    return static_cast<uint32_t>(mode);
}

SpriteBlendMode ToBlendModeEnum(uint32_t mode_value) {
    if (mode_value >= 2) {
        return SpriteBlendMode::Multiply;
    }
    if (mode_value == 1) {
        return SpriteBlendMode::Additive;
    }
    return SpriteBlendMode::Alpha;
}

void RecordMaterialHotUpdate(const MaterialInstanceComponent& material) {
    if (g_material_replay_guard) {
        return;
    }
    MaterialHotUpdateEvent event;
    event.sequence = g_material_hot_update_sequence++;
    event.material_id = material.material_id;
    event.name = material.name;
    event.shader_variant = material.shader_variant;
    event.blend_mode = ToBlendModeValue(material.blend_mode);
    event.texture_handle = material.texture_handle;
    event.tint = material.tint;
    event.uv_rect = material.uv_rect;
    g_material_hot_update_events.push_back(event);
}

MaterialInstanceComponent* GetMaterialComponent(uint32_t material_id) {
    if (!g_world) {
        return nullptr;
    }
    auto it = g_material_entities.find(material_id);
    if (it == g_material_entities.end()) {
        return nullptr;
    }
    Entity entity = it->second;
    if (!g_world->registry().valid(entity) || !g_world->registry().all_of<MaterialInstanceComponent>(entity)) {
        return nullptr;
    }
    return &g_world->registry().get<MaterialInstanceComponent>(entity);
}

void ApplyMaterialToBoundEntities(uint32_t material_id) {
    if (!g_world) {
        return;
    }
    auto* material = GetMaterialComponent(material_id);
    if (!material) {
        return;
    }
    auto sprite_view = g_world->registry().view<SpriteRendererComponent>();
    for (auto entity : sprite_view) {
        auto& sprite = sprite_view.get<SpriteRendererComponent>(entity);
        if (sprite.material_instance_id != material_id) {
            continue;
        }
        sprite.shader_variant = material->shader_variant;
        sprite.blend_mode = material->blend_mode;
        sprite.texture_handle = material->texture_handle;
        sprite.color = material->tint;
        sprite.uv = material->uv_rect;
    }
}

std::string GetOrCreateEntityName(uint32_t id) {
    auto it = g_entity_names.find(id);
    if (it != g_entity_names.end()) {
        return it->second;
    }
    std::string name = "Entity_" + std::to_string(id);
    g_entity_names[id] = name;
    return name;
}

void PutPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT) {
        return;
    }
    int index = (y * FRAME_WIDTH + x) * 4;
    g_frame_buffer[index + 0] = r;
    g_frame_buffer[index + 1] = g;
    g_frame_buffer[index + 2] = b;
    g_frame_buffer[index + 3] = a;
}

void DrawRect(int center_x, int center_y, int half_w, int half_h, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    for (int y = center_y - half_h; y <= center_y + half_h; ++y) {
        for (int x = center_x - half_w; x <= center_x + half_w; ++x) {
            PutPixel(x, y, r, g, b, a);
        }
    }
}

void ResetEditorWorld() {
    g_world = &Phase1World::Instance();
    g_world->Clear();
    g_entity_names.clear();
    g_imported_textures.clear();
    g_texture_path_to_handle.clear();
    g_next_texture_handle = 500000;
    g_material_entities.clear();
    g_material_hot_update_events.clear();
    g_material_hot_update_sequence = 1;
    g_next_material_instance_id = 1000;
    g_sim_time = 0.0f;

    uint32_t default_material_id = g_next_material_instance_id++;
    Entity default_material_entity = g_world->CreateEntity();
    auto& default_material = g_world->registry().emplace<MaterialInstanceComponent>(default_material_entity);
    default_material.material_id = default_material_id;
    default_material.name = "Default_Unlit";
    default_material.shader_variant = "SPRITE_UNLIT";
    default_material.blend_mode = SpriteBlendMode::Alpha;
    default_material.texture_handle = 0;
    default_material.tint = glm::vec4(1.0f);
    default_material.uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    g_material_entities[default_material_id] = default_material_entity;

    Entity camera = g_world->CreateEntity();
    auto& camera_transform = g_world->registry().emplace<TransformComponent>(camera);
    camera_transform.position = glm::vec3(0.0f, 0.0f, 10.0f);
    g_entity_names[ToId(camera)] = "Main Camera";

    Entity player = g_world->CreateEntity();
    auto& player_transform = g_world->registry().emplace<TransformComponent>(player);
    player_transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    player_transform.scale = glm::vec3(1.5f, 1.5f, 1.0f);
    auto& player_sprite = g_world->registry().emplace<SpriteRendererComponent>(player);
    player_sprite.color = glm::vec4(0.25f, 0.65f, 1.0f, 1.0f);
    player_sprite.material_instance_id = default_material.material_id;
    player_sprite.shader_variant = default_material.shader_variant;
    player_sprite.blend_mode = default_material.blend_mode;
    player_sprite.uv = default_material.uv_rect;
    g_entity_names[ToId(player)] = "Player";

    Entity ground = g_world->CreateEntity();
    auto& ground_transform = g_world->registry().emplace<TransformComponent>(ground);
    ground_transform.position = glm::vec3(0.0f, -8.0f, 0.0f);
    ground_transform.scale = glm::vec3(12.0f, 1.0f, 1.0f);
    auto& ground_sprite = g_world->registry().emplace<SpriteRendererComponent>(ground);
    ground_sprite.color = glm::vec4(0.2f, 0.9f, 0.3f, 1.0f);
    ground_sprite.material_instance_id = default_material.material_id;
    ground_sprite.shader_variant = default_material.shader_variant;
    ground_sprite.blend_mode = default_material.blend_mode;
    ground_sprite.uv = default_material.uv_rect;
    g_entity_names[ToId(ground)] = "Ground";
}

glm::vec4 ColorFromTextureHandle(uint32_t handle) {
    if (handle == 0) {
        return glm::vec4(0.95f, 0.8f, 0.2f, 1.0f);
    }
    float r = static_cast<float>((handle * 37u) % 255u) / 255.0f;
    float g = static_cast<float>((handle * 73u) % 255u) / 255.0f;
    float b = static_cast<float>((handle * 109u) % 255u) / 255.0f;
    return glm::vec4(std::max(r, 0.2f), std::max(g, 0.2f), std::max(b, 0.2f), 1.0f);
}

void TickWorld(float delta_time) {
    if (!g_world) {
        return;
    }
    g_sim_time += std::max(0.0f, delta_time);
    for (auto& pair : g_entity_names) {
        if (pair.second == "Player") {
            Entity entity = ToEntity(pair.first);
            if (g_world->registry().valid(entity) && g_world->registry().all_of<TransformComponent>(entity)) {
                auto& transform = g_world->registry().get<TransformComponent>(entity);
                transform.position.y = std::sin(g_sim_time * 1.5f) * 2.0f;
                transform.dirty = true;
            }
            break;
        }
    }
}

void RenderFrameBufferFromWorld() {
    for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; ++i) {
        g_frame_buffer[i * 4 + 0] = 28;
        g_frame_buffer[i * 4 + 1] = 30;
        g_frame_buffer[i * 4 + 2] = 38;
        g_frame_buffer[i * 4 + 3] = 255;
    }

    if (!g_world) {
        return;
    }

    struct DrawEntry {
        uint32_t texture_handle = 0;
        uint32_t material_instance_id = 0;
        uint32_t shader_variant_key = 0;
        uint32_t blend_mode = 0;
        int sorting_layer = 0;
        int order_in_layer = 0;
        int center_x = 0;
        int center_y = 0;
        int half_w = 0;
        int half_h = 0;
        unsigned char r = 255;
        unsigned char g = 255;
        unsigned char b = 255;
        unsigned char a = 255;
    };

    std::vector<DrawEntry> entries;
    auto view = g_world->registry().view<TransformComponent>();
    auto physics_view = g_world->registry().view<RigidBody2DComponent>();
    g_bridge_physics_body_count = 0;
    for (auto entity : physics_view) {
        (void)entity;
        ++g_bridge_physics_body_count;
    }

    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);
        DrawEntry entry;
        glm::vec4 color(0.75f, 0.78f, 0.86f, 1.0f);
        if (g_world->registry().all_of<SpriteRendererComponent>(entity)) {
            const auto& sprite = g_world->registry().get<SpriteRendererComponent>(entity);
            color = sprite.color;
            entry.texture_handle = sprite.texture_handle;
            entry.material_instance_id = sprite.material_instance_id;
            entry.shader_variant_key = static_cast<uint32_t>(std::hash<std::string>{}(sprite.shader_variant));
            entry.blend_mode = static_cast<uint32_t>(sprite.blend_mode);
            entry.sorting_layer = sprite.sorting_layer;
            entry.order_in_layer = sprite.order_in_layer;
        }
        entry.center_x = static_cast<int>(transform.position.x * PIXELS_PER_WORLD_UNIT) + FRAME_WIDTH / 2;
        entry.center_y = static_cast<int>(-transform.position.y * PIXELS_PER_WORLD_UNIT) + FRAME_HEIGHT / 2;
        entry.half_w = std::max(4, static_cast<int>(transform.scale.x * PIXELS_PER_WORLD_UNIT * 0.5f));
        entry.half_h = std::max(4, static_cast<int>(transform.scale.y * PIXELS_PER_WORLD_UNIT * 0.5f));
        entry.r = static_cast<unsigned char>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
        entry.g = static_cast<unsigned char>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
        entry.b = static_cast<unsigned char>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
        entry.a = static_cast<unsigned char>(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f);
        entries.push_back(entry);
    }
    g_bridge_entity_count = static_cast<uint32_t>(entries.size());

    std::sort(entries.begin(), entries.end(), [](const DrawEntry& a, const DrawEntry& b) {
        if (a.sorting_layer != b.sorting_layer) return a.sorting_layer < b.sorting_layer;
        if (a.shader_variant_key != b.shader_variant_key) return a.shader_variant_key < b.shader_variant_key;
        if (a.material_instance_id != b.material_instance_id) return a.material_instance_id < b.material_instance_id;
        if (a.texture_handle != b.texture_handle) return a.texture_handle < b.texture_handle;
        if (a.blend_mode != b.blend_mode) return a.blend_mode < b.blend_mode;
        return a.order_in_layer < b.order_in_layer;
    });

    g_bridge_draw_calls = 0;
    g_bridge_sprite_count = static_cast<uint32_t>(entries.size());
    g_bridge_max_batch_sprites = 0;
    uint32_t current_batch_sprites = 0;
    uint32_t current_tex = entries.empty() ? 0 : entries.front().texture_handle;
    uint32_t current_material = entries.empty() ? 0 : entries.front().material_instance_id;
    uint32_t current_variant = entries.empty() ? 0 : entries.front().shader_variant_key;
    uint32_t current_blend_mode = entries.empty() ? 0 : entries.front().blend_mode;

    for (const auto& entry : entries) {
        if (current_batch_sprites == 0) {
            current_tex = entry.texture_handle;
            current_material = entry.material_instance_id;
            current_variant = entry.shader_variant_key;
            current_blend_mode = entry.blend_mode;
            ++g_bridge_draw_calls;
        } else if (entry.texture_handle != current_tex || entry.material_instance_id != current_material || entry.shader_variant_key != current_variant || entry.blend_mode != current_blend_mode) {
            g_bridge_max_batch_sprites = std::max(g_bridge_max_batch_sprites, current_batch_sprites);
            current_batch_sprites = 0;
            current_tex = entry.texture_handle;
            current_material = entry.material_instance_id;
            current_variant = entry.shader_variant_key;
            current_blend_mode = entry.blend_mode;
            ++g_bridge_draw_calls;
        }
        ++current_batch_sprites;
        DrawRect(entry.center_x, entry.center_y, entry.half_w, entry.half_h, entry.r, entry.g, entry.b, entry.a);
    }
    g_bridge_max_batch_sprites = std::max(g_bridge_max_batch_sprites, current_batch_sprites);
}

void TryReadSharedFrame() {
#ifdef _WIN32
    const char* mapping_name = std::getenv("DSE_EDITOR_FRAME_SHM_NAME");
    if (!EnsureSharedFrameReader(mapping_name)) {
        return;
    }
    if (!g_shared_frame_view) {
        return;
    }
    const auto* header = reinterpret_cast<const SharedFrameHeader*>(g_shared_frame_view);
    if (header->ready != 1 || header->magic != kSharedFrameMagic || header->version != kSharedFrameVersion) {
        return;
    }
    uint32_t frame_id_before = header->frame_id;
    if (frame_id_before == 0 || frame_id_before == g_last_shared_frame_id) {
        return;
    }
    uint32_t width = header->width;
    uint32_t height = header->height;
    uint32_t pixel_bytes = header->pixel_bytes;
    if (width == 0 || height == 0 || pixel_bytes == 0 || pixel_bytes > kSharedFrameMaxBytes) {
        return;
    }
    if (pixel_bytes < width * height * 4) {
        return;
    }
    const unsigned char* pixel_data = g_shared_frame_view + sizeof(SharedFrameHeader);
    auto copy_begin = std::chrono::high_resolution_clock::now();
    std::vector<unsigned char> staging(pixel_bytes);
    std::memcpy(staging.data(), pixel_data, pixel_bytes);
    auto copy_end = std::chrono::high_resolution_clock::now();
    uint32_t frame_id_after = header->frame_id;
    if (frame_id_before != frame_id_after || header->ready != 1) {
        return;
    }
    g_external_frame_buffer.swap(staging);
    g_external_frame_width = static_cast<int>(width);
    g_external_frame_height = static_cast<int>(height);
    g_use_external_frame = true;
    g_external_frame_source = "engine-shm";
    g_last_shared_frame_id = frame_id_after;
    g_bridge_copy_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(copy_end - copy_begin).count();
    uint64_t now_us = NowUs();
    if (header->capture_time_us > 0 && now_us >= header->capture_time_us) {
        g_bridge_latency_ms = static_cast<double>(now_us - header->capture_time_us) / 1000.0;
    }
    if (g_bridge_last_consume_time_us > 0 && now_us > g_bridge_last_consume_time_us) {
        double seconds = static_cast<double>(now_us - g_bridge_last_consume_time_us) / 1000000.0;
        g_bridge_throughput_mbps = (static_cast<double>(pixel_bytes) / (1024.0 * 1024.0)) / seconds;
    }
    if (g_bridge_last_frame_id > 0 && frame_id_after > g_bridge_last_frame_id + 1) {
        g_bridge_dropped_frames += (frame_id_after - g_bridge_last_frame_id - 1);
    }
    g_bridge_sprite_count = header->sprite_count;
    g_bridge_draw_calls = header->draw_calls;
    g_bridge_max_batch_sprites = header->max_batch_sprites;
    if (header->entity_count > 0) {
        g_bridge_entity_count = header->entity_count;
    } else if (g_world) {
        g_bridge_entity_count = static_cast<uint32_t>(g_world->EntityCount());
    }
    if (header->physics_body_count > 0) {
        g_bridge_physics_body_count = header->physics_body_count;
    } else if (g_world) {
        g_bridge_physics_body_count = 0;
        auto physics_view = g_world->registry().view<RigidBody2DComponent>();
        for (auto entity : physics_view) {
            (void)entity;
            ++g_bridge_physics_body_count;
        }
    }
    g_bridge_last_frame_id = frame_id_after;
    g_bridge_last_consume_time_us = now_us;
#endif
}
}

Napi::String GetEngineVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, "DSEngine Phase 1 (Electron Bridge)");
}

Napi::Object InitEngine(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!g_engine_initialized) {
        ResetEditorWorld();
        g_engine_initialized = true;
    }

    Napi::Object result = Napi::Object::New(env);
    result.Set("status", "Initialized");
    result.Set("entityCount", static_cast<uint32_t>(g_world->EntityCount()));
    return result;
}

Napi::Value GetFrameBuffer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    TryReadSharedFrame();
    if (g_use_external_frame && !g_external_frame_buffer.empty()) {
        return Napi::Buffer<unsigned char>::New(
            env,
            g_external_frame_buffer.data(),
            g_external_frame_buffer.size(),
            [](Napi::Env, unsigned char*) {}
        );
    }
    RenderFrameBufferFromWorld();
    return Napi::Buffer<unsigned char>::New(
        env,
        g_frame_buffer.data(),
        g_frame_buffer.size(),
        [](Napi::Env, unsigned char*) {}
    );
}

Napi::Value GetFrameInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object result = Napi::Object::New(env);
    TryReadSharedFrame();
    if (g_use_external_frame && !g_external_frame_buffer.empty() && g_external_frame_width > 0 && g_external_frame_height > 0) {
        result.Set("width", g_external_frame_width);
        result.Set("height", g_external_frame_height);
        result.Set("source", g_external_frame_source);
    } else {
        result.Set("width", FRAME_WIDTH);
        result.Set("height", FRAME_HEIGHT);
        result.Set("source", "bridge");
    }
    return result;
}

Napi::Value GetEntities(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array entitiesArray = Napi::Array::New(env);

    if (!g_world) {
        return entitiesArray;
    }

    auto view = g_world->registry().view<TransformComponent>();
    int index = 0;
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        
        Napi::Object obj = Napi::Object::New(env);
        uint32_t id = ToId(entity);
        obj.Set("id", id);
        obj.Set("name", GetOrCreateEntityName(id));
        
        Napi::Object pos = Napi::Object::New(env);
        pos.Set("x", transform.position.x);
        pos.Set("y", transform.position.y);
        pos.Set("z", transform.position.z);
        obj.Set("position", pos);
        uint32_t texture_handle = 0;
        uint32_t material_instance_id = 0;
        std::string shader_variant = "SPRITE_UNLIT";
        uint32_t blend_mode = 0;
        glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
        if (g_world->registry().all_of<SpriteRendererComponent>(entity)) {
            const auto& sprite = g_world->registry().get<SpriteRendererComponent>(entity);
            texture_handle = sprite.texture_handle;
            material_instance_id = sprite.material_instance_id;
            shader_variant = sprite.shader_variant;
            blend_mode = static_cast<uint32_t>(sprite.blend_mode);
            uv = sprite.uv;
        }
        obj.Set("textureHandle", texture_handle);
        obj.Set("materialInstanceId", material_instance_id);
        obj.Set("shaderVariant", shader_variant);
        obj.Set("blendMode", blend_mode);
        Napi::Array uv_arr = Napi::Array::New(env, 4);
        uv_arr.Set(0u, uv.x);
        uv_arr.Set(1u, uv.y);
        uv_arr.Set(2u, uv.z);
        uv_arr.Set(3u, uv.w);
        obj.Set("uv", uv_arr);
        
        entitiesArray.Set(index++, obj);
    }

    return entitiesArray;
}

Napi::Value BuildProject(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    std::string target = "win64";
    if (info.Length() > 0 && info[0].IsString()) {
        target = info[0].As<Napi::String>().Utf8Value();
    }
    
    std::string output_msg = "Build pipeline triggered for target: " + target + ". Check console for details.";
    
    Napi::Object result = Napi::Object::New(env);
    result.Set("status", "success");
    result.Set("message", output_msg);
    return result;
}

Napi::Value UpdateEntityTransform(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 4 || !g_world) {
        return Napi::Boolean::New(env, false);
    }
    
    uint32_t id = info[0].As<Napi::Number>().Uint32Value();
    float x = info[1].As<Napi::Number>().FloatValue();
    float y = info[2].As<Napi::Number>().FloatValue();
    float z = info[3].As<Napi::Number>().FloatValue();

    Entity entity = (Entity)id;
    if (g_world->registry().valid(entity) && g_world->registry().all_of<TransformComponent>(entity)) {
        auto& t = g_world->registry().get<TransformComponent>(entity);
        t.position = glm::vec3(x, y, z);
        t.dirty = true;
        return Napi::Boolean::New(env, true);
    }

    return Napi::Boolean::New(env, false);
}

Napi::Value PickEntity(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !g_world) {
        return Napi::Number::New(env, -1);
    }
    
    float mouse_x = info[0].As<Napi::Number>().FloatValue();
    float mouse_y = info[1].As<Napi::Number>().FloatValue();

    auto view = g_world->registry().view<TransformComponent>();
    float closest_dist = 999999.0f;
    int picked_id = -1;

    for (auto entity : view) {
        auto& t = view.get<TransformComponent>(entity);
        float dx = t.position.x - mouse_x;
        float dy = t.position.y - mouse_y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float pick_radius = std::max(0.8f, std::max(t.scale.x, t.scale.y));
        if (dist < pick_radius && dist < closest_dist) {
            closest_dist = dist;
            picked_id = static_cast<int>(ToId(entity));
        }
    }

    return Napi::Number::New(env, picked_id);
}

Napi::Value CreateEntity(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!g_world) {
        return Napi::Number::New(env, -1);
    }
    
    Entity e = g_world->CreateEntity();
    auto& transform = g_world->registry().emplace<TransformComponent>(e, glm::vec3(0, 0, 0));
    transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    auto& sprite = g_world->registry().emplace<SpriteRendererComponent>(e);
    sprite.color = glm::vec4(0.95f, 0.8f, 0.2f, 1.0f);
    if (!g_material_entities.empty()) {
        auto it = g_material_entities.begin();
        auto* material = GetMaterialComponent(it->first);
        if (material) {
            sprite.material_instance_id = material->material_id;
            sprite.shader_variant = material->shader_variant;
            sprite.blend_mode = material->blend_mode;
            sprite.texture_handle = material->texture_handle;
            sprite.color = material->tint;
            sprite.uv = material->uv_rect;
        }
    }

    uint32_t id = ToId(e);
    g_entity_names[id] = "Entity_" + std::to_string(id);

    return Napi::Number::New(env, id);
}

Napi::Value DeleteEntity(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !g_world) {
        return Napi::Boolean::New(env, false);
    }
    
    uint32_t id = info[0].As<Napi::Number>().Uint32Value();
    Entity entity = (Entity)id;
    
    if (g_world->registry().valid(entity)) {
        g_world->DestroyEntity(entity);
        g_entity_names.erase(id);
        return Napi::Boolean::New(env, true);
    }
    
    return Napi::Boolean::New(env, false);
}

Napi::Value TickEngine(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!g_world) {
        return Napi::Boolean::New(env, false);
    }
    float delta_time = 1.0f / 60.0f;
    if (info.Length() > 0 && info[0].IsNumber()) {
        delta_time = info[0].As<Napi::Number>().FloatValue();
    }
    TickWorld(delta_time);
    return Napi::Boolean::New(env, true);
}

Napi::Value ImportTexture(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object result = Napi::Object::New(env);
    if (info.Length() < 1 || !info[0].IsString()) {
        result.Set("success", false);
        result.Set("error", "invalid_path");
        return result;
    }
    std::string path = info[0].As<Napi::String>().Utf8Value();
    if (path.empty() || !std::filesystem::exists(path)) {
        result.Set("success", false);
        result.Set("error", "file_not_found");
        return result;
    }
    auto found = g_texture_path_to_handle.find(path);
    uint32_t handle = 0;
    if (found != g_texture_path_to_handle.end()) {
        handle = found->second;
    } else {
        handle = g_next_texture_handle++;
        g_texture_path_to_handle[path] = handle;
        g_imported_textures[handle] = path;
    }
    result.Set("success", true);
    result.Set("handle", handle);
    result.Set("path", path);
    return result;
}

Napi::Value ListImportedTextures(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array arr = Napi::Array::New(env);
    uint32_t idx = 0;
    for (const auto& pair : g_imported_textures) {
        Napi::Object item = Napi::Object::New(env);
        item.Set("handle", pair.first);
        item.Set("path", pair.second);
        arr.Set(idx++, item);
    }
    return arr;
}

Napi::Value ApplyTextureToEntity(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber() || !g_world) {
        return Napi::Boolean::New(env, false);
    }
    uint32_t entity_id = info[0].As<Napi::Number>().Uint32Value();
    uint32_t texture_handle = info[1].As<Napi::Number>().Uint32Value();
    if (g_imported_textures.find(texture_handle) == g_imported_textures.end()) {
        return Napi::Boolean::New(env, false);
    }
    Entity entity = ToEntity(entity_id);
    if (!g_world->registry().valid(entity)) {
        return Napi::Boolean::New(env, false);
    }
    if (!g_world->registry().all_of<SpriteRendererComponent>(entity)) {
        g_world->registry().emplace<SpriteRendererComponent>(entity);
    }
    auto& sprite = g_world->registry().get<SpriteRendererComponent>(entity);
    sprite.texture_handle = texture_handle;
    sprite.color = ColorFromTextureHandle(texture_handle);
    return Napi::Boolean::New(env, true);
}

Napi::Value ListShaderVariants(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array arr = Napi::Array::New(env);
    for (uint32_t i = 0; i < g_shader_variants.size(); ++i) {
        arr.Set(i, g_shader_variants[i]);
    }
    return arr;
}

Napi::Value CreateMaterialInstance(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object result = Napi::Object::New(env);
    if (info.Length() < 1 || !info[0].IsString()) {
        result.Set("success", false);
        result.Set("error", "invalid_name");
        return result;
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();
    std::string shader_variant = "SPRITE_UNLIT";
    uint32_t texture_handle = 0;
    if (info.Length() > 1 && info[1].IsString()) {
        shader_variant = info[1].As<Napi::String>().Utf8Value();
    }
    if (info.Length() > 2 && info[2].IsNumber()) {
        texture_handle = info[2].As<Napi::Number>().Uint32Value();
    }
    if (std::find(g_shader_variants.begin(), g_shader_variants.end(), shader_variant) == g_shader_variants.end()) {
        result.Set("success", false);
        result.Set("error", "invalid_shader_variant");
        return result;
    }
    if (texture_handle != 0 && g_imported_textures.find(texture_handle) == g_imported_textures.end()) {
        result.Set("success", false);
        result.Set("error", "texture_not_found");
        return result;
    }

    uint32_t material_id = g_next_material_instance_id++;
    Entity material_entity = g_world->CreateEntity();
    auto& material = g_world->registry().emplace<MaterialInstanceComponent>(material_entity);
    material.material_id = material_id;
    material.name = name;
    material.shader_variant = shader_variant;
    material.blend_mode = shader_variant == "SPRITE_ADDITIVE" ? SpriteBlendMode::Additive : SpriteBlendMode::Alpha;
    material.texture_handle = texture_handle;
    material.tint = glm::vec4(1.0f);
    material.uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    g_material_entities[material_id] = material_entity;
    RecordMaterialHotUpdate(material);

    result.Set("success", true);
    result.Set("id", material_id);
    return result;
}

Napi::Value ListMaterialInstances(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array arr = Napi::Array::New(env);
    uint32_t idx = 0;
    for (const auto& pair : g_material_entities) {
        auto* material = GetMaterialComponent(pair.first);
        if (!material) {
            continue;
        }
        Napi::Object item = Napi::Object::New(env);
        item.Set("id", material->material_id);
        item.Set("name", material->name);
        item.Set("shaderVariant", material->shader_variant);
        item.Set("blendMode", ToBlendModeValue(material->blend_mode));
        item.Set("textureHandle", material->texture_handle);
        Napi::Array tint = Napi::Array::New(env, 4);
        tint.Set(0u, material->tint.r);
        tint.Set(1u, material->tint.g);
        tint.Set(2u, material->tint.b);
        tint.Set(3u, material->tint.a);
        item.Set("tint", tint);
        Napi::Array uv = Napi::Array::New(env, 4);
        uv.Set(0u, material->uv_rect.x);
        uv.Set(1u, material->uv_rect.y);
        uv.Set(2u, material->uv_rect.z);
        uv.Set(3u, material->uv_rect.w);
        item.Set("uv", uv);
        arr.Set(idx++, item);
    }
    return arr;
}

Napi::Value UpdateMaterialInstance(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsObject()) {
        return Napi::Boolean::New(env, false);
    }
    uint32_t material_id = info[0].As<Napi::Number>().Uint32Value();
    auto* material = GetMaterialComponent(material_id);
    if (!material) {
        return Napi::Boolean::New(env, false);
    }
    Napi::Object payload = info[1].As<Napi::Object>();
    if (payload.Has("name") && payload.Get("name").IsString()) {
        material->name = payload.Get("name").As<Napi::String>().Utf8Value();
    }
    if (payload.Has("shaderVariant") && payload.Get("shaderVariant").IsString()) {
        std::string shader_variant = payload.Get("shaderVariant").As<Napi::String>().Utf8Value();
        if (std::find(g_shader_variants.begin(), g_shader_variants.end(), shader_variant) == g_shader_variants.end()) {
            return Napi::Boolean::New(env, false);
        }
        material->shader_variant = shader_variant;
        if (shader_variant == "SPRITE_ADDITIVE") {
            material->blend_mode = SpriteBlendMode::Additive;
        }
    }
    if (payload.Has("blendMode") && payload.Get("blendMode").IsNumber()) {
        uint32_t blend_mode = payload.Get("blendMode").As<Napi::Number>().Uint32Value();
        if (blend_mode > 2) {
            return Napi::Boolean::New(env, false);
        }
        material->blend_mode = ToBlendModeEnum(blend_mode);
    }
    if (payload.Has("textureHandle") && payload.Get("textureHandle").IsNumber()) {
        uint32_t texture_handle = payload.Get("textureHandle").As<Napi::Number>().Uint32Value();
        if (texture_handle != 0 && g_imported_textures.find(texture_handle) == g_imported_textures.end()) {
            return Napi::Boolean::New(env, false);
        }
        material->texture_handle = texture_handle;
    }
    if (payload.Has("tint") && payload.Get("tint").IsArray()) {
        Napi::Array tint = payload.Get("tint").As<Napi::Array>();
        if (tint.Length() == 4) {
            material->tint.r = tint.Get(0u).As<Napi::Number>().FloatValue();
            material->tint.g = tint.Get(1u).As<Napi::Number>().FloatValue();
            material->tint.b = tint.Get(2u).As<Napi::Number>().FloatValue();
            material->tint.a = tint.Get(3u).As<Napi::Number>().FloatValue();
        }
    }
    if (payload.Has("uv") && payload.Get("uv").IsArray()) {
        Napi::Array uv = payload.Get("uv").As<Napi::Array>();
        if (uv.Length() == 4) {
            material->uv_rect.x = uv.Get(0u).As<Napi::Number>().FloatValue();
            material->uv_rect.y = uv.Get(1u).As<Napi::Number>().FloatValue();
            material->uv_rect.z = uv.Get(2u).As<Napi::Number>().FloatValue();
            material->uv_rect.w = uv.Get(3u).As<Napi::Number>().FloatValue();
        }
    }
    RecordMaterialHotUpdate(*material);
    ApplyMaterialToBoundEntities(material_id);
    return Napi::Boolean::New(env, true);
}

Napi::Value ApplyMaterialToEntity(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber() || !g_world) {
        return Napi::Boolean::New(env, false);
    }
    uint32_t entity_id = info[0].As<Napi::Number>().Uint32Value();
    uint32_t material_id = info[1].As<Napi::Number>().Uint32Value();
    auto* material = GetMaterialComponent(material_id);
    if (!material) {
        return Napi::Boolean::New(env, false);
    }
    Entity entity = ToEntity(entity_id);
    if (!g_world->registry().valid(entity)) {
        return Napi::Boolean::New(env, false);
    }
    if (!g_world->registry().all_of<SpriteRendererComponent>(entity)) {
        g_world->registry().emplace<SpriteRendererComponent>(entity);
    }
    auto& sprite = g_world->registry().get<SpriteRendererComponent>(entity);
    sprite.material_instance_id = material->material_id;
    sprite.shader_variant = material->shader_variant;
    sprite.blend_mode = material->blend_mode;
    sprite.texture_handle = material->texture_handle;
    sprite.color = material->tint;
    sprite.uv = material->uv_rect;
    return Napi::Boolean::New(env, true);
}

Napi::Value ListMaterialHotUpdateEvents(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array arr = Napi::Array::New(env);
    for (uint32_t i = 0; i < g_material_hot_update_events.size(); ++i) {
        const auto& event = g_material_hot_update_events[i];
        Napi::Object item = Napi::Object::New(env);
        item.Set("sequence", static_cast<double>(event.sequence));
        item.Set("materialId", event.material_id);
        item.Set("shaderVariant", event.shader_variant);
        item.Set("blendMode", event.blend_mode);
        item.Set("textureHandle", event.texture_handle);
        arr.Set(i, item);
    }
    return arr;
}

Napi::Value ClearMaterialHotUpdateEvents(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    g_material_hot_update_events.clear();
    return Napi::Boolean::New(env, true);
}

Napi::Value ReplayMaterialHotUpdates(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    uint64_t max_sequence = 0;
    if (info.Length() > 0 && info[0].IsNumber()) {
        max_sequence = static_cast<uint64_t>(info[0].As<Napi::Number>().Int64Value());
    }
    g_material_replay_guard = true;
    uint32_t applied = 0;
    for (const auto& event : g_material_hot_update_events) {
        if (max_sequence != 0 && event.sequence > max_sequence) {
            continue;
        }
        auto* material = GetMaterialComponent(event.material_id);
        if (!material) {
            continue;
        }
        material->name = event.name;
        material->shader_variant = event.shader_variant;
        material->blend_mode = ToBlendModeEnum(event.blend_mode);
        material->texture_handle = event.texture_handle;
        material->tint = event.tint;
        material->uv_rect = event.uv_rect;
        ApplyMaterialToBoundEntities(event.material_id);
        ++applied;
    }
    g_material_replay_guard = false;
    Napi::Object result = Napi::Object::New(env);
    result.Set("success", true);
    result.Set("applied", applied);
    return result;
}

Napi::Value GetFrameBridgeStats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object result = Napi::Object::New(env);
    result.Set("copyMs", g_bridge_copy_ms);
    result.Set("latencyMs", g_bridge_latency_ms);
    result.Set("throughputMBps", g_bridge_throughput_mbps);
    result.Set("frameId", g_bridge_last_frame_id);
    result.Set("droppedFrames", g_bridge_dropped_frames);
    result.Set("drawCalls", g_bridge_draw_calls);
    result.Set("maxBatchSprites", g_bridge_max_batch_sprites);
    result.Set("spriteCount", g_bridge_sprite_count);
    result.Set("entityCount", g_bridge_entity_count);
    result.Set("physicsBodies", g_bridge_physics_body_count);
    return result;
}

Napi::Value PushExternalFrame(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 3 || !info[0].IsBuffer() || !info[1].IsNumber() || !info[2].IsNumber()) {
        return Napi::Boolean::New(env, false);
    }

    Napi::Buffer<unsigned char> input = info[0].As<Napi::Buffer<unsigned char>>();
    int width = info[1].As<Napi::Number>().Int32Value();
    int height = info[2].As<Napi::Number>().Int32Value();
    if (width <= 0 || height <= 0) {
        return Napi::Boolean::New(env, false);
    }

    size_t expected_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (input.Length() < expected_size) {
        return Napi::Boolean::New(env, false);
    }

    g_external_frame_buffer.assign(input.Data(), input.Data() + expected_size);
    g_external_frame_width = width;
    g_external_frame_height = height;
    g_use_external_frame = true;
    g_external_frame_source = "engine";
    return Napi::Boolean::New(env, true);
}

Napi::Value ClearExternalFrame(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    g_external_frame_buffer.clear();
    g_external_frame_width = 0;
    g_external_frame_height = 0;
    g_use_external_frame = false;
    g_external_frame_source = "bridge";
    return Napi::Boolean::New(env, true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getVersion"), Napi::Function::New(env, GetEngineVersion));
    exports.Set(Napi::String::New(env, "initEngine"), Napi::Function::New(env, InitEngine));
    exports.Set(Napi::String::New(env, "getFrameBuffer"), Napi::Function::New(env, GetFrameBuffer));
    exports.Set(Napi::String::New(env, "getFrameInfo"), Napi::Function::New(env, GetFrameInfo));
    exports.Set(Napi::String::New(env, "getEntities"), Napi::Function::New(env, GetEntities));
    exports.Set(Napi::String::New(env, "updateEntityTransform"), Napi::Function::New(env, UpdateEntityTransform));
    exports.Set(Napi::String::New(env, "pickEntity"), Napi::Function::New(env, PickEntity));
    exports.Set(Napi::String::New(env, "createEntity"), Napi::Function::New(env, CreateEntity));
    exports.Set(Napi::String::New(env, "deleteEntity"), Napi::Function::New(env, DeleteEntity));
    exports.Set(Napi::String::New(env, "tickEngine"), Napi::Function::New(env, TickEngine));
    exports.Set(Napi::String::New(env, "importTexture"), Napi::Function::New(env, ImportTexture));
    exports.Set(Napi::String::New(env, "listImportedTextures"), Napi::Function::New(env, ListImportedTextures));
    exports.Set(Napi::String::New(env, "applyTextureToEntity"), Napi::Function::New(env, ApplyTextureToEntity));
    exports.Set(Napi::String::New(env, "listShaderVariants"), Napi::Function::New(env, ListShaderVariants));
    exports.Set(Napi::String::New(env, "createMaterialInstance"), Napi::Function::New(env, CreateMaterialInstance));
    exports.Set(Napi::String::New(env, "listMaterialInstances"), Napi::Function::New(env, ListMaterialInstances));
    exports.Set(Napi::String::New(env, "updateMaterialInstance"), Napi::Function::New(env, UpdateMaterialInstance));
    exports.Set(Napi::String::New(env, "applyMaterialToEntity"), Napi::Function::New(env, ApplyMaterialToEntity));
    exports.Set(Napi::String::New(env, "listMaterialHotUpdateEvents"), Napi::Function::New(env, ListMaterialHotUpdateEvents));
    exports.Set(Napi::String::New(env, "clearMaterialHotUpdateEvents"), Napi::Function::New(env, ClearMaterialHotUpdateEvents));
    exports.Set(Napi::String::New(env, "replayMaterialHotUpdates"), Napi::Function::New(env, ReplayMaterialHotUpdates));
    exports.Set(Napi::String::New(env, "getFrameBridgeStats"), Napi::Function::New(env, GetFrameBridgeStats));
    exports.Set(Napi::String::New(env, "pushExternalFrame"), Napi::Function::New(env, PushExternalFrame));
    exports.Set(Napi::String::New(env, "clearExternalFrame"), Napi::Function::New(env, ClearExternalFrame));
    exports.Set(Napi::String::New(env, "buildProject"), Napi::Function::New(env, BuildProject));
    return exports;
}

NODE_API_MODULE(dsengine_bridge, Init)
