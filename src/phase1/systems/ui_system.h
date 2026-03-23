#ifndef DSE_PHASE1_UI_SYSTEM_H
#define DSE_PHASE1_UI_SYSTEM_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse {
namespace phase1 {

class UISystem {
public:
    UISystem() = default;
    ~UISystem() = default;

    // Update layout and handle UI events
    void Update(entt::registry& registry, float dt, const glm::vec2& screen_size, const glm::vec2& mouse_pos, bool is_mouse_down);

private:
    void SyncLabels(entt::registry& registry);
    void UpdateLayout(entt::registry& registry, const glm::vec2& screen_size);
    void HandleEvents(entt::registry& registry, const glm::vec2& mouse_pos, bool is_mouse_down);
};

} // namespace phase1
} // namespace dse

#endif // DSE_PHASE1_UI_SYSTEM_H
