require("lua_extension")
require("cpp_class")
require("renderer/texture_2d")

--- @class Sprite 2D精灵
Sprite = class("Sprite", CppClass)

function Sprite:ctor(...)
    Sprite.super.ctor(self, ...)
end

function Sprite:ctor_with(cpp_class_instance, ...)
    Sprite.super.ctor_with(self, cpp_class_instance, ...)
end

--- 创建Sprite
--- @param texture Texture2D
--- @return Sprite
function Sprite.Create(texture)
    local cpp_instance = Cpp.Sprite.Create(texture:cpp_class_instance())
    return Sprite.new_with(cpp_instance)
end

function Sprite:set_texture(texture)
    self.cpp_class_instance_:set_texture(texture:cpp_class_instance())
end

function Sprite:texture()
    return self.cpp_class_instance_:texture()
end

function Sprite:set_rect(x, y, width, height)
    self.cpp_class_instance_:set_rect(x, y, width, height)
end

function Sprite:rect()
    return self.cpp_class_instance_:rect()
end

function Sprite:set_pivot(x, y)
    self.cpp_class_instance_:set_pivot(x, y)
end

function Sprite:pivot()
    return self.cpp_class_instance_:pivot()
end

function Sprite:set_ppu(ppu)
    self.cpp_class_instance_:set_ppu(ppu)
end

function Sprite:ppu()
    return self.cpp_class_instance_:ppu()
end
