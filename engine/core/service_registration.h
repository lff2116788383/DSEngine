/**
 * @file service_registration.h
 * @brief Centralized registration of engine singletons into ServiceLocator.
 *
 * Registers existing singleton instances (HttpClient, DSSLMaterialLoader,
 * LuaDebugger) as non-owning entries in ServiceLocator, enabling test-time
 * override via ServiceLocator::Register without changing singleton lifetimes.
 */
#ifndef DSE_CORE_SERVICE_REGISTRATION_H
#define DSE_CORE_SERVICE_REGISTRATION_H

namespace dse {
namespace core {

/// Register all engine-level singletons into the global ServiceLocator.
/// Call once during EngineInstance initialization, after singletons are alive.
void RegisterEngineSingletons();

/// Unregister engine-level singletons from ServiceLocator.
/// Call during EngineInstance shutdown.
void UnregisterEngineSingletons();

} // namespace core
} // namespace dse

#endif // DSE_CORE_SERVICE_REGISTRATION_H
