/**
 * @file main.cpp
 * @brief DSEngine SDK consumer minimal verification example.
 *
 * Checks:
 * 1. find_package(DSEngine) locates the SDK
 * 2. Public headers are includable
 * 3. Linking to the DSEngine DLL succeeds
 * 4. Core API types are visible
 */

#include <cstdio>

// Aggregate header
#include "engine/dse.h"

// Individual header verification
#include "engine/core/service_locator.h"
#include "engine/input/input.h"

// Public rendering API (abstract RHI). Verifies the SDK header boundary: the
// abstract factory/types are present while concrete backend impl headers
// (render/rhi/dx11|opengl|vulkan/) are intentionally excluded from the SDK.
#include "engine/render/rhi/rhi_factory.h"

int main() {
    std::printf("=== DSEngine SDK Consumer Example ===\n");
    std::printf("Version: %s\n", DSE_VERSION_STRING);
    std::printf("  Major: %d  Minor: %d  Patch: %d\n",
                DSE_VERSION_MAJOR, DSE_VERSION_MINOR, DSE_VERSION_PATCH);

    // ServiceLocator singleton
    auto& locator = dse::core::ServiceLocator::Instance();
    std::printf("ServiceLocator singleton: OK\n");

    // glm header propagation
    glm::vec3 v(1.0f, 2.0f, 3.0f);
    std::printf("glm::vec3(%.1f, %.1f, %.1f) OK\n", v.x, v.y, v.z);

    // entt header propagation
    entt::registry registry;
    auto entity = registry.create();
    std::printf("entt::registry create entity %u OK\n",
                static_cast<unsigned>(entity));

    // Abstract RHI public header. Uses the enum (header-only, no link) to prove
    // the public rendering surface (rhi_factory.h + rhi_types.h + rhi_handle.h)
    // is installed, while the concrete backend impl headers are excluded.
    RhiBackend backend = RhiBackend::Default;
    std::printf("RHI public header OK (default backend enum = %u)\n",
                static_cast<unsigned>(backend));

    std::printf("\nAll SDK consumer checks passed!\n");
    return 0;
}
