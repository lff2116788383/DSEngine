-- stress_test — 大规模实例化性能压测
-- 用法:
--   DSE_ENTITY_COUNT=500  → 生成 500 个实体 (默认 100)
--   DSE_ANIM_ENABLED=1    → 启用骨骼动画 (默认 1)
--   DSE_PERF_FRAMES=600   → 统计帧数 (默认 600)
--------------------------------------------------------------------------------

local app = dse.app
local ecs = dse.ecs
local metrics = dse.metrics

--------------------------------------------------------------------------------
-- 配置
--------------------------------------------------------------------------------
local ENTITY_COUNT   = tonumber(os.getenv("DSE_ENTITY_COUNT") or "50")
local ANIM_ENABLED   = (os.getenv("DSE_ANIM_ENABLED") or "1") ~= "0"
local PERF_FRAMES    = tonumber(os.getenv("DSE_PERF_FRAMES") or "600")

-- 资产路径 (复用 KF_Framework 的 cooked 资产)
local KNIGHT_MESH    = "cooked/paladin_prop_j_nordstrom.dmesh"
local KNIGHT_SKEL    = "cooked/paladin_prop_j_nordstrom.dskel"
local KNIGHT_TEX     = "assets/textures/Paladin_diffuse.png"
local KNIGHT_NORM    = "assets/textures/Paladin_normal.png"
local IDLE_ANIM      = "cooked/Sword And Shield Idle.danim"
local RUN_ANIM       = "cooked/Sword And Shield Run.danim"
local GROUND_TEX     = "assets/textures/demoField.jpg"
local SKYBOX_TEX     = "assets/textures/skybox000.jpg"

-- 性能统计
local frame_times    = {}
local frame_idx      = 0
local warmup_frames  = 30     -- 跳过前 30 帧的初始化开销
local stats_printed  = false
local last_clock     = nil

--------------------------------------------------------------------------------
-- 辅助: NxN 网格排列
--------------------------------------------------------------------------------
local function grid_layout(count)
    local cols = math.ceil(math.sqrt(count))
    return cols
end

local function grid_position(index, cols, spacing)
    local row = math.floor(index / cols)
    local col = index % cols
    local cx = (cols - 1) * spacing * 0.5
    local cz = (math.ceil(ENTITY_COUNT / cols) - 1) * spacing * 0.5
    return col * spacing - cx, 0, -(row * spacing - cz)
end

--------------------------------------------------------------------------------
-- Awake
--------------------------------------------------------------------------------
function Awake()
    app.set_window_title("Stress Test — " .. ENTITY_COUNT .. " entities")
    app.set_data_root("examples/KF_Framework")

    print(string.format("[StressTest] entities=%d anim=%s perf_frames=%d",
        ENTITY_COUNT, tostring(ANIM_ENABLED), PERF_FRAMES))

    -- 1. 方向光 + 阴影
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1 + 16 + 1)
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,
        0.8, 0.8, 0.8, 1.0, 0.50, 1.0)
    ecs.set_directional_light_shadow(sun, true, 1.0, 800, 4000, 20000)

    -- 2. Sky light
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0)
    ecs.add_sky_light(sky, 0.38, 0.45, 0.55, 0.12, 0.11, 0.10, 0.3)

    -- 3. Skybox
    local skybox = ecs.create_entity()
    ecs.add_transform(skybox, 0, 0, 0)
    ecs.set_transform_rotation(skybox, 0, 180, 0)
    ecs.add_skybox(skybox, SKYBOX_TEX)

    -- 4. 地面 (大尺寸)
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, -1, 0, 400, 400, 400)
    ecs.add_mesh_renderer(ground, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(ground, "cooked/demoField.dmesh")
    ecs.set_mesh_shader_variant(ground, "MESH_HALFLAMBERT_STATIC")
    ecs.set_mesh_material(ground, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, true, false)
    ecs.set_mesh_texture(ground, "albedo", GROUND_TEX)

    -- 5. 大量骑士实体
    local cols = grid_layout(ENTITY_COUNT)
    local spacing = 300  -- 3m 间距
    local anims = { IDLE_ANIM, RUN_ANIM }

    for i = 0, ENTITY_COUNT - 1 do
        local x, y, z = grid_position(i, cols, spacing)
        local e = ecs.create_entity()
        ecs.add_transform(e, x, y, z, 1, 1, 1)
        ecs.set_transform_rotation(e, 0, 0, 0)
        ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 1.0)
        ecs.set_mesh_path(e, KNIGHT_MESH)
        ecs.set_mesh_material(e, 0.3, 0.7, 1.0, 0.0, 0.0, 0.0, 1.0, true, false)
        ecs.set_mesh_texture(e, "albedo", KNIGHT_TEX)
        ecs.set_mesh_texture(e, "normal", KNIGHT_NORM)

        if ANIM_ENABLED then
            -- 交替 idle/run 动画
            local anim = anims[(i % #anims) + 1]
            ecs.add_animator_3d(e, anim, KNIGHT_SKEL)
        end
    end

    -- 6. 正面军团视角 — 摄像机在+Z后方, yaw=0看向-Z(引擎默认前方)
    local cam = ecs.create_entity()
    local rows = math.ceil(ENTITY_COUNT / cols)
    local grid_w = cols * spacing
    local grid_d = rows * spacing
    local cam_dist = math.max(grid_w * 0.7, 2000)
    local cam_h = 350
    local cam_z = grid_d * 0.5 + cam_dist
    ecs.add_transform(cam, 0, cam_h, cam_z)
    -- look-at 网格中心 (0, 80, 0)
    local dy = 80 - cam_h
    local dz = 0 - cam_z  -- 负值(向-Z)
    local dist_xz = math.abs(dz)
    local pitch = math.deg(math.atan(dy, dist_xz))
    ecs.set_transform_rotation(cam, pitch, 0, 0)
    ecs.add_camera_3d(cam, 60.0, 0, 1.0, 50000.0)

    print(string.format("[StressTest] Spawned %d knights in %dx%d grid (spacing=%d)",
        ENTITY_COUNT, cols, math.ceil(ENTITY_COUNT / cols), spacing))
end

--------------------------------------------------------------------------------
-- Update — 性能采集
--------------------------------------------------------------------------------
function Update(dt)
    frame_idx = frame_idx + 1

    -- 跳过 warmup
    if frame_idx <= warmup_frames then return end

    local sample_idx = frame_idx - warmup_frames
    if sample_idx <= PERF_FRAMES then
        -- 使用 unclamped wall-clock 差值（engine dt 被 clamp 到 100ms）
        local now = os.clock() * 1000.0  -- ms
        if last_clock then
            frame_times[sample_idx] = now - last_clock
        else
            frame_times[sample_idx] = dt * 1000.0
        end
        last_clock = now
    end

    -- 每 100 帧输出一次实时 FPS
    if sample_idx % 100 == 0 and sample_idx <= PERF_FRAMES then
        local fps = metrics.get_fps()
        local dc = metrics.get_draw_calls()
        print(string.format("[StressTest] frame=%d fps=%.1f draw_calls=%d frame_time=%.2fms",
            frame_idx, fps, dc, dt * 1000.0))
    end

    -- 统计完成, 输出报告并自动退出
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
        print("[StressTest] No samples collected")
        return
    end

    -- 排序用于百分位数
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
    local p50 = sorted[math.floor(n * 0.50)]
    local p95 = sorted[math.floor(n * 0.95)]
    local p99 = sorted[math.floor(n * 0.99)]

    local fps_avg = 1000.0 / avg
    local fps_min = 1000.0 / max_t   -- worst frame → min fps
    local fps_max = 1000.0 / min_t   -- best frame → max fps

    print("======================================================================")
    print("  STRESS TEST PERFORMANCE REPORT")
    print("======================================================================")
    print(string.format("  Entities:    %d", ENTITY_COUNT))
    print(string.format("  Animation:   %s", tostring(ANIM_ENABLED)))
    print(string.format("  Samples:     %d frames (after %d warmup)", n, warmup_frames))
    print(string.format("  Backend:     %s", os.getenv("DSE_RHI_BACKEND") or "default"))
    print(string.format("  GPU-Driven:  %s", os.getenv("DSE_DISABLE_GPU_DRIVEN") == "1" and "OFF" or "ON"))
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
    print(string.format("DSE_PERF_RESULT entities=%d fps_avg=%.1f fps_min=%.1f ft_avg=%.2f ft_p99=%.2f",
        ENTITY_COUNT, fps_avg, fps_min, avg, p99))
end
