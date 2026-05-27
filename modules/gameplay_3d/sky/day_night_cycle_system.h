#ifndef DSE_DAY_NIGHT_CYCLE_SYSTEM_H
#define DSE_DAY_NIGHT_CYCLE_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
namespace gameplay3d {

/// 昼夜循环系统：从 DayNightCycleComponent 计算太阳方向并写入 DirectionalLightComponent。
class DayNightCycleSystem {
public:
    void Update(World& world, float delta_time);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_DAY_NIGHT_CYCLE_SYSTEM_H
