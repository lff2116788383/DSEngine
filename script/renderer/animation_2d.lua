require("lua_extension")
require("component/component")
require("renderer/animation_clip_2d")

--- @class Animation2D : Component
Animation2D = class("Animation2D", Component)

function Animation2D:ctor()
    Animation2D.super.ctor(self)
end

function Animation2D:InitCppComponent()
    self.cpp_component_instance_ = Cpp.Animation2D()
end

function Animation2D:set_clip(clip)
    self.cpp_component_instance_:set_clip(clip:cpp_class_instance())
end

function Animation2D:clip()
    return self.cpp_component_instance_:clip()
end

function Animation2D:Play()
    self.cpp_component_instance_:Play()
end

function Animation2D:Stop()
    self.cpp_component_instance_:Stop()
end

function Animation2D:set_is_playing(is_playing)
    self.cpp_component_instance_:set_is_playing(is_playing)
end

function Animation2D:is_playing()
    return self.cpp_component_instance_:is_playing()
end
