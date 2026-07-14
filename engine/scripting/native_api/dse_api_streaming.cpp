/**
 * @file dse_api_streaming.cpp
 * @brief DSEngine C ABI - Streaming — 使用 StreamingManager
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/assets/streaming_manager.h"

using namespace dse;
using namespace dse_api_internal;


static dse::streaming::AssetType ParseAssetType(const char* type_str) {
    if (!type_str) return dse::streaming::AssetType::Texture;
    if (strcmp(type_str, "texture") == 0) return dse::streaming::AssetType::Texture;
    if (strcmp(type_str, "mesh") == 0) return dse::streaming::AssetType::Mesh;
    if (strcmp(type_str, "animation") == 0) return dse::streaming::AssetType::Animation;
    if (strcmp(type_str, "skeleton") == 0) return dse::streaming::AssetType::Skeleton;
    if (strcmp(type_str, "audio") == 0) return dse::streaming::AssetType::Audio;
    if (strcmp(type_str, "material") == 0) return dse::streaming::AssetType::Material;
    return dse::streaming::AssetType::Texture;
}

extern "C" uint32_t dse_streaming_create_zone(const char* name, float x, float y, float z,
                                       float load_r, float unload_r) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr) return 0;
    return mgr->CreateZone(name ? name : "", glm::vec3(x, y, z), load_r, unload_r);
}
extern "C" void dse_streaming_destroy_zone(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->DestroyZone(zone);
}
extern "C" void dse_streaming_add_asset(uint32_t zone, const char* path, const char* type_str) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->AddAsset(zone, path ? path : "", ParseAssetType(type_str));
}
extern "C" void dse_streaming_add_assets(uint32_t zone, const char* const* paths, int count,
                                        const char* type_str) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr || !paths) return;
    std::vector<std::string> v;
    v.reserve(count);
    for (int i = 0; i < count; ++i) if (paths[i]) v.emplace_back(paths[i]);
    mgr->AddAssets(zone, v, ParseAssetType(type_str));
}
extern "C" void dse_streaming_set_zone_center(uint32_t zone, float x, float y, float z) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->SetZoneCenter(zone, glm::vec3(x, y, z));
}
extern "C" void dse_streaming_force_load(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->ForceLoadZone(zone);
}
extern "C" void dse_streaming_force_unload(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (mgr) mgr->ForceUnloadZone(zone);
}
extern "C" int dse_streaming_get_zone_state(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr) return 0;
    return static_cast<int>(mgr->GetZoneState(zone));
}
extern "C" float dse_streaming_get_zone_progress(uint32_t zone) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    return mgr ? mgr->GetZoneProgress(zone) : 0.0f;
}
extern "C" void dse_streaming_set_budget(int max_loads_per_frame, int max_concurrent) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!mgr) return;
    mgr->SetLoadBudgetPerFrame(max_loads_per_frame);
    mgr->SetMaxConcurrentLoads(max_concurrent);
}
extern "C" int dse_streaming_get_active_loads(void) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    return mgr ? mgr->GetActiveLoadCount() : 0;
}
extern "C" int dse_streaming_get_zone_count(void) {
    auto* mgr = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    return mgr ? static_cast<int>(mgr->GetZoneCount()) : 0;
}

