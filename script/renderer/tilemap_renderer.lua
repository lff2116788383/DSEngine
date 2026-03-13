require("lua_extension")
require("component/component")

--- @class TilemapRenderer : Component
TilemapRenderer = class("TilemapRenderer", Component)

function TilemapRenderer:ctor()
    TilemapRenderer.super.ctor(self)
end

function TilemapRenderer:set_color(color)
    self.cpp_component_instance_:set_color(color)
end

function TilemapRenderer:color()
    return self.cpp_component_instance_:color()
end

function TilemapRenderer:set_sorting_layer(layer)
    self.cpp_component_instance_:set_sorting_layer(layer)
end

function TilemapRenderer:sorting_layer()
    return self.cpp_component_instance_:sorting_layer()
end

function TilemapRenderer:set_order_in_layer(order)
    self.cpp_component_instance_:set_order_in_layer(order)
end

function TilemapRenderer:order_in_layer()
    return self.cpp_component_instance_:order_in_layer()
end
