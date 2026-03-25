#ifndef DSE_COMPONENTS_2D_H
#define DSE_COMPONENTS_2D_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <entt/entt.hpp>

using Entity = entt::entity;

// We use forward declarations or minimal includes for Box2D to keep headers clean
class b2Body;
class b2Fixture;

class TextureAsset;

// Forward declaration for Box2D
class b2Body;
class b2Fixture;

struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::mat4 local_to_world = glm::mat4(1.0f);
    bool dirty = true;
};

struct ParentComponent {
    Entity parent = entt::null;
};

enum class SpriteBlendMode {
    Alpha = 0,
    Additive = 1,
    Multiply = 2
};

struct MaterialInstanceComponent {
    unsigned int material_id = 0;
    std::string name;
    std::string shader_variant = "SPRITE_UNLIT";
    SpriteBlendMode blend_mode = SpriteBlendMode::Alpha;
    unsigned int texture_handle = 0;
    glm::vec4 tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
};

struct SpriteRendererComponent {
    std::shared_ptr<TextureAsset> texture;
    unsigned int texture_handle = 0;
    unsigned int material_instance_id = 0;
    std::string shader_variant = "SPRITE_UNLIT";
    SpriteBlendMode blend_mode = SpriteBlendMode::Alpha;
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int sorting_layer = 0;
    int order_in_layer = 0;
    bool visible = true;
};

struct UIRendererComponent {
    std::shared_ptr<TextureAsset> texture;
    unsigned int texture_handle = 0;
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int order = 0;
    bool visible = true;
    
    // UI layout params (Anchor & Flex base)
    glm::vec2 position = glm::vec2(0.0f); // Local offset
    glm::vec2 size = glm::vec2(100.0f);
    glm::vec2 anchor_min = glm::vec2(0.5f); // 0-1 percentage of parent
    glm::vec2 anchor_max = glm::vec2(0.5f);
    glm::vec2 pivot = glm::vec2(0.5f);
    
    // UI Event state
    bool interactable = true;
    bool is_hovered = false;
    bool is_pressed = false;

    // Callbacks for Event Bubbling
    std::function<void(Entity)> on_click;
    std::function<void(Entity)> on_pointer_enter;
    std::function<void(Entity)> on_pointer_exit;
    
    // Runtime computed layout
    glm::mat4 runtime_model = glm::mat4(1.0f);
};

struct UIPanelComponent {
    bool blocks_input = false;
};

struct UIButtonComponent {
    glm::vec4 normal_color = glm::vec4(1.0f);
    glm::vec4 hover_color = glm::vec4(1.1f, 1.1f, 1.1f, 1.0f);
    glm::vec4 pressed_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    std::function<void(Entity)> on_click;
    std::function<void(Entity)> on_pointer_enter;
    std::function<void(Entity)> on_pointer_exit;
};

struct UILabelComponent {
    std::string text;
    unsigned int font_texture_handle = 0;
    glm::vec2 glyph_size = glm::vec2(16.0f, 16.0f);
    glm::vec2 offset = glm::vec2(0.0f);
    float spacing = 0.0f;
    int atlas_cols = 16;
    int atlas_rows = 6;
    int ascii_start = 32;
    glm::vec4 color = glm::vec4(1.0f);
    bool dirty = true;
    std::vector<Entity> runtime_glyph_entities;
};

struct CameraComponent {
    bool orthographic = true;
    float orthographic_size = 5.0f;
    float near_clip = -1.0f;
    float far_clip = 1.0f;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

struct ScriptComponent {
    std::string script_path;
    bool enabled = true;
};

enum class RigidBody2DType {
    Static,
    Kinematic,
    Dynamic
};

struct RigidBody2DComponent {
    RigidBody2DType type = RigidBody2DType::Dynamic;
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    float gravity_scale = 1.0f;
    bool fixed_rotation = false;
    
    // Internal Box2D body pointer
    b2Body* runtime_body = nullptr;
    
    // Callbacks for collision events
    std::function<void(Entity other)> on_collision_enter;
    std::function<void(Entity other)> on_collision_exit;
    std::function<void(Entity other)> on_trigger_enter;
    std::function<void(Entity other)> on_trigger_exit;
};

struct BoxCollider2DComponent {
    glm::vec2 size = glm::vec2(1.0f, 1.0f);
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;
    bool is_trigger = false;
    
    // Internal Box2D fixture pointer
    b2Fixture* runtime_fixture = nullptr;
};

// --- New Core Systems Components ---

// --- Animation State Machine ---
struct AnimationState {
    std::string name;
    std::vector<std::shared_ptr<TextureAsset>> frames;
    std::vector<unsigned int> frame_handles; // for lua bindings
    float frame_rate = 10.0f;
    bool loop = true;
};

struct AnimationTransition {
    std::string to_state;
    std::string condition_param; // e.g., "is_walking"
    bool condition_value;        // e.g., true
};

struct AnimatorComponent {
    std::unordered_map<std::string, AnimationState> states;
    std::unordered_map<std::string, std::vector<AnimationTransition>> transitions;
    std::unordered_map<std::string, bool> bool_params;
    std::unordered_map<std::string, float> float_params;

    std::string current_state = "";
    float current_time = 0.0f;
    int current_frame = 0;
    bool playing = true;
    
    // Add helper to set params
    void SetBool(const std::string& name, bool value) { bool_params[name] = value; }
    void SetFloat(const std::string& name, float value) { float_params[name] = value; }
};

struct Particle2D {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;
    float life_time;
    float life_remaining;
    float size;
};

struct ParticleEmitterComponent {
    std::vector<Particle2D> particles;
    std::shared_ptr<TextureAsset> texture;
    unsigned int texture_handle = 0;
    int max_particles = 100;
    float emit_rate = 10.0f; // particles per second
    float emit_accumulator = 0.0f;
    bool emitting = true;
    
    // Emission parameters
    float start_life_time = 2.0f;
    float start_size = 1.0f;
    glm::vec4 start_color = glm::vec4(1.0f);
};

class AudioClipAsset;

struct AudioSourceComponent {
    std::shared_ptr<AudioClipAsset> clip;
    bool play_on_awake = true;
    bool loop = false;
    float volume = 1.0f;
    bool is_playing = false;
    
    // Internal handle to audio engine (e.g., SoLoud)
    unsigned int runtime_handle = 0;
};

struct TilemapComponent {
    std::vector<int> tiles;
    int width = 0;
    int height = 0;
    float tile_size = 1.0f;
    std::shared_ptr<TextureAsset> tileset_texture;
    unsigned int tileset_handle = 0;
    int tileset_cols = 1;
    int tileset_rows = 1;
    int sorting_layer = 0;
    int order_in_layer_base = 0;
    bool generate_colliders = false;
    int collider_tile_min = 1;
    bool dirty = true;
    std::vector<Entity> runtime_tile_entities;
};

// --- Lua Scripting Component ---

struct LuaScriptComponent {
    std::string script_path;
    bool is_initialized = false;
    
    // Sol2 table instance representing the script environment for this entity
    // We use a void pointer or forward declaration here if sol::table is not included
    // to avoid polluting the ECS header with Lua/Sol2 dependencies.
    void* script_instance = nullptr; 
};

#endif
