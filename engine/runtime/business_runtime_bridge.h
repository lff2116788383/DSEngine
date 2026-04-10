#ifndef DSE_BUSINESS_RUNTIME_BRIDGE_H
#define DSE_BUSINESS_RUNTIME_BRIDGE_H

#include "engine/runtime/runtime_context.h"
#include "engine/scripting/lua/lua_runtime.h"

namespace dse::runtime {

struct RuntimeStatsBindings {
    std::function<int()> get_draw_calls;
    std::function<int()> get_max_batch_sprites;
    std::function<int()> get_sprite_count;
};

bool BootstrapBusinessRuntime(RuntimeContext& context, const RuntimeStatsBindings& stats_bindings);
void TickBusinessRuntime(RuntimeContext& context, float delta_time);
void ShutdownBusinessRuntime(const RuntimeContext& context);

} // namespace dse::runtime

#endif
