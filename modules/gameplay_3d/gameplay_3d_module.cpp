#include "modules/gameplay_3d/gameplay_3d_module.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef DSE_GAMEPLAY3D_EXPORTS
#define DSE_MODULE_API __declspec(dllexport)
#else
#define DSE_MODULE_API __declspec(dllimport)
#endif
#else
#define DSE_MODULE_API __attribute__((visibility("default")))
#endif

extern "C" {

DSE_MODULE_API dse::core::IModule* CreateModule() {
    return new dse::gameplay3d::Gameplay3DModule();
}

DSE_MODULE_API void DestroyModule(dse::core::IModule* module) {
    delete module;
}

}
