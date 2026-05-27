#ifndef DSE_COMPONENTS_3D_ANIMATION_H
#define DSE_COMPONENTS_3D_ANIMATION_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <entt/entity/entity.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse { namespace gameplay3d { class AnimationStateMachine; } }

namespace dse {

constexpr int MAX_BONE_INFLUENCE = 4;
constexpr int MAX_BONES = 100;

struct FootIKConfig {
    std::string name;
    std::string foot_bone;
    std::string hip_bone;
    float foot_height = 0.1f;
    float max_ground_distance = 0.5f;
    float blend_speed = 10.0f;
    float weight = 1.0f;
    int foot_bone_index = -1;
    int hip_bone_index = -1;
    std::vector<int> chain_indices;  // From hip to foot
    bool indices_dirty = true;
};

struct AnimBlendNode {
    std::string name;
    std::string danim_path;
    float current_time = 0.0f;
    float speed = 1.0f;
    bool loop = true;
    float x = 0.0f;
    float threshold = 0.0f;  // 1D Blend threshold
    float weight = 1.0f;     // Blend weight
};

struct AnimBlendNode2D {
    std::string name;
    std::string danim_path;
    float current_time = 0.0f;
    float speed = 1.0f;
    bool loop = true;
    float x = 0.0f;
    float y = 0.0f;
};

enum class AnimLayerBlendMode : uint8_t {
    Override = 0,
    Additive = 1,
};

enum class AnimSourceType : uint8_t {
    SingleClip = 0,
    BlendTree1D = 1,
    BlendTree2D = 2,
};

struct AnimEventConfig {
    std::string name;
    float trigger_time = 0.0f;
    bool fired = false;
};

struct AnimLayerConfig {
    std::string name;
    float weight = 1.0f;
    AnimLayerBlendMode blend_mode = AnimLayerBlendMode::Override;
    AnimSourceType source_type = AnimSourceType::SingleClip;

    std::vector<std::string> bone_mask_include;
    std::vector<int> bone_mask_indices;
    bool bone_mask_dirty = true;

    std::string danim_path;
    float current_time = 0.0f;
    float speed = 1.0f;
    bool loop = true;

    std::vector<AnimBlendNode> blend_nodes;
    std::string blend_parameter = "speed";
    float blend_parameter_value = 0.0f;

    std::vector<AnimBlendNode2D> blend_nodes_2d;
    glm::vec2 blend_parameter_2d = glm::vec2(0.0f);
};

struct AnimLayerComponent {
    bool enabled = true;
    std::vector<AnimLayerConfig> layers;
};

enum class IKChainType : uint8_t {
    FABRIK = 0,
    LookAt = 1,
    CCD = 2,
    Jacobian = 3,
};

struct IKChainConfig {
    std::string name;
    IKChainType type = IKChainType::FABRIK;
    std::string root_bone;
    std::string tip_bone;
    float weight = 1.0f;

    uint32_t target_entity = UINT32_MAX;
    glm::vec3 target_position = glm::vec3(0.0f);

    glm::vec3 pole_vector = glm::vec3(0.0f, 0.0f, -1.0f);
    int iterations = 10;
    float tolerance = 0.01f;

    // Runtime cached indices (resolved on first use)
    int root_bone_index = -1;
    int tip_bone_index = -1;
    std::vector<int> chain_indices;
    bool indices_dirty = true;
};

struct IKChain3DComponent {
    bool enabled = true;
    std::vector<IKChainConfig> chains;
};

struct FootIK3DComponent {
    bool enabled = true;
    std::vector<FootIKConfig> feet;  // 左右脚配置
    float pelvis_weight = 0.5f;       // 骨盆调整权重
    float max_pelvis_offset = 0.3f;   // 骨盆最大偏移量
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

    /// Per-vertex morph delta data (CPU 侧，供 GPU Compute Skinning 使用)
    /// 布局: [target0: v0(xyz0), v1(xyz0), ...], [target1: v0(xyz0), v1(xyz0), ...], ...
    /// 每个 delta 占 4 float (vec4, w=0)，总长 = morph_target_count * vertex_count * 4
    /// 由 mesh loader 填充。为空时 GPU 蒙皮跳过 morph blending。
    std::vector<float> morph_delta_vertices;
    uint32_t morph_vertex_count = 0;  ///< 每个 target 的顶点数
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

    // Root motion extraction: when true, EvaluateBaseAnim computes per-frame
    // Hips delta for gameplay to consume (e.g. CharacterController movement).
    bool extract_root_motion = false;
    glm::vec3 root_motion_delta = glm::vec3(0.0f);
    glm::quat root_motion_rotation_delta = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 prev_root_position_ = glm::vec3(0.0f);
    glm::quat prev_root_rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // Animation events
    std::vector<AnimEventConfig> events;
    std::vector<std::string> fired_events;
    float prev_event_time_ = 0.0f;

    // --- Runtime pose buffer (written by EvaluateBaseAnim, modified by LayerBlend, read by ComputeFinalMatrices) ---
    struct PoseBuffer {
        std::vector<glm::vec3> positions;
        std::vector<glm::quat> rotations;
        std::vector<glm::vec3> scales;
        std::vector<bool> touched;

        void Resize(uint32_t bone_count) {
            positions.resize(bone_count, glm::vec3(0.0f));
            rotations.resize(bone_count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            scales.resize(bone_count, glm::vec3(1.0f));
            touched.resize(bone_count, false);
        }
        void Reset(uint32_t bone_count) {
            Resize(bone_count);
            std::fill(positions.begin(), positions.end(), glm::vec3(0.0f));
            std::fill(rotations.begin(), rotations.end(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            std::fill(scales.begin(), scales.end(), glm::vec3(1.0f));
            std::fill(touched.begin(), touched.end(), false);
        }
        uint32_t Size() const { return static_cast<uint32_t>(positions.size()); }
    };
    PoseBuffer pose_buffer;

    // --- Skeletal cache (rebuilt when dskel_path changes) ---
    struct SkeletalCache {
        std::string cached_dskel_path;
        uint32_t bone_count = 0;
        std::vector<glm::mat4> bind_globals;
        std::vector<glm::mat4> inv_bind_globals;
        std::vector<glm::mat4> local_bind_poses;
        std::vector<int> parent_indices;
        std::vector<uint32_t> topo_order; // 拓扑排序：single-pass global transform propagation
        std::unordered_map<std::string, int> bone_name_to_index;
        bool valid = false;
    };
    SkeletalCache skel_cache;

    std::vector<glm::mat4> final_bone_matrices; // Palette uploaded to GPU

    /// Bone palette key: AnimatorSystem 计算的动画状态哈希
    /// 相同 key 的实例共享同一组骨骼矩阵（用于 instancing 去重）
    uint64_t bone_palette_key = 0;

    /// 缓存上次评估的动画时长（供 palette 去重路径推进时间）
    float cached_duration_ = 0.0f;

    /// 动画 LOD: 帧计数器，用于跳帧更新
    uint8_t anim_lod_skip_ = 0;      // 0=每帧, 1=每2帧, 3=每4帧
    uint8_t anim_lod_counter_ = 0;   // 当前帧计数
};

/// 骨骼挂点组件（Paper Doll / Socket）
/// 将本实体附着到另一个拥有 Animator3DComponent 的实体的指定骨骼上。
/// BoneAttachmentSystem 每帧从 final_bone_matrices 恢复骨骼全局矩阵并应用偏移。
struct BoneAttachmentComponent {
    entt::entity target_entity{entt::null};                   ///< 拥有骨骼动画的目标实体
    std::string bone_name;                                    ///< 挂载骨骼名称
    glm::vec3 offset_position = glm::vec3(0.0f);             ///< 相对骨骼的位置偏移
    glm::quat offset_rotation = glm::quat(1, 0, 0, 0);      ///< 相对骨骼的旋转偏移
    glm::vec3 offset_scale    = glm::vec3(1.0f);             ///< 相对骨骼的缩放偏移

    // Runtime（BoneAttachmentSystem 管理）
    int cached_bone_index = -1;
    bool index_dirty = true;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_ANIMATION_H
