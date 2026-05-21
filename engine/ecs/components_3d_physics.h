#ifndef DSE_COMPONENTS_3D_PHYSICS_H
#define DSE_COMPONENTS_3D_PHYSICS_H

#include <glm/glm.hpp>
#include <entt/entity/entity.hpp>
#include <vector>
#include <string>

namespace dse {

enum class RigidBody3DType {
    Static = 0,
    Kinematic = 1,
    Dynamic = 2
};

struct RigidBody3DComponent {
    RigidBody3DType type = RigidBody3DType::Dynamic;
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 angular_velocity = glm::vec3(0.0f);
    float mass = 1.0f;
    float drag = 0.0f;
    float angular_drag = 0.05f;
    bool use_gravity = true;
    float gravity_scale = 1.0f;
    bool is_kinematic = false;

    // Collision filtering (Task 6)
    uint16_t collision_layer = 0x0001;  ///< 所属碰撞层
    uint16_t collision_mask  = 0xFFFF;  ///< 可碰撞的层掩码

    // Deferred impulse: applied once when the PhysX actor is first created
    glm::vec3 pending_impulse = glm::vec3(0.0f);
    bool has_pending_impulse = false;

    // Backend handle
    void* runtime_body = nullptr;
};

struct BoxCollider3DComponent {
    glm::vec3 size = glm::vec3(1.0f);
    glm::vec3 center = glm::vec3(0.0f);
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Dirty tracking (Task 7)
    glm::vec3 prev_size = glm::vec3(-1.0f);
    float prev_bounciness = -1.0f;
    float prev_friction = -1.0f;

    // Backend handle
    void* runtime_shape = nullptr;
};

struct SphereCollider3DComponent {
    float radius = 0.5f;
    glm::vec3 center = glm::vec3(0.0f);
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Dirty tracking (Task 7)
    float prev_radius = -1.0f;
    float prev_bounciness = -1.0f;
    float prev_friction = -1.0f;

    // Backend handle
    void* runtime_shape = nullptr;
};

struct MeshCollider3DComponent {
    bool convex = false;
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Backend handle
    void* runtime_shape = nullptr;

    // 动态更新：记录上次的 mesh_path
    std::string prev_mesh_path;
};

/// 胶囊碰撞体组件（Task 4）
struct CapsuleCollider3DComponent {
    float radius = 0.5f;        ///< 胶囊半径
    float height = 1.0f;        ///< 胶囊高度（不含半球）
    glm::vec3 center = glm::vec3(0.0f);
    int direction = 1;          ///< 轴向: 0=X, 1=Y, 2=Z
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Dirty tracking (Task 7)
    float prev_radius = -1.0f;
    float prev_height = -1.0f;
    float prev_bounciness = -1.0f;
    float prev_friction = -1.0f;

    // Backend handle
    void* runtime_shape = nullptr;
};

/// 物理关节类型（Task 5）
enum class Joint3DType {
    Fixed = 0,
    Hinge = 1,     ///< 铰链关节（PxRevoluteJoint）
    Spring = 2,    ///< 弹簧关节（PxD6Joint + drive）
    Distance = 3   ///< 距离关节（PxDistanceJoint）
};

/// 物理关节组件（Task 5）
struct Joint3DComponent {
    Joint3DType type = Joint3DType::Fixed;
    uint32_t connected_entity_id = 0;  ///< 连接的另一个实体 ID（0=世界锚点）
    glm::vec3 anchor = glm::vec3(0.0f);         ///< 本体锚点（局部坐标）
    glm::vec3 connected_anchor = glm::vec3(0.0f); ///< 对方锚点（局部坐标）
    glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f); ///< 关节轴向

    // Hinge 参数
    bool use_limits = false;
    float lower_limit = -45.0f;   ///< 最小角度（度）
    float upper_limit = 45.0f;    ///< 最大角度（度）

    // Distance 参数
    float min_distance = 0.0f;
    float max_distance = 10.0f;

    // Spring 参数
    float spring_stiffness = 100.0f;
    float spring_damping = 10.0f;

    // 断裂
    float break_force = FLT_MAX;   ///< 断裂力（FLT_MAX=不可断裂）
    float break_torque = FLT_MAX;  ///< 断裂扭矩
    bool is_broken = false;

    // Backend handle
    void* runtime_joint = nullptr;
};

/// 角色控制器碰撞标志位（对应 PhysX PxControllerCollisionFlag）
enum class CharacterCollisionFlag : uint8_t {
    None    = 0,
    Sides   = 1 << 0,   ///< 与侧面碰撞
    Up      = 1 << 1,   ///< 与上方碰撞
    Down    = 1 << 2,   ///< 与下方碰撞（着地）
};

inline CharacterCollisionFlag operator|(CharacterCollisionFlag a, CharacterCollisionFlag b) {
    return static_cast<CharacterCollisionFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(CharacterCollisionFlag a, CharacterCollisionFlag b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/// 角色控制器组件 — 基于 kinematic PxRigidDynamic + sweep 的运动学角色控制
struct CharacterController3DComponent {
    float radius = 0.3f;            ///< 胶囊半径
    float height = 1.0f;            ///< 胶囊高度（不含半球）
    float slope_limit = 45.0f;      ///< 不可行走坡度限制（度）
    float step_offset = 0.3f;       ///< 台阶高度偏移
    float skin_width = 0.01f;       ///< 碰撞皮肤宽度
    float min_move_distance = 0.0f; ///< 最小移动距离阈值

    // 运行时状态
    bool is_grounded = false;       ///< 是否着地
    glm::vec3 velocity = glm::vec3(0.0f);      ///< 当前速度
    CharacterCollisionFlag collision_flags = CharacterCollisionFlag::None;  ///< 碰撞标志

    // Backend handle — PxRigidDynamic*（kinematic 角色代理）
    void* runtime_controller = nullptr;
};

/// 地形高度图组件 — 存储网格化高度数据，提供 C++ 双线性插值查询
struct TerrainHeightmapComponent {
    float origin_x = 0.0f;     ///< 网格原点 X（本地单位）
    float origin_z = 0.0f;     ///< 网格原点 Z（本地单位）
    float block_size = 1.0f;   ///< 网格单元大小（本地单位）
    float scale = 1.0f;        ///< 本地 → 世界缩放因子
    bool flip_z = false;       ///< 若 true，世界 Z = -本地 Z
    int cols = 0;              ///< 列数
    int rows = 0;              ///< 行数
    std::vector<float> heights; ///< 行主序高度数据（rows × cols，本地 Y 值）

    /// 查询世界坐标处的地形高度（返回世界 Y）
    float GetHeight(float world_x, float world_z) const {
        if (cols <= 1 || rows <= 1 || static_cast<int>(heights.size()) < cols * rows)
            return 0.0f;

        float local_x = world_x / scale;
        float local_z = flip_z ? (-world_z / scale) : (world_z / scale);

        float gx = (local_x - origin_x) / block_size;
        float gz = (origin_z - local_z) / block_size;

        float max_col = static_cast<float>(cols - 1);
        float max_row = static_cast<float>(rows - 1);
        if (gx < 0.0f) gx = 0.0f; if (gx > max_col) gx = max_col;
        if (gz < 0.0f) gz = 0.0f; if (gz > max_row) gz = max_row;

        int ix = static_cast<int>(gx);
        int iz = static_cast<int>(gz);
        float fx = gx - static_cast<float>(ix);
        float fz = gz - static_cast<float>(iz);

        if (ix >= cols - 1) { ix = cols - 2; fx = 1.0f; }
        if (iz >= rows - 1) { iz = rows - 2; fz = 1.0f; }

        float h00 = heights[iz * cols + ix];
        float h10 = heights[iz * cols + ix + 1];
        float h01 = heights[(iz + 1) * cols + ix];
        float h11 = heights[(iz + 1) * cols + ix + 1];

        float h = h00 * (1.0f - fx) * (1.0f - fz)
                + h10 * fx * (1.0f - fz)
                + h01 * (1.0f - fx) * fz
                + h11 * fx * fz;

        return h * scale;
    }
};

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
// ============================================================
// Ragdoll 布娃娃（Phase 2 — Task 1）
// ============================================================

/// 布娃娃单骨骼配置
struct RagdollBoneSetup {
    int bone_index = -1;           ///< 骨骼索引（对应 dskel）
    int parent_setup_index = -1;   ///< 父配置索引（-1=根，指向 bone_setups 数组下标）
    float radius = 0.05f;          ///< 胶囊半径
    float height = 0.1f;           ///< 胶囊高度（不含半球）
    float mass = 1.0f;             ///< 质量
    glm::vec3 offset = glm::vec3(0.0f); ///< 相对骨骼的偏移

    // D6 关节限制
    float swing_limit_y = 30.0f;   ///< Y轴摆动限制（度）
    float swing_limit_z = 30.0f;   ///< Z轴摆动限制（度）
    float twist_limit = 20.0f;     ///< 扭转限制（度）
};

/// 布娃娃运行时骨骼
struct RagdollRuntimeBone {
    void* actor = nullptr;         ///< PhysX: PxRigidDynamic*
    void* joint = nullptr;         ///< PhysX: PxD6Joint*
    entt::entity bone_entity = entt::null; ///< Jolt: ECS 骨骼刚体 entity
    int bone_index = -1;
};

/// 布娃娃组件
struct RagdollComponent {
    bool active = false;           ///< 是否已激活（物理驱动）
    bool auto_setup = true;        ///< 自动从骨骼层级生成配置
    float total_mass = 10.0f;      ///< 总质量（auto_setup 时按骨骼长度分配）
    float joint_stiffness = 0.0f;  ///< 关节弹簧刚度（0=自由摆动）
    float joint_damping = 50.0f;   ///< 关节阻尼

    uint16_t collision_layer = 0x0002;  ///< 布娃娃碰撞层
    uint16_t collision_mask  = 0xFFFF;

    std::vector<RagdollBoneSetup> bone_setups;
    std::vector<RagdollRuntimeBone> runtime_bones;
    bool initialized = false;
};

#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT

// ============================================================
// SoftBody 软体模拟（Phase 2 — Task 2）
// ============================================================

/// PBD 距离约束
struct SoftBodyDistConstraint {
    uint32_t i0 = 0, i1 = 0;
    float rest_length = 0.0f;
};

/// 软体组件
struct SoftBodyComponent {
    bool enabled = true;
    float stiffness = 0.5f;        ///< 约束刚度 [0,1]
    int solver_iterations = 4;     ///< PBD 投影迭代次数
    float damping = 0.99f;         ///< 速度阻尼
    bool use_gravity = true;
    float gravity_scale = 1.0f;

    // 粒子数据（从 mesh 顶点初始化）
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prev_positions;
    std::vector<glm::vec3> velocities;
    std::vector<float> inv_masses;   ///< 0 = 固定点

    // 约束
    std::vector<SoftBodyDistConstraint> constraints;
    float rest_volume = 0.0f;       ///< 初始体积（用于体积保持）
    float volume_stiffness = 0.5f;  ///< 体积保持刚度

    bool initialized = false;
    bool mesh_dirty = false;         ///< 需要回写 mesh 顶点
};

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
// ============================================================
// Vehicle 车辆物理（Phase 2 — Task 3）
// ============================================================

/// 车轮配置
struct VehicleWheelConfig {
    glm::vec3 position = glm::vec3(0.0f);  ///< 相对车体的位置
    float radius = 0.3f;
    float suspension_rest_length = 0.3f;
    float suspension_stiffness = 30000.0f;
    float suspension_damping = 4500.0f;
    float friction = 1.5f;
    bool is_drive_wheel = true;
    bool is_steer_wheel = false;
};

/// 车轮运行时状态
struct VehicleWheelState {
    float compression = 0.0f;          ///< 悬挂压缩量
    float angular_velocity = 0.0f;     ///< 车轮角速度
    float rotation = 0.0f;             ///< 累积旋转角度
    float steer_angle = 0.0f;          ///< 当前转向角度
    bool grounded = false;
    glm::vec3 contact_point = glm::vec3(0.0f);
    glm::vec3 contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
};

/// 车辆组件（Raycast 车辆，不依赖 PhysXVehicle）
struct VehicleComponent {
    bool enabled = true;
    float max_engine_force = 5000.0f;
    float max_brake_force = 3000.0f;
    float max_steer_angle = 35.0f;     ///< 最大转向角（度）

    // 输入
    float throttle = 0.0f;   ///< [-1, 1] 油门/倒车
    float brake = 0.0f;      ///< [0, 1] 刹车
    float steering = 0.0f;   ///< [-1, 1] 转向

    std::vector<VehicleWheelConfig> wheels;
    std::vector<VehicleWheelState> wheel_states;

    float current_speed = 0.0f;  ///< 当前速度 (m/s)
    bool initialized = false;
};

#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT

// ============================================================
// Rope 绳索/链条（Phase 2 — Task 4）
// ============================================================

/// 绳索组件（Verlet 积分）
struct RopeComponent {
    bool enabled = true;
    int segment_count = 10;        ///< 段数
    float segment_length = 0.2f;   ///< 每段长度
    float radius = 0.02f;          ///< 碰撞/渲染半径
    float damping = 0.99f;         ///< 速度阻尼
    int solver_iterations = 8;     ///< 约束求解迭代
    bool use_gravity = true;
    float gravity_scale = 1.0f;

    uint32_t anchor_entity_a = 0;  ///< 锚点实体A（0=世界固定点）
    uint32_t anchor_entity_b = 0;  ///< 锚点实体B（0=自由端）
    glm::vec3 anchor_offset_a = glm::vec3(0.0f);
    glm::vec3 anchor_offset_b = glm::vec3(0.0f);
    glm::vec3 start_position = glm::vec3(0.0f);  ///< 起始位置（无锚点实体时）

    // Verlet 粒子数据
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prev_positions;

    bool initialized = false;
};

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
// ============================================================
// Buoyancy 浮力模拟（Phase 2 — Task 5）
// ============================================================

/// 浮力采样点
struct BuoyancySamplePoint {
    glm::vec3 offset = glm::vec3(0.0f);  ///< 相对实体中心的偏移
    float force_scale = 1.0f;
};

/// 浮力组件
struct BuoyancyComponent {
    bool enabled = true;
    float water_level = 0.0f;          ///< 全局水面高度（无流体实体时使用）
    bool use_fluid_system = true;      ///< 尝试从 FluidEmitter 获取水面
    float buoyancy_force = 10.0f;      ///< 浮力系数
    float water_drag = 3.0f;           ///< 线性水阻力
    float water_angular_drag = 1.0f;   ///< 角水阻力
    float submerge_depth = 1.0f;       ///< 完全淹没所需深度

    std::vector<BuoyancySamplePoint> sample_points;

    // 运行时
    float submerge_ratio = 0.0f;       ///< 当前淹没比例 [0,1]
};

#endif // DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT

} // namespace dse

#endif // DSE_COMPONENTS_3D_PHYSICS_H