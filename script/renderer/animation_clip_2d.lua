require("lua_extension")
require("cpp_class")
require("renderer/sprite")

--- @class AnimationClip2D 2D动画片段
AnimationClip2D = class("AnimationClip2D", CppClass)

function AnimationClip2D:ctor(...)
    AnimationClip2D.super.ctor(self, ...)
    if self.cpp_class_instance_ == nil then
        self.cpp_class_instance_ = Cpp.AnimationClip2D()
    end
end

function AnimationClip2D:AddFrame(sprite, duration)
    self.cpp_class_instance_:AddFrame(sprite:cpp_class_instance(), duration)
end

function AnimationClip2D:set_is_looping(loop)
    self.cpp_class_instance_:set_is_looping(loop)
end

function AnimationClip2D:is_looping()
    return self.cpp_class_instance_:is_looping()
end

function AnimationClip2D:set_name(name)
    self.cpp_class_instance_:set_name(name)
end

function AnimationClip2D:name()
    return self.cpp_class_instance_:name()
end
