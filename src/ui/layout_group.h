#ifndef DSE_UI_LAYOUT_H
#define DSE_UI_LAYOUT_H

#include "component/component.h"
#include "ui/rect_transform.h"
#include <vector>

class LayoutGroup : public Component {
public:
    enum class Type {
        Horizontal,
        Vertical,
        Grid
    };

    LayoutGroup();
    virtual ~LayoutGroup();

    void CalculateLayout();

    void SetType(Type type) { type_ = type; }
    void SetSpacing(float spacing) { spacing_ = spacing; }

    virtual void Update() override;

private:
    Type type_;
    float spacing_;
    std::vector<RectTransform*> children_;
};

#endif // DSE_UI_LAYOUT_H
