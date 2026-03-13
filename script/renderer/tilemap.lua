require("lua_extension")
require("component/component")
require("renderer/sprite")

--- @class Tilemap : Component
Tilemap = class("Tilemap", Component)

function Tilemap:ctor()
    Tilemap.super.ctor(self)
end

function Tilemap:SetTile(cell_pos, sprite)
    if sprite then
        self.cpp_component_instance_:SetTile(cell_pos, sprite:cpp_class_instance())
    else
        -- If sprite is nil, we should probably clear the tile
        -- But since we can't pass nil to a function expecting Sprite* in some bindings, we need to be careful
        -- However, sol2 handles nullptr for pointers usually.
        -- Let's assume C++ side handles nullptr.
        self.cpp_component_instance_:SetTile(cell_pos, nil)
    end
end

function Tilemap:GetTile(cell_pos)
    local cpp_sprite = self.cpp_component_instance_:GetTile(cell_pos)
    if cpp_sprite then
        return Sprite.new_with(cpp_sprite)
    end
    return nil
end

function Tilemap:ClearAllTiles()
    self.cpp_component_instance_:ClearAllTiles()
end

function Tilemap:SaveToFile(file_path)
    return self.cpp_component_instance_:SaveToFile(file_path)
end

function Tilemap:LoadFromFile(file_path)
    return self.cpp_component_instance_:LoadFromFile(file_path)
end
