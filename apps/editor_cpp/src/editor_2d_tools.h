#pragma once

/**
 * @file editor_2d_tools.h
 * @brief 2D Editor Tools - Sprite Slicer, Atlas Packer, Frame Animation,
 *        9-Slice, Collision Shape, 2D Particles, Parallax, 2D Lighting
 */

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "imgui.h"

namespace dse::editor::tools2d {

// ═══════════════════════════════════════════════════════════════════════════
// #1 — Sprite Sheet Slicer
// ═══════════════════════════════════════════════════════════════════════════

enum class SliceMode {
    Grid,       // Fixed grid (cell_width x cell_height)
    Auto,       // Alpha-based boundary detection
    Manual      // User draws rects
};

struct SpriteFrame {
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;  // pixel coords in source image
    glm::vec2 pivot = {0.5f, 0.5f};   // normalized pivot point
};

struct SpriteSheetAsset {
    std::string source_texture_path;
    int texture_width = 0;
    int texture_height = 0;
    std::vector<SpriteFrame> frames;
    std::string name;
};

struct SpriteSlicerState {
    bool open = false;
    SliceMode mode = SliceMode::Grid;
    int cell_width = 64;
    int cell_height = 64;
    int padding = 0;
    int offset_x = 0;
    int offset_y = 0;
    float alpha_threshold = 0.1f;  // for auto mode
    SpriteSheetAsset current_sheet;
    int selected_frame = -1;
    float zoom = 1.0f;
    ImVec2 scroll_offset = {0, 0};
    bool preview_dirty = true;
};

SpriteSlicerState& GetSpriteSlicerState();
void DrawSpriteSlicerPanel();
void SliceGrid(SpriteSheetAsset& sheet, int cell_w, int cell_h, int pad, int ox, int oy);
void SliceAuto(SpriteSheetAsset& sheet, float alpha_threshold);
bool SaveSpriteSheet(const SpriteSheetAsset& sheet, const std::string& path);
bool LoadSpriteSheet(SpriteSheetAsset& sheet, const std::string& path);

// Test accessors
int SpriteSlicerFrameCount();
const char* SpriteSlicerModeName();

// ═══════════════════════════════════════════════════════════════════════════
// #2 — Sprite Atlas Packer
// ═══════════════════════════════════════════════════════════════════════════

struct AtlasEntry {
    std::string source_path;
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;  // packed position in atlas
};

struct AtlasAsset {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<AtlasEntry> entries;
    int max_size = 4096;
    int padding = 2;
    bool power_of_two = true;
};

struct AtlasPackerState {
    bool open = false;
    AtlasAsset current_atlas;
    std::vector<std::string> input_paths;
    bool pack_dirty = true;
};

AtlasPackerState& GetAtlasPackerState();
void DrawAtlasPackerPanel();
bool PackAtlas(AtlasAsset& atlas, const std::vector<std::string>& input_paths);
bool SaveAtlas(const AtlasAsset& atlas, const std::string& path);

// Test accessors
int AtlasEntryCount();

// ═══════════════════════════════════════════════════════════════════════════
// #3 — 2D Frame Animation Editor
// ═══════════════════════════════════════════════════════════════════════════

struct AnimFrame2D {
    int frame_index = 0;         // index into SpriteSheetAsset::frames
    float duration = 0.1f;       // seconds
    glm::vec2 offset = {0, 0};  // per-frame offset
};

struct Animation2DClip {
    std::string name;
    std::vector<AnimFrame2D> frames;
    bool loop = true;
    float total_duration = 0.0f;
};

struct Animation2DAsset {
    std::string name;
    std::string sprite_sheet_path;
    std::vector<Animation2DClip> clips;
};

struct Anim2DEditorState {
    bool open = false;
    Animation2DAsset current_anim;
    int selected_clip = 0;
    int selected_frame = -1;
    bool playing = false;
    float playback_time = 0.0f;
    float playback_speed = 1.0f;
    int preview_frame = 0;
};

Anim2DEditorState& GetAnim2DEditorState();
void DrawAnim2DEditorPanel();
void Anim2DPlay();
void Anim2DStop();
void Anim2DAddFrame(int clip_index, int frame_index, float duration);
bool SaveAnimation2D(const Animation2DAsset& asset, const std::string& path);

// Test accessors
int Anim2DClipCount();
int Anim2DFrameCount(int clip_index);
bool Anim2DIsPlaying();

// ═══════════════════════════════════════════════════════════════════════════
// #4 — 9-Slice / 9-Patch Editor
// ═══════════════════════════════════════════════════════════════════════════

struct NineSliceData {
    std::string texture_path;
    int left = 0, right = 0, top = 0, bottom = 0;  // border sizes in pixels
    int tex_width = 0, tex_height = 0;
};

struct NineSliceEditorState {
    bool open = false;
    NineSliceData current;
    float zoom = 2.0f;
    bool show_guides = true;
    ImVec2 preview_size = {200, 200};  // stretched preview target
};

NineSliceEditorState& GetNineSliceEditorState();
void DrawNineSliceEditorPanel();
bool SaveNineSlice(const NineSliceData& data, const std::string& path);

// Test accessors
bool NineSliceHasValidBorders();

// ═══════════════════════════════════════════════════════════════════════════
// #5 — 2D Collision Shape Editor
// ═══════════════════════════════════════════════════════════════════════════

enum class Shape2DType {
    Box,
    Circle,
    Polygon,
    Edge,
    Capsule
};

struct CollisionShape2D {
    Shape2DType type = Shape2DType::Box;
    glm::vec2 offset = {0, 0};
    float rotation = 0.0f;
    // Box
    glm::vec2 half_extents = {0.5f, 0.5f};
    // Circle / Capsule
    float radius = 0.5f;
    float capsule_height = 1.0f;
    // Polygon / Edge
    std::vector<glm::vec2> vertices;
    bool is_trigger = false;
};

struct CollisionEditor2DState {
    bool open = false;
    std::vector<CollisionShape2D> shapes;
    int selected_shape = -1;
    int dragging_vertex = -1;
    Shape2DType create_type = Shape2DType::Box;
    bool editing = false;
    float grid_size = 0.25f;
    bool snap_to_grid = true;
};

CollisionEditor2DState& GetCollisionEditor2DState();
void DrawCollisionEditor2DPanel();
void DrawCollisionShapes2DOverlay(ImDrawList* draw_list, ImVec2 origin, float scale);
void AddCollisionShape2D(Shape2DType type);

// Test accessors
int CollisionShape2DCount();

// ═══════════════════════════════════════════════════════════════════════════
// #6 — 2D Particle Editor
// ═══════════════════════════════════════════════════════════════════════════

enum class Particle2DEmitShape {
    Point,
    Circle,
    Rectangle,
    Line
};

enum class Particle2DBlendMode {
    Alpha,
    Additive,
    Multiply
};

struct Particle2DConfig {
    std::string name = "New Particle";
    // Emission
    float emit_rate = 50.0f;
    int max_particles = 500;
    Particle2DEmitShape emit_shape = Particle2DEmitShape::Point;
    float emit_radius = 0.0f;
    glm::vec2 emit_rect = {1, 1};
    // Lifetime
    float lifetime_min = 0.5f;
    float lifetime_max = 2.0f;
    // Motion
    glm::vec2 velocity_min = {-50, -100};
    glm::vec2 velocity_max = {50, -200};
    glm::vec2 gravity = {0, 200};
    float angular_velocity_min = -90.0f;
    float angular_velocity_max = 90.0f;
    float damping = 0.0f;
    // Appearance
    float start_size_min = 8.0f;
    float start_size_max = 16.0f;
    float end_size = 2.0f;
    glm::vec4 start_color = {1, 1, 1, 1};
    glm::vec4 end_color = {1, 1, 1, 0};
    Particle2DBlendMode blend_mode = Particle2DBlendMode::Additive;
    std::string texture_path;
    // Trail
    bool trail_enabled = false;
    int trail_length = 5;
    float trail_width = 2.0f;
};

struct Particle2DEditorState {
    bool open = false;
    Particle2DConfig config;
    bool simulating = false;
    float sim_time = 0.0f;
    int active_particles = 0;
};

Particle2DEditorState& GetParticle2DEditorState();
void DrawParticle2DEditorPanel();
bool SaveParticle2DConfig(const Particle2DConfig& cfg, const std::string& path);

// Test accessors
bool Particle2DSimulating();
int Particle2DActiveCount();

// ═══════════════════════════════════════════════════════════════════════════
// #7 — Parallax Layer Editor
// ═══════════════════════════════════════════════════════════════════════════

struct ParallaxLayer {
    std::string name;
    std::string texture_path;
    float scroll_factor_x = 1.0f;
    float scroll_factor_y = 1.0f;
    float offset_y = 0.0f;
    bool repeat_x = true;
    bool repeat_y = false;
    int sort_order = 0;
    float opacity = 1.0f;
    glm::vec4 tint = {1, 1, 1, 1};
};

struct ParallaxConfig {
    std::string name;
    std::vector<ParallaxLayer> layers;
    float base_speed = 1.0f;
};

struct ParallaxEditorState {
    bool open = false;
    ParallaxConfig config;
    int selected_layer = -1;
    float preview_scroll = 0.0f;
    bool preview_playing = false;
};

ParallaxEditorState& GetParallaxEditorState();
void DrawParallaxEditorPanel();
void AddParallaxLayer(const std::string& name);
bool SaveParallaxConfig(const ParallaxConfig& cfg, const std::string& path);

// Test accessors
int ParallaxLayerCount();

// ═══════════════════════════════════════════════════════════════════════════
// #8 — 2D Lighting Editor
// ═══════════════════════════════════════════════════════════════════════════

enum class Light2DType {
    Point,
    Spot,
    Directional
};

enum class Light2DShadowMode {
    None,
    Hard,
    Soft
};

struct Light2DConfig {
    std::string name = "Light2D";
    Light2DType type = Light2DType::Point;
    glm::vec2 position = {0, 0};
    glm::vec3 color = {1, 1, 1};
    float intensity = 1.0f;
    float range = 200.0f;
    float falloff = 2.0f;
    // Spot
    float spot_angle = 45.0f;
    float spot_direction = 0.0f;
    // Shadows
    Light2DShadowMode shadow_mode = Light2DShadowMode::None;
    float shadow_softness = 1.0f;
    int shadow_rays = 64;
    // Normal map
    bool use_normal_map = false;
    float normal_strength = 1.0f;
};

struct Light2DEditorState {
    bool open = false;
    std::vector<Light2DConfig> lights;
    int selected_light = -1;
    glm::vec3 ambient_color = {0.1f, 0.1f, 0.15f};
    float ambient_intensity = 0.3f;
    bool show_gizmos = true;
    bool show_shadow_preview = false;
};

Light2DEditorState& GetLight2DEditorState();
void DrawLight2DEditorPanel();
void DrawLight2DGizmos(ImDrawList* draw_list, ImVec2 origin, float scale);
void AddLight2D(Light2DType type);
bool SaveLight2DScene(const Light2DEditorState& state, const std::string& path);

// Test accessors
int Light2DCount();

}  // namespace dse::editor::tools2d
