#ifndef DSE_COMPONENTS_3D_PHYSICS_H
#define DSE_COMPONENTS_3D_PHYSICS_H

#include <glm/glm.hpp>
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

} // namespace dse

#endif // DSE_COMPONENTS_3D_PHYSICS_H