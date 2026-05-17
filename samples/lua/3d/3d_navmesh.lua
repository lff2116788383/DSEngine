-- NavMesh 寻路验证 Demo（Recast/Detour）
-- 验证: nav.bake() 从三角面构建导航网格 → nav.find_path() 寻路
--       ECS NavAgent 自动沿路径移动
-- 场景: 围栏地面 + 障碍物 + 多个 agent 向目标移动
local NavMeshDemo = {}

local state = {
    camera = nil,
    light  = nil,
    agents = {},
    obstacles = {},
    time = 0.0,
    baked = false,
    nav_available = false,
    path_log_done = false,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_box(x, y, z, sx, sy, sz, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    return e
end

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 22.0
    dse.ecs.add_transform(e, 0, 16, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -38, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 120)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end
    state.camera = e
end

-- 生成地面盒子的三角面数据用于 nav.bake（上表面+侧面提供高度场厚度）
local function build_floor_triangles()
    local half = 14.0
    local divs = 10  -- 10x10 = 200 三角形 (上表面)
    local step = half * 2 / divs
    local y_top = 0.0
    local y_bot = -1.0  -- 给 Recast 提供 1m 的竖直范围
    local verts = {}
    local tris = {}
    -- 上表面顶点 (divs+1)^2
    for rz = 0, divs do
        for rx = 0, divs do
            table.insert(verts, -half + rx * step)
            table.insert(verts, y_top)
            table.insert(verts, -half + rz * step)
        end
    end
    -- 上表面三角形（CCW winding 使法线朝 +Y）
    local cols = divs + 1
    for rz = 0, divs - 1 do
        for rx = 0, divs - 1 do
            local i0 = rz * cols + rx
            local i1 = i0 + 1
            local i2 = i0 + cols
            local i3 = i2 + 1
            table.insert(tris, i0); table.insert(tris, i3); table.insert(tris, i1)
            table.insert(tris, i0); table.insert(tris, i2); table.insert(tris, i3)
        end
    end
    -- 底部 4 顶点（提供竖直包围盒下界）
    local base = #verts / 3
    table.insert(verts, -half); table.insert(verts, y_bot); table.insert(verts, -half)
    table.insert(verts,  half); table.insert(verts, y_bot); table.insert(verts, -half)
    table.insert(verts,  half); table.insert(verts, y_bot); table.insert(verts,  half)
    table.insert(verts, -half); table.insert(verts, y_bot); table.insert(verts,  half)
    -- 底部 2 三角形（法线朝下，不会被标记 walkable）
    table.insert(tris, base+0); table.insert(tris, base+2); table.insert(tris, base+1)
    table.insert(tris, base+0); table.insert(tris, base+3); table.insert(tris, base+2)
    -- 天花板 4 顶点（扩展 AABB 上界，确保 walkableHeight 净空）
    local y_sky = 4.0  -- 4m 净空 >> agent_height 1.8m
    local sky = #verts / 3
    table.insert(verts, -half); table.insert(verts, y_sky); table.insert(verts, -half)
    table.insert(verts,  half); table.insert(verts, y_sky); table.insert(verts, -half)
    table.insert(verts,  half); table.insert(verts, y_sky); table.insert(verts,  half)
    table.insert(verts, -half); table.insert(verts, y_sky); table.insert(verts,  half)
    -- 天花板 2 三角形（法线朝上但在高处，不影响地面行走）
    table.insert(tris, sky+0); table.insert(tris, sky+1); table.insert(tris, sky+2)
    table.insert(tris, sky+0); table.insert(tris, sky+2); table.insert(tris, sky+3)
    return verts, tris
end

local function setup_scene()
    -- 光源
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.4, 1.0, 0.95, 0.88, 1.1, 0.18, 0.32)
    state.light = light

    -- 地面
    make_box(0, -0.15, 0, 28, 0.1, 28, 0.30, 0.35, 0.30)

    -- 障碍物（agent 需要绕开）
    local obs_positions = {
        {-3, 0.8, -2, 2.5, 1.6, 2.5},
        { 4, 0.8,  3, 3.0, 1.6, 1.5},
        { 0, 0.8,  6, 1.5, 1.6, 4.0},
        {-5, 0.8,  5, 1.5, 1.6, 1.5},
        { 6, 0.8, -4, 2.0, 1.6, 2.0},
    }
    for _, pos in ipairs(obs_positions) do
        local e = make_box(pos[1], pos[2], pos[3], pos[4], pos[5], pos[6], 0.5, 0.45, 0.4)
        table.insert(state.obstacles, e)
    end

    -- 目标标记柱
    make_box(10, 1.2, -8, 0.5, 2.4, 0.5, 0.9, 0.9, 0.2)

    -- 检查 nav API
    state.nav_available = (nav ~= nil and nav.bake ~= nil)

    if state.nav_available then
        -- 烘焙导航网格
        local verts, tris = build_floor_triangles()
        local bake_cfg = {
            cell_size = 0.5,
            cell_height = 0.4,
            agent_height = 1.0,
            agent_radius = 0.3,
            agent_max_climb = 0.4,
            agent_max_slope = 60,
        }
        local ok = nav.bake(verts, tris, bake_cfg)
        state.baked = ok
        print(string.format("[NavMesh] bake: %s", ok and "成功" or "失败"))

        if ok then
            -- 测试寻路
            local path = nav.find_path(-10, 0, 8, 10, 0, -8)
            if path then
                print(string.format("[NavMesh] find_path: %d 个路径点", #path))
            else
                print("[NavMesh] find_path: 无路径")
            end
        end
    else
        print("[NavMesh] nav API 不可用 (DSE_ENABLE_NAVMESH 未启用), 展示静态场景")
    end

    -- 创建 agent 实体（小方块代表 agent）
    local agent_starts = {
        {-10, 0.5,  8, 0.2, 0.8, 0.3},
        { -8, 0.5, 10, 0.3, 0.5, 0.8},
        {-10, 0.5, 10, 0.8, 0.3, 0.5},
    }
    for i, start in ipairs(agent_starts) do
        local e = make_box(start[1], start[2], start[3], 0.6, 1.0, 0.6,
            start[4], start[5], start[6])

        if state.nav_available and state.baked and dse.ecs.set_nav_agent then
            dse.ecs.set_nav_agent(e, {
                speed = 2.5 + i * 0.3,
                acceleration = 6.0,
                stopping_dist = 0.5,
                radius = 0.4,
                height = 1.8,
            })
            dse.ecs.set_nav_destination(e, 10, 0, -8)
            print(string.format("[NavMesh] Agent %d: 出发 (%.0f,%.0f) → 目标 (10,-8) speed=%.1f",
                i, start[1], start[3], 2.5 + i * 0.3))
        end

        table.insert(state.agents, {entity = e, start = start, arrived = false})
    end

    print("[NavMesh] WASD 移动相机, 俯视观察 agent 寻路绕障")
end

function NavMeshDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function NavMeshDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    if not state.nav_available or not state.baked then return end
    if not dse.ecs.nav_agent_arrived then return end

    -- 检查到达
    for i, agent in ipairs(state.agents) do
        if not agent.arrived then
            local ok = dse.ecs.nav_agent_arrived(agent.entity)
            if ok then
                agent.arrived = true
                print(string.format("[NavMesh] Agent %d 已到达目标!", i))
            end
        end
    end

    -- 所有到达后重新出发（循环）
    local all_arrived = true
    for _, agent in ipairs(state.agents) do
        if not agent.arrived then all_arrived = false; break end
    end
    if all_arrived and state.time > 3.0 then
        for i, agent in ipairs(state.agents) do
            agent.arrived = false
            -- 随机新目标
            local tx = (math.random() - 0.5) * 20
            local tz = (math.random() - 0.5) * 20
            if dse.ecs.set_nav_destination then
                dse.ecs.set_nav_destination(agent.entity, tx, 0, tz)
            end
        end
    end
end

return NavMeshDemo
