#ifndef DSE_LIGHT_2D_H
#define DSE_LIGHT_2D_H

#include "component/component.h"
#include <glm/glm.hpp>

class Light2D : public Component {
public:
    enum class Type {
        Point,
        Directional,
        Spot
    };

    Light2D();
    virtual ~Light2D();

    void SetColor(const glm::vec3& color) { color_ = color; }
    void SetIntensity(float intensity) { intensity_ = intensity; }
    void SetRange(float range) { range_ = range; }
    void SetType(Type type) { type_ = type; }

    virtual void OnRender(); // Should probably submit to a light manager

private:
    glm::vec3 color_;
    float intensity_;
    float range_;
    Type type_;
};

#endif // DSE_LIGHT_2D_H
