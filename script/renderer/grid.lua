require("lua_extension")
require("component/component")

--- @class Grid : Component
Grid = class("Grid", Component)

function Grid:ctor()
    Grid.super.ctor(self)
end

function Grid:set_cell_size(size)
    self.cpp_component_instance_:set_cell_size(size)
end

function Grid:cell_size()
    return self.cpp_component_instance_:cell_size()
end

function Grid:set_cell_gap(gap)
    self.cpp_component_instance_:set_cell_gap(gap)
end

function Grid:cell_gap()
    return self.cpp_component_instance_:cell_gap()
end

function Grid:CellToWorld(cell_pos)
    return self.cpp_component_instance_:CellToWorld(cell_pos)
end

function Grid:WorldToCell(world_pos)
    return self.cpp_component_instance_:WorldToCell(world_pos)
end
