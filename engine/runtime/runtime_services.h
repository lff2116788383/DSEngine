#ifndef DSE_RUNTIME_SERVICES_H
#define DSE_RUNTIME_SERVICES_H

class World;
class AssetManager;

namespace dse::runtime {

struct RuntimeServices {
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
};

} // namespace dse::runtime

#endif
