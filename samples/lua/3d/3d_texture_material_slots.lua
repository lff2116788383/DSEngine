-- 3D P3 sample: texture/material slots showcase
-- 目标：专项展示 albedo/normal/emissive/roughness 等 texture slot；Lua 可直接绑定贴图槽并 author UV/normal/tangent。
local TextureMaterialSlots3D = {}

local state = { camera = nil, samples = {}, time = 0.0, logged = false }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function plane_vertices()
    return { -0.7,-0.45,0.0, 0.7,-0.45,0.0, 0.7,0.45,0.0, -0.7,0.45,0.0 }
end

local function plane_indices()
    return { 0,1,2, 2,3,0 }
end

local function plane_uvs()
    return { 0.0,0.0, 1.0,0.0, 1.0,1.0, 0.0,1.0 }
end

local function plane_normals()
    return { 0.0,0.0,1.0, 0.0,0.0,1.0, 0.0,0.0,1.0, 0.0,0.0,1.0 }
end

local function plane_tangents()
    return { 1.0,0.0,0.0, 1.0,0.0,0.0, 1.0,0.0,0.0, 1.0,0.0,0.0 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 10.0
    dse.ecs.add_transform(camera, 0.0, 3.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.5, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.5, 0.12) end
    state.camera = camera
end

local function add_cube(name, x, y, z, color, metallic, roughness, emissive, material_path, mesh_path)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 0.74, 0.74, 0.74)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    if type(mesh_path) == "string" and mesh_path ~= "" then
        dse.ecs.set_mesh_path(e, mesh_path)
    end
    if type(material_path) == "string" and material_path ~= "" then
        dse.ecs.set_mesh_material(e, material_path)
    else
        dse.ecs.set_mesh_material(e, metallic or 0.0, roughness or 0.55, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    end
    table.insert(state.samples, { name = name, entity = e, x = x, y = y, z = z, roughness = roughness or 0.55, emissive = emissive or {0.0, 0.0, 0.0} })
    return e
end

local function add_label_marker(name, x, z, color)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, -0.58, z, 0.42, 0.06, 0.42)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.45, 1.0, color[1] * 0.12, color[2] * 0.12, color[3] * 0.12, 1.0, true, true)
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.96, 0.88, 1.35, 0.18, 0.34)
    local fill = dse.ecs.create_entity()
    dse.ecs.add_point_light_3d(fill, 0.0, 2.4, 2.8, 0.25, 0.52, 1.0, 1.2, 6.0)

    local mesh_path = (type(config) == "table" and type(config.mesh_path) == "string") and config.mesh_path or "models/cube.dmesh"
    local material_path = (type(config) == "table" and type(config.material_path) == "string") and config.material_path or "models/cube.dmat"

    local albedo = add_cube("albedo_dmat", -2.6, 0.25, 0.0, {1.0, 1.0, 1.0, 1.0}, 0.0, 0.55, {0.0, 0.0, 0.0}, material_path, mesh_path)
    local rough_low = add_cube("roughness_low", -1.3, 0.25, 0.0, {0.86, 0.86, 0.92, 1.0}, 0.0, 0.18)
    add_cube("roughness_high", 0.0, 0.25, 0.0, {0.62, 0.70, 0.76, 1.0}, 0.0, 0.88)
    local emissive = add_cube("emissive_slot", 1.3, 0.25, 0.0, {0.18, 0.32, 1.0, 1.0}, 0.0, 0.45, {0.02, 0.10, 0.65})
    local normal = add_cube("normal_slot_marker", 2.6, 0.25, 0.0, {0.55, 0.38, 1.0, 1.0}, 0.0, 0.42, {0.08, 0.04, 0.20})

    local authored = dse.ecs.create_entity()
    dse.ecs.add_transform(authored, 0.0, 1.45, 0.0, 1.55, 1.55, 1.55)
    dse.ecs.add_mesh_renderer(authored, 1.0, 1.0, 1.0, 1.0, plane_vertices(), plane_indices())
    dse.ecs.set_mesh_shader_variant(authored, "MESH_LIT")
    dse.ecs.set_mesh_material(authored, 0.0, 0.42, 1.0, 0.05, 0.05, 0.05, 1.0, true, true)
    table.insert(state.samples, { name = "authored_uv_normal_tangent", entity = authored, x = 0.0, y = 1.45, z = 0.0, roughness = 0.42, emissive = {0.05, 0.05, 0.05} })

    if dse.ecs.set_mesh_texture then
        local ok_albedo, handle_albedo, width_albedo, height_albedo = dse.ecs.set_mesh_texture(albedo, "albedo", "models/CesiumLogoFlat.png")
        local ok_normal = dse.ecs.set_mesh_texture(normal, "normal", "models/CesiumLogoFlat.png")
        local ok_emissive = dse.ecs.set_mesh_texture(emissive, "emissive", "models/CesiumLogoFlat.png")
        local ok_mr = dse.ecs.set_mesh_texture(rough_low, "metallic_roughness", "models/CesiumLogoFlat.png")
        local ok_authored_texture = dse.ecs.set_mesh_texture(authored, "albedo", "models/CesiumLogoFlat.png")
        print(string.format("[3D][TextureMaterialSlots] set_mesh_texture: albedo=%s handle=%s size=%sx%s normal=%s emissive=%s metallic_roughness=%s authored_texture=%s", tostring(ok_albedo), tostring(handle_albedo), tostring(width_albedo), tostring(height_albedo), tostring(ok_normal), tostring(ok_emissive), tostring(ok_mr), tostring(ok_authored_texture)))
    else
        print("[3D][TextureMaterialSlots] set_mesh_texture unavailable; using dmat/component fallback markers only.")
    end

    local uv_ok, uv_count, uv_vertices = false, 0, 0
    local normals_ok, normal_count = false, 0
    local tangents_ok, tangent_count = false, 0
    if dse.ecs.set_mesh_uvs then
        uv_ok, uv_count, uv_vertices = dse.ecs.set_mesh_uvs(authored, plane_uvs())
    end
    if dse.ecs.set_mesh_normals then
        normals_ok, normal_count = dse.ecs.set_mesh_normals(authored, plane_normals())
    end
    if dse.ecs.set_mesh_tangents then
        tangents_ok, tangent_count = dse.ecs.set_mesh_tangents(authored, plane_tangents())
    end
    print(string.format("[3D][TextureMaterialSlots] mesh_authoring_api set_mesh_uvs=%s uv_count=%s/%s set_mesh_normals=%s normal_count=%s set_mesh_tangents=%s tangent_count=%s", tostring(uv_ok), tostring(uv_count), tostring(uv_vertices), tostring(normals_ok), tostring(normal_count), tostring(tangents_ok), tostring(tangent_count)))

    add_label_marker("albedo", -2.6, -0.95, {1.0, 1.0, 1.0})
    add_label_marker("rough_low", -1.3, -0.95, {0.9, 0.9, 0.25})
    add_label_marker("rough_high", 0.0, -0.95, {0.35, 0.75, 1.0})
    add_label_marker("emissive", 1.3, -0.95, {0.1, 0.4, 1.0})
    add_label_marker("normal", 2.6, -0.95, {0.7, 0.45, 1.0})

    print(string.format("[3D][TextureMaterialSlots] setup: dmat sample mesh=%s material=%s plus Lua set_mesh_texture slot markers and authored UV quad.", mesh_path, material_path))
    print("[3D][TextureMaterialSlots] active API: set_mesh_texture plus set_mesh_uvs/set_mesh_normals/set_mesh_tangents author Lua mesh vertex attributes.")
end

function TextureMaterialSlots3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function TextureMaterialSlots3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    for i, item in ipairs(state.samples) do
        dse.ecs.set_transform_rotation(item.entity, state.time * (10.0 + i * 2.0), state.time * (22.0 + i * 4.0), 0.0)
        if item.name == "emissive_slot" then
            local pulse = 0.45 + math.sin(state.time * 3.0) * 0.25
            dse.ecs.set_mesh_emissive(item.entity, 0.02, 0.10, pulse)
        elseif item.name == "roughness_low" then
            dse.ecs.set_mesh_material_scalar(item.entity, "roughness", 0.12 + math.sin(state.time * 2.0) * 0.06)
        end
    end
    if not state.logged and state.time > 0.2 then
        state.logged = true
        print("[3D][TextureMaterialSlots] runtime: mesh_authoring_api authored quad uses UV texture sampling, explicit normal, and explicit tangent; visible row maps to material texture slots.")
    end
end

return TextureMaterialSlots3D
