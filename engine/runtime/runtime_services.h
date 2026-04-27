#ifndef DSE_RUNTIME_SERVICES_H
#define DSE_RUNTIME_SERVICES_H

class World;
class AssetManager;

namespace dse::core {
class JobSystem;
}

namespace dse::runtime {

struct RuntimeServices {
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
    dse::core::JobSystem* job_system = nullptr;
};

} // namespace dse::runtime

#endif
