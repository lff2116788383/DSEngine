#ifndef DSE_BONE_ATTACHMENT_SYSTEM_H
#define DSE_BONE_ATTACHMENT_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
namespace gameplay3d {

/// 骨骼挂点系统：将拥有 BoneAttachmentComponent 的实体跟随目标骨骼。
/// 执行顺序须在 AnimatorSystem::ComputeFinalMatrices 之后。
class BoneAttachmentSystem {
public:
    static void Update(World& world);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_BONE_ATTACHMENT_SYSTEM_H
