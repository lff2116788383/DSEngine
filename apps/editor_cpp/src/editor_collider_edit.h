#pragma once

// 选中实体的碰撞体可视化拖拽编辑。
//
// 在 Scene 视口中，对已选实体的碰撞体（Box3D / Sphere3D / Box2D / Circle2D）显示一个
// ImGuizmo 控制柄，直接拖拽修改碰撞体的 size/radius 与 center/offset（独立于实体 Transform）。
//
// 纯数学核心放在 collideredit 命名空间，无 ImGui 依赖、可无头测试：碰撞体局部参数与
// 世界矩阵之间的相互转换（拖拽 → 写回的关键逻辑）。

#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// 碰撞体拖拽编辑总开关（默认关闭）。
bool& GetColliderEditEnabled();

/// 在视口内为选中实体绘制碰撞体编辑控制柄并写回组件。
/// 返回 true 表示本帧消费了控制柄（调用方应跳过普通 Transform Gizmo，避免叠加）。
bool DrawColliderEditGizmo(EditorContext& ctx,
                           const glm::vec2& window_pos,
                           const glm::vec2& panel_size,
                           const glm::mat4& view,
                           const glm::mat4& proj);

namespace collideredit {

enum class Kind { None, Box3D, Sphere3D, Box2D, Circle2D };

inline float SafeDiv(float a, float b) { return (b > 1e-6f || b < -1e-6f) ? a / b : a; }

// ── Box（3D；2D 时 z 分量退化）─────────────────────────────────────────────
// 世界矩阵：T(entity_pos + local_center * entity_scale) · S(size * entity_scale)。
glm::mat4 BuildBoxMatrix(const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                         const glm::vec3& local_center, const glm::vec3& size);

// 从世界矩阵反解碰撞体局部 center / size（与 BuildBoxMatrix 互逆）。
void ExtractBox(const glm::mat4& m, const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                glm::vec3& out_center, glm::vec3& out_size);

// ── Sphere / Circle（统一直径，参考 entity_scale 的最大分量）──────────────────
glm::mat4 BuildSphereMatrix(const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                            const glm::vec3& local_center, float radius);

void ExtractSphere(const glm::mat4& m, const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                   glm::vec3& out_center, float& out_radius);

}  // namespace collideredit

}  // namespace dse::editor
