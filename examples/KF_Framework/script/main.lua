-- KF_Framework Demo — 主入口
-- Phase 1: 场景  |  Phase 2: Knight FSM  |  Phase 3: 摄像机跟随 + 跳跃
-- Phase 4: Mutant AI  |  Phase 5: 战斗系统  |  Phase 6: 游戏流程  |  Phase 7: 音效
--
--------------------------------------------------------------------------------
-- ⚠️ AI 注意: 以下转换规则区块为经过反复验证的最终结论，禁止修改！
--    如需调整场景，仅修改各模块中的具体数值，不要改动此头部注释。
--------------------------------------------------------------------------------
-- KF_Framework (DX9 LH)  ↔  DSEngine (OpenGL RH) 转换对照表
--------------------------------------------------------------------------------
-- ▌坐标系:  DSE 右手系, +X=右, +Y=上, -Z=前方
-- ▌位置转换: DSE_pos = KF_pos × 100, DSE_z = -KF_z  (LH→RH Z取反)
-- ▌模型比例: Mixamo FBX → Assimp 导入, 角色 ~172 单位高 (cm), 等效KF 1.7m
-- ▌缩放规则:
--     - 场景模型 (建筑/栅栏/岩石/桶/井): scale = 1.0 (FBX 与角色同为 cm 单位)
--     - Pine_tree 例外: scale = 0.1 (其 FBX 原始尺寸异常大 ~5000u, 实际应为 ~500u)
--     - KF demo.stage 中所有模型 scale 均为 (1,1,1)
-- ▌旋转转换: KF quaternion(x,y,z,w) → DSE euler_y = -原始Y轴旋转角度 (度)
-- ▌光照:    KF 方向 (-1,-4,+1) → DSE Z取反 (-1,-4,-1)
-- ▌摄像机:  KF distance=5, offsetY=3.5, pitch=15°
--            → DSE pos=(0, 350, 500), pitch=-15°, fov=60°
-- ▌运动:    KF moveSpeed=10 → DSE 1000;  jumpSpeed=20 → DSE 2000
-- ▌阴影:    KF offset=(20,80,-20), range=20, far=200
--            → DSE shadow_range=800, shadow_distance=3000, shadow_far=15000
--------------------------------------------------------------------------------
-- 数据来源: C:\Users\Administrator\Desktop\temp_analysis\KF_Framework
--   - data\stage\demo.stage (二进制: 18种模型, 精确 position/rotation/scale)
--   - source_code\camera\third_person_camera.h (摄像机参数)
--   - source_code\game_object\stage_spawner.cpp (场景加载逻辑)
--   - source_code\light\light.cpp (灯光颜色/方向)
--------------------------------------------------------------------------------
-- 模块拆分:
--   script/config.lua  — 全局配置 & 资产路径 & 游戏参数
--   script/scene.lua   — Phase 1 场景搭建
--   script/player.lua  — Phase 2+3 Knight 角色 + 摄像机
--   script/enemy.lua   — Phase 4 Mutant 敌人 AI
--------------------------------------------------------------------------------

local app = dse.app
local ecs = dse.ecs

-- 模块加载
local Config   = require("script.config")
local Scene    = require("script.scene")
local Player   = require("script.player")
local Enemy    = require("script.enemy")
local GameFlow = require("script.gameflow")
local Audio    = require("script.audio")
local HUD      = require("script.hud")
local Fade     = require("script.fade")
local AutoPlay = require("script.autoplay")
local TerrainHeight = require("script.terrain_height")

-- 风车旋转 (KF: WindmillController, rotate_speed_=0.1 rad/s)
local windmill_fans = {}    -- Fan entity list
local windmill_fan_orig_rot = {}  -- Fan 原始旋转 (rx, ry, rz)
local WINDMILL_SPEED = math.deg(0.1)  -- KF: 0.1 rad/s = 5.73°/s
local windmill_angle = 0    -- 累计旋转角度

-- KF: CollisionDetector::SphereCollision — 球体碰撞推开
local function resolve_sphere_collision(e1, r1, e2, r2)
    local x1, y1, z1 = ecs.get_transform_position(e1)
    local x2, y2, z2 = ecs.get_transform_position(e2)
    local dx = x2 - x1
    local dz = z2 - z1
    local dist = math.sqrt(dx * dx + dz * dz)
    local min_dist = r1 + r2
    if dist < min_dist and dist > 0.01 then
        local overlap = (min_dist - dist) * 0.5
        local nx = dx / dist
        local nz = dz / dist
        ecs.set_transform_position(e1, x1 - nx * overlap, y1, z1 - nz * overlap)
        ecs.set_transform_position(e2, x2 + nx * overlap, y2, z2 + nz * overlap)
    end
end

--------------------------------------------------------------------------------
-- Awake
--------------------------------------------------------------------------------
function Awake()
    app.set_window_title("KF_Framework Demo")
    app.set_data_root("examples/KF_Framework")

    Scene.setup()
    Audio.setup()
    TerrainHeight.setup()  -- C++ 高度图组件初始化（必须在 Player/Enemy 之前）
    Player.setup()

    -- 生成 4 只 Mutant (KF demo.enemy 原始位置 ×100, z取反)
    -- 每敌人独立参数来自 KF demo.enemy: warning_range, patrol_range, ActorParameter
    Enemy.spawn(929, 51, -2026, {    -- KF(9.29, 0.51, 20.26)
        warning_range = 1670, patrol_range = 2530, max_hp = 3, attack = 1, defence = 1})
    Enemy.spawn(8506, 1372, -4654, { -- KF(85.06, 13.72, 46.54)
        warning_range = 2200, patrol_range = 1430, max_hp = 5, attack = 2, defence = 1})
    Enemy.spawn(1777, 214, -2297, {  -- KF(17.77, 2.14, 22.97)
        warning_range = 1960, patrol_range = 2290, max_hp = 3, attack = 1, defence = 1})
    Enemy.spawn(3519, 0, -4338, {    -- KF(35.19, 0.00, 43.38)
        warning_range = 1960, patrol_range = 1550, max_hp = 3, attack = 1, defence = 1})

    HUD.setup()
    HUD.hide()  -- Title 状态下隐藏 HUD, enter_battle 时显示
    Fade.setup()

    -- 敌人 HP 条初始化
    Enemy.setup_hp_bars()

    -- 风车 Fan 实体查找 (KF: WindmillController 旋转 Fan 子物体)
    windmill_fans = ecs.find_entities_by_mesh_path("cooked/Fan_0.dmesh")
    for _, fan in ipairs(windmill_fans) do
        local rx, ry, rz = ecs.get_transform_rotation(fan)
        table.insert(windmill_fan_orig_rot, {rx, ry, rz})
    end
    if #windmill_fans > 0 then
        print("[KF_Framework] Windmill fans found: " .. #windmill_fans)
    end

    GameFlow.setup()  -- 进入 Title 状态 + title BGM + fade_in

    print("[KF_Framework] Phase 1~8 loaded. Title → Battle → Result → Title")
    print("[KF_Framework] Controls: WASD=move, Shift=run, Space=jump, LMB=attack, RMB=block, Q=kick, E=cast")
    print("[KF_Framework] F5=Toggle DemoPlay (AI auto-battle)")
end

--------------------------------------------------------------------------------
-- Update
--------------------------------------------------------------------------------
function Update(dt)
    if dt > 0.1 then dt = 0.1 end

    -- F5: DemoPlay 切换 (KF: ModeDemoPlay)
    if app.get_key_down(294) then  -- F5
        AutoPlay.toggle()
        print("[KF_Framework] DemoPlay: " .. (AutoPlay.is_enabled() and "ON" or "OFF"))
    end

    -- Fade 过渡更新 (KF: fade_system.cpp)
    Fade.update(dt)

    -- Phase 6: 游戏流程更新
    GameFlow.update(dt)

    -- 风车旋转 (KF: WindmillController::Update — 全状态运行, RotateByRoll)
    windmill_angle = windmill_angle + WINDMILL_SPEED * dt
    if windmill_angle > 360 then windmill_angle = windmill_angle - 360 end
    for i, fan in ipairs(windmill_fans) do
        local orig = windmill_fan_orig_rot[i]
        if orig then
            ecs.set_transform_rotation(fan, orig[1], orig[2], orig[3] + windmill_angle)
        end
    end

    -- Result 状态或 Fade 过渡中冻结游戏逻辑 (KF: TimeScale=0 during fade)
    if GameFlow.get_state() ~= GameFlow.STATE_BATTLE then
        Enemy.hide_hp_bars()
        return
    end
    if Fade.is_fading() then return end

    Player.update(dt)

    -- 敌人 AI 更新
    local px, py, pz = Player.get_position()
    Enemy.update_all(dt, px, py, pz)

    -- KF: CollisionDetector — 角色间球体碰撞
    local p_ent = Player.get_entity()
    local p_r = Config.PLAYER.collision_radius
    local e_r = Config.ENEMY.collision_radius
    for _, data in ipairs(Enemy.instances) do
        if data.state ~= "dead" then
            resolve_sphere_collision(p_ent, p_r, data.entity, e_r)
        end
    end
    for i = 1, #Enemy.instances do
        local a = Enemy.instances[i]
        if a.state ~= "dead" then
            for j = i + 1, #Enemy.instances do
                local b = Enemy.instances[j]
                if b.state ~= "dead" then
                    resolve_sphere_collision(a.entity, e_r, b.entity, e_r)
                end
            end
        end
    end

    -- 更新碰撞后的玩家位置
    px, py, pz = Player.get_position()

    -- 敌人 HP 条更新 (KF: EnemyUiController::Update — 使用摄像机位置计算距离)
    local cam_x, cam_y, cam_z = Player.get_camera_position()
    Enemy.update_hp_bars(cam_x, cam_y, cam_z)

    -- Phase 5: 战斗判定 — 敌人攻击命中玩家
    if not Player.is_dead() then
        local hits = Enemy.check_attacks(px, pz)
        for _, dmg in ipairs(hits) do
            Player.damage(dmg)
        end
    end

    -- Phase 5: 战斗判定 — 玩家攻击命中敌人
    if Player.is_attacking() then
        local attack_range = 250.0  -- 2.5m 攻击距离
        local yaw_rad = math.rad(Player.get_facing_yaw())
        -- 攻击方向前方扇形判定
        local fwd_x = math.sin(yaw_rad)
        local fwd_z = math.cos(yaw_rad)
        for _, data in ipairs(Enemy.instances) do
            if data.state ~= "dead" and not data.hit_this_attack then
                local ex, _, ez = dse.ecs.get_transform_position(data.entity)
                local dx = ex - px
                local dz = ez - pz
                local dist = math.sqrt(dx * dx + dz * dz)
                if dist < attack_range then
                    -- 前方120°扇形检测
                    local dot = (dx * fwd_x + dz * fwd_z) / (dist + 0.01)
                    if dot > 0.5 then  -- cos(60°) = 0.5
                        Enemy.damage(data, Config.PLAYER.attack)
                        data.hit_this_attack = true
                    end
                end
            end
        end
    else
        -- 攻击结束时重置命中标记
        for _, data in ipairs(Enemy.instances) do
            data.hit_this_attack = false
        end
    end

    -- HUD 更新
    HUD.update(Player.get_hp(), Config.PLAYER.max_hp, dt)

    -- Phase 6: 检查战斗结束条件
    GameFlow.check_battle_end(Player.is_dead(), Enemy.alive_count())

end
