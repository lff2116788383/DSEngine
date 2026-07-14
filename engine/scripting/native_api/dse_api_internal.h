/**
 * @file dse_api_internal.h
 * @brief Shared internal utilities for dse_api_* implementation files.
 *
 * Provides common inline helpers (GW, TE, GAM, Keep, WriteStr) and
 * handle-based registries used across split implementation modules.
 */
#pragma once

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

using Entity = entt::entity;

namespace dse_api_internal {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }
inline bool Keep(float v) { return std::isnan(v); }
inline void WriteStr(const std::string& s, char* out, int cap) {
    if (!out || cap <= 0) return;
    int n = std::min(static_cast<int>(s.size()), cap - 1);
    std::memcpy(out, s.data(), static_cast<size_t>(n));
    out[n] = '\0';
}

inline float Hash2D(int x, int y) {
    int n = x * 374761393 + y * 668265263;
    n = (n ^ (n >> 13)) * 1274126177;
    return static_cast<float>((n & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF));
}

inline float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace dse_api_internal
