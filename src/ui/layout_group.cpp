#include "layout_group.h"
#include "component/game_object.h"
#include "ui/rect_transform.h"

LayoutGroup::LayoutGroup() : type_(Type::Vertical), spacing_(0.0f) {
}

LayoutGroup::~LayoutGroup() {
}

void LayoutGroup::Update() {
    // Check dirty flag and recalculate if needed
    // In a real system, we'd use event-driven updates (e.g. OnRectTransformChanged)
    CalculateLayout();
}

void LayoutGroup::CalculateLayout() {
    // Collect active children with RectTransform
    children_.clear();
    auto& nodes = game_object()->children();
    for (auto node : nodes) {
        GameObject* go = dynamic_cast<GameObject*>(node);
        if (go && go->active_self()) {
            RectTransform* rect = go->GetComponent<RectTransform>();
            if (rect) {
                children_.push_back(rect);
            }
        }
    }

    if (children_.empty()) return;

    float current_pos = 0.0f;
    for (auto rect : children_) {
        // Simple layout logic: stack them
        // Assuming pivot is top-left for simplicity
        // glm::vec2 size = rect->size();
        
        // In real implementation, we need to handle anchors, pivots, padding etc.
        // rect->set_anchored_position(glm::vec2(0, -current_pos));
        
        // current_pos += size.y + spacing_;
    }
}
