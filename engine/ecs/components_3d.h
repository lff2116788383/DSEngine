#ifndef DSE_COMPONENTS_3D_H
#define DSE_COMPONENTS_3D_H

#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <vector>
#include "modules/gameplay_3d/animation/animation_state_machine.h"

namespace dse {

struct MeshRendererComponent {
    enum class MaterialDataSource {
        ComponentFallback = 0,
        MaterialInstance = 1
    };

    std::string mesh_path;
    unsigned int material_instance_id = 0;
    std::string shader_variant = "MESH_UNLIT";
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec3 emissive = glm::vec3(0.0f, 0.0f, 0.0f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    float normal_strength = 1.0f;
    float material_alpha_cutoff = 0.5f;
    bool material_alpha_test = false;
    bool material_double_sided = false;
    unsigned int albedo_texture_handle = 0;
    unsigned int normal_texture_handle = 0;
    unsigned int metallic_roughness_texture_handle = 0;
    unsigned int emissive_texture_handle = 0;
    unsigned int occlusion_texture_handle = 0;
    bool receive_shadow = true;
    bool depth_test_enabled = true;
    bool depth_write_enabled = true;
    bool visible = true;
    int sorting_layer = 0;
    int order_in_layer = 0;
    MaterialDataSource material_data_source = MaterialDataSource::ComponentFallback;
    std::vector<float> temp_vertices;
    std::vector<unsigned short> temp_indices;
    std::vector<float> temp_uvs;
    std::vector<float> temp_normals;
    std::vector<float> temp_tangents;
    int dmesh_vertex_stride = 20;  ///< v1=20 (no color), v2=24 (with RGBA color at [20-23])
};

struct BoundingBoxComponent {
    glm::vec3 min_extents = glm::vec3(0.0f);
    glm::vec3 max_extents = glm::vec3(0.0f);
    
    // Calculate center
    glm::vec3 center() const {
        return (min_extents + max_extents) * 0.5f;
    }
    
    // Calculate half extents
    glm::vec3 extents() const {
        return (max_extents - min_extents) * 0.5f;
    }
};

struct Camera3DComponent {
    bool enabled = true;
    int priority = 0;
    float fov = 60.0f;
    float aspect_ratio = 1.333f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

struct PostProcessComponent {
    bool enabled = true;
    
    // Bloom
    bool bloom_enabled = true;
    float bloom_threshold = 1.0f; 
    float bloom_intensity = 0.5f; 
    
    // Color Grading
    bool color_grading_enabled = true;
    float exposure = 1.0f;
    float gamma = 2.2f;
    
    // SSAO
    bool ssao_enabled = false;
    float ssao_radius = 0.5f;
    float ssao_bias = 0.025f;

    // Auto Exposure
    bool auto_exposure_enabled = false;
    float exposure_min = 0.1f;
    float exposure_max = 10.0f;
    float adaptation_speed_up = 2.0f;
    float adaptation_speed_down = 1.0f;
    float exposure_compensation = 0.0f;  // EV offset

    // Color Grading LUT
    unsigned int color_lut_handle = 0;   // 3D LUT texture handle (0 = disabled)
    float color_lut_intensity = 1.0f;    // blend factor (0 = no LUT, 1 = full LUT)

    // FXAA
    bool fxaa_enabled = true;
};

#define CSM_CASCADES 3

struct DirectionalLight3DComponent {
    bool enabled = true;
    glm::vec3 direction = glm::vec3(-0.4f, -1.0f, -0.3f);
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength = 0.35f;
    bool cast_shadow = true;
    
    // CSM (Cascaded Shadow Maps) Settings
    float cascade_splits[CSM_CASCADES] = { 20.0f, 60.0f, 200.0f };
};

struct PointLightComponent {
    bool enabled = true;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float radius = 10.0f; // Attenuation radius
    float falloff = 1.0f;
    bool cast_shadow = false;
    unsigned int shadow_map_handle = 0; // Omnidirectional shadow map
};

struct SpotLightComponent {
    bool enabled = true;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float intensity = 1.0f;
    float radius = 20.0f;
    float falloff = 1.0f;
    float inner_cone_angle = 12.5f; // degrees
    float outer_cone_angle = 17.5f; // degrees
    bool cast_shadow = false;
    unsigned int shadow_map_handle = 0;
};

struct SkyLightComponent {
    bool enabled = true;
    glm::vec3 up_color = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 down_color = glm::vec3(0.0f, 0.0f, 0.5f);
    float intensity = 1.0f;
};

#define MAX_BONE_INFLUENCE 4
#define MAX_BONES 100

struct AnimBlendNode {
    std::string name;
    std::string danim_path;
    float current_time = 0.0f;
    float speed = 1.0f;
    bool loop = true;
    float weight = 1.0f; // For blend tree
    float threshold = 0.0f;
};

struct MorphTarget {
    std::string name;
    float weight = 0.0f; // 0.0 to 1.0
};

struct MorphComponent {
    bool enabled = true;
    std::vector<MorphTarget> targets;
    // GPU resources for morph targets
    unsigned int morph_buffer_handle = 0;
};

struct Animator3DComponent {
    bool enabled = true;
    std::string dskel_path;
    
    // Legacy support (single animation)
    std::string danim_path;
    float current_time = 0.0f;
    float speed = 1.0f;
    bool loop = true;

    // Advanced AnimTree support
    bool use_anim_tree = false;
    std::vector<AnimBlendNode> blend_nodes; // Simple 1D blend for now
    std::string blend_parameter = "speed";
    float blend_parameter_value = 0.0f;
    
    // Animation State Machine support
    std::shared_ptr<gameplay3d::AnimationStateMachine> state_machine;
    std::string current_state_name;
    float state_time = 0.0f;
    float normalized_time = 0.0f;
    
    // Transition crossfade state
    bool is_transitioning = false;
    std::string next_state_name;
    float transition_progress = 0.0f;
    float transition_duration = 0.0f;
    float next_state_time = 0.0f;

    // Root motion lock: when true, the motion-root bone (first child of the
    // skeleton root) has its position locked to the bind pose, preventing
    // animation root motion from visually shifting the mesh.
    bool lock_root_motion = false;

    std::vector<glm::mat4> final_bone_matrices; // Palette uploaded to GPU
};

struct SkyboxComponent {
    bool enabled = true;
    unsigned int cubemap_handle = 0; // The RHI handle for the loaded cubemap texture
    std::string cubemap_path; // Path to load the cubemap from
};

struct FreeCameraControllerComponent {
    bool enabled = true;
    float move_speed = 5.0f;
    float mouse_sensitivity = 0.1f;
    float pitch = 0.0f;
    float yaw = -90.0f;
};

struct TerrainComponent {
    bool enabled = true;
    std::string heightmap_path;
    std::string texture_path;
    unsigned int texture_handle = 0;
    int heightmap_width = 0;
    int heightmap_height = 0;
    int heightmap_channels = 0;
    
    // Terrain parameters
    float width = 100.0f;
    float depth = 100.0f;
    float max_height = 20.0f;
    int resolution_x = 64; // Should be a power of 2 (e.g., 64, 128, 256)
    int resolution_z = 64;
    
    // Advanced LOD parameters (Based on VSEngine2.1 VSQuadTerrainGeometry)
    bool use_dynamic_lod = true;
    int max_lod_levels = 4; // 0 is highest detail, max_lod_levels-1 is lowest
    float lod_distance_factor = 50.0f; // Distance multiplier for LOD switching
    int current_lod = 0; // Updated dynamically by the TerrainSystem
    
    // Frustum Culling visibility
    bool visible = true;
    
    // Splat map: 4-layer texture weights per vertex (R=layer0, G=layer1, B=layer2, A=layer3)
    std::vector<float> splat_data;           // size = resolution_x * resolution_z * 4
    std::string splat_texture_paths[4];      // texture path per layer
    unsigned int splat_texture_handles[4] = {0, 0, 0, 0};
    bool splat_dirty = true;

    // Internal state
    bool is_dirty = true;
    std::vector<float> height_data;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0; // The base EBO (max LOD)
    std::vector<unsigned int> lod_ebos; // EBOs for different LOD levels
    std::vector<unsigned int> lod_index_counts;
    unsigned int index_count = 0;
};

struct SteeringComponent {
    bool enabled = true;
    
    // Steering behaviors
    bool seek_enabled = false;
    glm::vec3 seek_target = glm::vec3(0.0f);
    
    bool flee_enabled = false;
    glm::vec3 flee_target = glm::vec3(0.0f);
    
    bool arrive_enabled = false;
    glm::vec3 arrive_target = glm::vec3(0.0f);
    float arrive_deceleration_radius = 5.0f;
    
    // Physical properties
    float max_velocity = 5.0f;
    float max_force = 10.0f;
    float mass = 1.0f;
    
    // Current state
    glm::vec3 velocity = glm::vec3(0.0f);
};

/// Light Probe: captures indirect diffuse lighting at a point for GI approximation
struct LightProbeComponent {
    bool enabled = true;
    float influence_radius = 10.0f;       ///< Radius of influence (for blending)
    glm::vec3 sh_coefficients[9] = {};    ///< SH L2 coefficients (9 vec3 for RGB)
    bool needs_rebake = true;             ///< Flag for system to rebake
    bool show_debug = true;               ///< Show debug sphere in editor
};

/// Reflection Probe: captures environment reflections as a cubemap
struct ReflectionProbeComponent {
    bool enabled = true;
    float influence_radius = 15.0f;       ///< Blend distance
    float box_size_x = 10.0f;            ///< Box projection extents
    float box_size_y = 10.0f;
    float box_size_z = 10.0f;
    bool use_box_projection = false;
    int resolution = 128;                 ///< Cubemap face resolution
    unsigned int cubemap_handle = 0;      ///< GPU cubemap texture
    bool needs_rebake = true;
    bool show_debug = true;               ///< Show debug wireframe in editor
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_H
