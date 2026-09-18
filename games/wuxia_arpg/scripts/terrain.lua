-- New game terrain: merged lit 3D floor meshes + billboards + lantern lights.
local core = require("core")
local A = require("assets")
local D = require("data")
local T = { entities = {}, lights = {}, dir_light = nil }
local ecs = dse.ecs
local LIGHTS = (os and os.getenv and os.getenv("DSE_WUXIA_LIGHTS") ~= "0")
local MAT = { grass={58,112,68}, dirt={142,104,62}, stone={128,128,136}, water={44,88,140} }

local function track(e) T.entities[#T.entities+1] = e; return e end
local function add_quad(g, x0, z0, x1, z1, y)
    local v, i = g.v, g.i
    local b = #v/3
    local pts = { {x0,y,z0}, {x1,y,z0}, {x1,y,z1}, {x0,y,z1} }
    for _,p in ipairs(pts) do v[#v+1]=p[1]; v[#v+1]=p[2]; v[#v+1]=p[3] end
    i[#i+1]=b; i[#i+1]=b+1; i[#i+1]=b+2; i[#i+1]=b; i[#i+1]=b+2; i[#i+1]=b+3
end
local function add_box(g, x0, z0, x1, z1, y0, y1)
    add_quad(g, x0, z0, x1, z1, y1)
    local v, i = g.v, g.i
    local faces = {
        {x0,y0,z0, x1,y0,z0, x1,y1,z0, x0,y1,z0},
        {x1,y0,z0, x1,y0,z1, x1,y1,z1, x1,y1,z0},
        {x1,y0,z1, x0,y0,z1, x0,y1,z1, x1,y1,z1},
        {x0,y0,z1, x0,y0,z0, x0,y1,z0, x0,y1,z1},
    }
    for _,f in ipairs(faces) do
        local b=#v/3
        for k=1,12 do v[#v+1]=f[k] end
        i[#i+1]=b; i[#i+1]=b+1; i[#i+1]=b+2; i[#i+1]=b; i[#i+1]=b+2; i[#i+1]=b+3
    end
end
local function create_mesh(kind, g)
    if #g.i == 0 then return end
    local c = MAT[kind] or MAT.grass
    local e = ecs.create_entity(); track(e)
    ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
    ecs.add_mesh_renderer(e, c[1]/255, c[2]/255, c[3]/255, 1.0, g.v, g.i)
    ecs.set_mesh_shader_variant(e, "MESH_LIT")
    ecs.set_mesh_material(e, 0.0, 0.86, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
end
local function add_prop(kind, x, z)
    local e = ecs.create_entity(); track(e)
    ecs.add_transform(e, x, 0, z, 1, 1, 1)
    local texture = A.tex[kind]
    local w,h = 1,1
    if kind=="tree" then w,h=1.5,2.0 elseif kind=="bamboo" then w,h=1.0,2.0
    elseif kind=="house" then w,h=3.0,2.5 elseif kind=="rock" then w,h=1.0,0.75
    elseif kind=="lantern" then w,h=0.5,1.0 end
    ecs.add_sprite3d(e, texture or 0, w, h, {billboard="yaw",anchor=0.0,lit=true,receive_shadow=true})
    ecs.set_sprite3d_lit(e, true)
    ecs.set_sprite3d_receive_shadow(e, true)
    ecs.set_sprite3d_contact_shadow(e, true, kind=="house" and 1.4 or 0.45, 0.45)
    if kind=="lantern" then
        ecs.set_sprite3d_emissive(e, 0.55, 0.18, 0.06)
        if LIGHTS then
            local l = ecs.create_entity(); track(l)
            ecs.add_transform(l, x, 1.1, z, 1, 1, 1)
            ecs.add_point_light_3d(l, 1.0, 0.62, 0.32, 2.2, 5.0)
            T.lights[#T.lights+1] = l
        end
    end
end
function T.clear()
    for _,e in ipairs(T.entities) do pcall(ecs.destroy_entity, e) end
    T.entities, T.lights, T.dir_light = {}, {}, nil
end
function T.build()
    T.clear()
    local groups = { grass={v={},i={}}, dirt={v={},i={}}, stone={v={},i={}}, water={v={},i={}} }
    for row=1,D.h do
        for col=1,D.w do
            local x0, z0 = col-1, row-1
            local kind = D.tile_type(row, col)
            local y = (kind=="water") and -0.08 or 0.0
            add_quad(groups[kind], x0, z0, x0+1, z0+1, y)
            if D.solid[row][col] and D.map[row]:sub(col,col) == "#" then
                add_box(groups.stone, x0, z0, x0+1, z0+1, 0.0, 1.0)
            end
        end
    end
    for kind,g in pairs(groups) do create_mesh(kind,g) end
    for row=1,D.h do
        for col=1,D.w do
            local p = D.prop_for(row,col)
            if p then add_prop(p, col-0.5, row-0.5) end
        end
    end
    local d = ecs.create_entity(); track(d); T.dir_light = d
    ecs.add_transform(d, 0, 8, 0, 1, 1, 1)
    ecs.add_directional_light_3d(d, 0.32, -0.9, 0.25, 0.92, 0.96, 1.0, 1.0, 0.24, 0.0)
    core.accept_log("terrain built map=%s lights=%s", D.current_id, tostring(LIGHTS))
end
function T.set_weather_light(weather, mix)
    mix = mix or 1.0
    if not T.dir_light then return end
    local r,g,b,intensity = 0.92,0.96,1.0,1.0
    if weather == "storm" or weather == "rain" then r,g,b,intensity = 0.62,0.72,1.0,0.72
    elseif weather == "fog" then r,g,b,intensity = 0.78,0.88,0.92,0.82
    elseif weather == "snow" then r,g,b,intensity = 1.0,1.0,1.0,0.95
    elseif weather == "leaf" then r,g,b,intensity = 1.0,0.90,0.72,0.95 end
    ecs.set_directional_light_3d(T.dir_light, 1, 0.32, -0.9, 0.25, r,g,b, intensity, 0.24, 0.0)
    for _,l in ipairs(T.lights) do
        if weather == "storm" or weather == "rain" then
            ecs.set_point_light_intensity(l, 2.8)
            ecs.set_point_light_color(l, 1.0, 0.52, 0.24)
        elseif weather == "fog" then
            ecs.set_point_light_intensity(l, 2.0)
            ecs.set_point_light_color(l, 0.72, 0.95, 0.82)
        else
            ecs.set_point_light_intensity(l, 2.2)
            ecs.set_point_light_color(l, 1.0, 0.62, 0.32)
        end
    end
end
return T