#pragma once

#include <entt/entt.hpp>

namespace dse::editor {

/// Draw the Particle Curve Editor section inside Inspector (for ParticleEmitterComponent)
void DrawParticleCurveEditor(entt::registry& registry, entt::entity entity);

} // namespace dse::editor
