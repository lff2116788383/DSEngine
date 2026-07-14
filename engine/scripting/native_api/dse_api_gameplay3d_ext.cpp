/**
 * @file dse_api_gameplay3d_ext.cpp
 * @brief DSEngine C ABI - Gameplay3D 扩展
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/gameplay.h"
#include "engine/ecs/components_3d_ai.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" int dse_character_check_ground(uint32_t e, float* out_normal) {
    World* world = GW();
    if (!world) return 0;
    const auto* cc = world->registry().try_get<CharacterController3DComponent>(TE(e));
    if (!cc) return 0;
    if (out_normal) { out_normal[0] = 0.0f; out_normal[1] = 1.0f; out_normal[2] = 0.0f; }
    return cc->is_grounded ? 1 : 0;
}

