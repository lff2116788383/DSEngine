-- cube_stress — 简单几何体 GPU 吞吐量压测
-- 用法:
--   DSE_ENTITY_COUNT=1000  → 生成实体数 (默认 500)
--   DSE_PERF_FRAMES=300    → 统计帧数 (默认 300)
--   DSE_NO_SHADOW=1        → 关闭阴影 (默认 0)
--------------------------------------------------------------------------------

local app = dse.app
local ecs = dse.ecs
local metrics = dse.metrics

--------------------------------------------------------------------------------
-- 配置
--------------------------------------------------------------------------------
local ENTITY_COUNT = tonumber(os.getenv("DSE_ENTITY_COUNT") or "500")
local PERF_FRAMES  = tonumber(os.getenv("DSE_PERF_FRAMES") or "300")
local NO_SHADOW    = (os.getenv("DSE_NO_SHADOW") or "0") ~= "0"
local UNIFORM_MAT  = (os.getenv("DSE_UNIFORM_MAT") or "0") ~= "0"

-- 简单 mesh (最小 dmesh, 无骨骼/无子网格)
local CUBE_MESH    = "cooked/Mesh1_2.dmesh"  -- 776B, 最小单子网格
local GROUND_TEX   = "assets/textures/demoField.jpg"
local SKYBOX_TEX   = "assets/textures/skybox000.jpg"

-- 性能统计
local frame_times  = {}
local frame_idx    = 0
local warmup_frames = 60
local stats_printed = false

--------------------------------------------------------------------------------
-- 辅助
--------------------------------------------------------------------------------
local function grid_layout(count)
    return math.ceil(math.sqrt(count))
end

local function grid_position(index, cols, spacing)
    local row = math.floor(index / cols)
    local col = index % cols
    local cx = (cols - 1) * spacing * 0.5
    local rz = (math.ceil(ENTITY_COUNT / cols) - 1) * spacing * 0.5
    return col * spacing - cx, 0, -(row * spacing - rz)
end

--------------------------------------------------------------------------------
-- Awake
--------------------------------------------------------------------------------
function Awake()
    app.set_window_title("Cube Stress — " .. ENTITY_COUNT .. " entities")
    app.set_data_root("examples/KF_Framework")

    print(string.format("[CubeStress] entities=%d perf_frames=%d no_shadow=%s gpu_driven_policy=%s uniform_mat=%s",
        ENTITY_COUNT, PERF_FRAMES, tostring(NO_SHADOW),
        os.getenv("DSE_GPU_DRIVEN_POLICY") or (os.getenv("DSE_DISABLE_GPU_DRIVEN") == "1" and "off" or "auto"),
        tostring(UNIFORM_MAT)))

    -- 1. 方向光
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1 + 16 + 1)
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,
        0.8, 0.8, 0.8, 1.0, 0.50, 1.0)
    if NO_SHADOW then
        ecs.set_directional_light_shadow(sun, false, 0, 0, 0, 0)
    else
        ecs.set_directional_light_shadow(sun, true, 1.0, 800, 4000, 20000)
    end

    -- 2. Sky light
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0)
    ecs.add_sky_light(sky, 0.38, 0.45, 0.55, 0.12, 0.11, 0.10, 0.3)

    -- 3. Skybox
    local skybox = ecs.create_entity()
    ecs.add_transform(skybox, 0, 0, 0)
    ecs.add_skybox(skybox, SKYBOX_TEX)

    -- 4. 地面
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, -1, 0, 400, 400, 400)
    ecs.add_mesh_renderer(ground, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(ground, "cooked/demoField.dmesh")
    ecs.set_mesh_shader_variant(ground, "MESH_HALFLAMBERT_STATIC")
    ecs.set_mesh_material(ground, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, true, false)
    ecs.set_mesh_texture(ground, "albedo", GROUND_TEX)

    -- 5. 大量简单几何体 (无动画, 单 submesh, 低顶点数)
    local cols = grid_layout(ENTITY_COUNT)
    local spacing = 80

    -- 随机颜色让场景可辨识
    math.randomseed(42)

    for i = 0, ENTITY_COUNT - 1 do
        local x, y, z = grid_position(i, cols, spacing)
        local scale = 0.3 + math.random() * 0.5
        local e = ecs.create_entity()
        ecs.add_transform(e, x, y, z, scale, scale, scale)
        local r, g, b
        if UNIFORM_MAT then
            r, g, b = 0.8, 0.3, 0.3
        else
            r = 0.3 + math.random() * 0.7
            g = 0.3 + math.random() * 0.7
            b = 0.3 + math.random() * 0.7
        end
        ecs.add_mesh_renderer(e, r, g, b, 1.0)
        ecs.set_mesh_path(e, CUBE_MESH)
        ecs.set_mesh_material(e, 0.3, 0.7, 1.0, 0.0, 0.0, 0.0, 1.0, true, false)
    end

    -- 6. 摄像机
    local cam = ecs.create_entity()
    local rows = math.ceil(ENTITY_COUNT / cols)
    local grid_w = cols * spacing
    local grid_d = rows * spacing
    local cam_dist = math.max(grid_w * 0.5, 1000)
    local cam_h = math.max(grid_d * 0.3, 300)
    local cam_z = grid_d * 0.5 + cam_dist
    ecs.add_transform(cam, 0, cam_h, cam_z)
    local dy = 0 - cam_h
    local dz = 0 - cam_z
    local pitch = math.deg(math.atan(dy, math.abs(dz)))
    ecs.set_transform_rotation(cam, pitch, 0, 0)
    ecs.add_camera_3d(cam, 60.0, 0, 1.0, 100000.0)

    print(string.format("[CubeStress] Spawned %d cubes in %dx%d grid (spacing=%d)",
        ENTITY_COUNT, cols, rows, spacing))
end

--------------------------------------------------------------------------------
-- Update
--------------------------------------------------------------------------------
function Update(dt)
    frame_idx = frame_idx + 1
    if frame_idx <= warmup_frames then return end

    local sample_idx = frame_idx - warmup_frames
    if sample_idx <= PERF_FRAMES then
        frame_times[sample_idx] = dt * 1000.0
    end

    if sample_idx % 100 == 0 and sample_idx <= PERF_FRAMES then
        local fps = metrics.get_fps()
        local dc = metrics.get_draw_calls()
        print(string.format("[CubeStress] frame=%d fps=%.1f draw_calls=%d ft=%.2fms",
            frame_idx, fps, dc, dt * 1000.0))
    end

    if sample_idx == PERF_FRAMES and not stats_printed then
        stats_printed = true
        print_perf_report()
        app.quit()
    end
end

--------------------------------------------------------------------------------
-- 性能报告
--------------------------------------------------------------------------------
function print_perf_report()
    local n = #frame_times
    if n == 0 then
        print("[CubeStress] No samples collected")
        return
    end

    local sorted = {}
    local sum = 0
    for i = 1, n do
        sorted[i] = frame_times[i]
        sum = sum + frame_times[i]
    end
    table.sort(sorted)

    local avg = sum / n
    local min_t = sorted[1]
    local max_t = sorted[n]
    local p50 = sorted[math.max(1, math.floor(n * 0.50))]
    local p95 = sorted[math.max(1, math.floor(n * 0.95))]
    local p99 = sorted[math.max(1, math.floor(n * 0.99))]

    local fps_avg = 1000.0 / avg
    local fps_min = 1000.0 / max_t
    local fps_max = 1000.0 / min_t

    print("======================================================================")
    print("  CUBE STRESS TEST — GPU THROUGHPUT")
    print("======================================================================")
    print(string.format("  Entities:    %d", ENTITY_COUNT))
    print(string.format("  Shadow:      %s", NO_SHADOW and "OFF" or "ON"))
    print(string.format("  Samples:     %d frames (after %d warmup)", n, warmup_frames))
    print(string.format("  Backend:     %s", os.getenv("DSE_RHI_BACKEND") or "default"))
    local gpu_active = metrics.get_gpu_driven_active and metrics.get_gpu_driven_active() or false
    local gpu_draws = metrics.get_gpu_indirect_draw_count and metrics.get_gpu_indirect_draw_count() or 0
    local gpu_instances = metrics.get_gpu_total_instances and metrics.get_gpu_total_instances() or 0
    print(string.format("  GPU-Driven:  %s (draws=%d, instances=%d)", gpu_active and "ACTIVE" or "INACTIVE", gpu_draws, gpu_instances))
    print(string.format("  Uniform Mat: %s", tostring(UNIFORM_MAT)))
    print("----------------------------------------------------------------------")
    print(string.format("  FPS avg:     %.1f", fps_avg))
    print(string.format("  FPS min:     %.1f", fps_min))
    print(string.format("  FPS max:     %.1f", fps_max))
    print("----------------------------------------------------------------------")
    print(string.format("  Frame Time avg:  %.2f ms", avg))
    print(string.format("  Frame Time min:  %.2f ms", min_t))
    print(string.format("  Frame Time max:  %.2f ms", max_t))
    print(string.format("  Frame Time p50:  %.2f ms", p50))
    print(string.format("  Frame Time p95:  %.2f ms", p95))
    print(string.format("  Frame Time p99:  %.2f ms", p99))
    print("======================================================================")
    print(string.format("DSE_PERF_RESULT entities=%d fps_avg=%.1f fps_min=%.1f ft_avg=%.2f ft_p99=%.2f shadow=%s uniform_mat=%s draw_calls=%d gpu_driven_active=%s gpu_indirect_draws=%d gpu_instances=%d",
        ENTITY_COUNT, fps_avg, fps_min, avg, p99, NO_SHADOW and "OFF" or "ON",
        tostring(UNIFORM_MAT), metrics.get_draw_calls(), tostring(gpu_active), gpu_draws, gpu_instances))
end
