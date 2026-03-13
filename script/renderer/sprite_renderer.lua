require("lua_extension")
require("component/component")
require("renderer/sprite")

--- @class SpriteRenderer : Component
SpriteRenderer = class("SpriteRenderer", Component)

function SpriteRenderer:ctor()
    SpriteRenderer.super.ctor(self)
end

function SpriteRenderer:InitCppComponent()
    self.cpp_component_instance_ = Cpp.SpriteRenderer()
end

function SpriteRenderer:set_sprite(sprite)
    if sprite then
        self.cpp_component_instance_:set_sprite(sprite:cpp_class_instance())
    else
        self.cpp_component_instance_:set_sprite(nil)
    end
end

function SpriteRenderer:sprite()
    return self.cpp_component_instance_:sprite()
end

function SpriteRenderer:set_color(r, g, b, a)
    self.cpp_component_instance_:set_color(glm.vec4(r, g, b, a))
end

function SpriteRenderer:color()
    return self.cpp_component_instance_:color()
end

function SpriteRenderer:set_sorting_layer(layer)
    self.cpp_component_instance_:set_sorting_layer(layer)
end

function SpriteRenderer:sorting_layer()
    return self.cpp_component_instance_:sorting_layer()
end

function SpriteRenderer:set_order_in_layer(order)
    self.cpp_component_instance_:set_order_in_layer(order)
end

function SpriteRenderer:order_in_layer()
    return self.cpp_component_instance_:order_in_layer()
end
