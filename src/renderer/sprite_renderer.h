#ifndef UNTITLED_SPRITE_RENDERER_H
#define UNTITLED_SPRITE_RENDERER_H

#include <glm/glm.hpp>
#include "component/component.h"
#include "sprite.h"
#include <rttr/registration>

class SpriteRenderer : public Component {
public:
    SpriteRenderer();
    ~SpriteRenderer();

    void set_sprite(Sprite* sprite);
    Sprite* sprite() const { return sprite_; }

    void set_color(const glm::vec4& color) { color_ = color; }
    const glm::vec4& color() const { return color_; }

    void set_sorting_layer(int layer){sorting_layer_=layer;}
    int sorting_layer() const {return sorting_layer_;}

    void set_order_in_layer(int order){order_in_layer_=order;}
    int order_in_layer() const {return order_in_layer_;}

    virtual void Update() override;

private:
    Sprite* sprite_ = nullptr;
    glm::vec4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};

    int sorting_layer_ = 0;
    int order_in_layer_ = 0;
    
    // Internal state to track if mesh needs update
    Sprite* last_sprite_ = nullptr;
    
RTTR_ENABLE();
};

#endif //UNTITLED_SPRITE_RENDERER_H
