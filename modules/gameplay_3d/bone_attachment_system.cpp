#include "modules/gameplay_3d/bone_attachment_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse {
namespace gameplay3d {

void BoneAttachmentSystem::Update(World& world) {
    auto& reg = world.registry();
    auto view = reg.view<BoneAttachmentComponent, TransformComponent>();
    if (view.size_hint() == 0) return;

    for (auto entity : view) {
        auto& attach = view.get<BoneAttachmentComponent>(entity);
        auto& xform  = view.get<TransformComponent>(entity);

        if (attach.target_entity == entt::null) continue;
        if (!reg.valid(attach.target_entity)) continue;

        auto* anim = reg.try_get<Animator3DComponent>(attach.target_entity);
        if (!anim || !anim->enabled || !anim->skel_cache.valid) continue;

        // 1. 惰性解析骨骼索引
        if (attach.index_dirty) {
            auto it = anim->skel_cache.bone_name_to_index.find(attach.bone_name);
            attach.cached_bone_index = (it != anim->skel_cache.bone_name_to_index.end())
                                     ? it->second : -1;
            attach.index_dirty = false;
        }
        if (attach.cached_bone_index < 0) continue;

        const int bi = attach.cached_bone_index;
        if (bi >= static_cast<int>(anim->final_bone_matrices.size())) continue;
        if (bi >= static_cast<int>(anim->skel_cache.bind_globals.size())) continue;

        // 2. 恢复骨骼 model-space 全局矩阵
        //    final_bone_matrices[i] = global[i] * inv_bind[i]
        //    => global[i] = final_bone_matrices[i] * bind_globals[i]
        const glm::mat4 bone_model = anim->final_bone_matrices[bi]
                                   * anim->skel_cache.bind_globals[bi];

        auto* target_xform = reg.try_get<TransformComponent>(attach.target_entity);
        if (!target_xform) continue;

        const glm::mat4 bone_world = target_xform->local_to_world * bone_model;

        // 3. 应用本地偏移
        const glm::mat4 offset = glm::translate(glm::mat4(1.0f), attach.offset_position)
                               * glm::mat4_cast(attach.offset_rotation)
                               * glm::scale(glm::mat4(1.0f), attach.offset_scale);

        xform.local_to_world = bone_world * offset;
        xform.dirty = false;

        // 4. 触发直接子实体脏标记（确保层级传播）
        auto child_view = reg.view<ParentComponent, TransformComponent>();
        for (auto child : child_view) {
            if (child_view.get<ParentComponent>(child).parent == entity) {
                child_view.get<TransformComponent>(child).dirty = true;
            }
        }
    }
}

} // namespace gameplay3d
} // namespace dse
