-- KF_Framework Demo — Phase 1: 场景搭建
-- 入口脚本：相机 + 灯光 + 地面 + Knight 模型 + Idle 动画
--
--------------------------------------------------------------------------------
-- KF_Framework (DX9 LH)  ↔  DSEngine (OpenGL RH) 转换对照表
--------------------------------------------------------------------------------
--
-- ▌坐标系
--   KF:  左手坐标系 (DirectX 9), +X=右, +Y=上, +Z=前方(屏幕内)
--   DSE: 右手坐标系 (OpenGL),    +X=右, +Y=上, -Z=前方(屏幕内), +Z=朝观察者
--   转换: pos_dse = (kf.x, kf.y, -kf.z)
--
-- ▌旋转 (Euler, degrees)
--   KF  pitch>0 → 向下看;  DSE pitch<0 → 向下看
--   转换: pitch_dse = -pitch_kf,  yaw_dse = -yaw_kf
--
-- ▌模型比例
--   KF:  自定义 .model/.skin, 角色约 1.7 单位高 (米)
--   DSE: Assimp 导入 Mixamo FBX, 保持 cm, 角色 ~172 单位高
--   缩放因子 S = 172 / 1.7 ≈ 101  (本文件取 S=100 近似)
--
-- ▌相机 (KF ThirdPersonCamera → DSE FreeCam 静态等价位)
--   KF参数: distance=5, offsetY=3.5, pitch=15°, fov=75°, near=0.1, far=1000
--   eye_local = (0, 0, -dist) → 经 pivot 旋转+平移:
--     eye_kf = (0, offsetY + dist*sin(pitch), -dist*cos(pitch))
--            = (0, 3.5+1.29, -4.83) = (0, 4.79, -4.83)
--   DSE等价 (×S, Z取反):
--     eye_dse = (0, 479, 483),  pitch_dse = -15°,  fov=75°
--     near=0.1*S=10, far=1000*S=100000
--
-- ▌光照
--   KF:  方向 (-1,-4,+1).norm,  diffuse=(0.8,0.8,0.8), ambient=gray
--   DSE: Z取反 → 方向 (-1,-4,-1).norm
--
-- ▌运动参数 (×S)
--   KF moveSpeed=10, jumpSpeed=20
--   DSE moveSpeed=1000, jumpSpeed=2000
--
-- ▌碰撞体 (×S)
--   KF sphere r=0.6, offsetY=0.8
--   DSE sphere r=60, offsetY=80
--------------------------------------------------------------------------------

local app   = dse.app
local ecs   = dse.ecs
local audio = dse.audio

--------------------------------------------------------------------------------
-- 资产路径常量
--------------------------------------------------------------------------------
local KNIGHT_MESH  = "cooked/paladin_prop_j_nordstrom.dmesh"
local KNIGHT_DMAT  = "cooked/paladin_prop_j_nordstrom.dmat"
local KNIGHT_DSKEL = "cooked/paladin_prop_j_nordstrom.dskel"
-- 骨骼名称重映射 (dskel/danim v2) 已实现，Sword And Shield 52 骨骼动画
-- 可正确映射到 Paladin 54 骨骼模型。
local KNIGHT_ANIM_IDLE  = "cooked/Sword And Shield Idle.danim"
local KNIGHT_ANIM_WALK  = "cooked/Sword And Shield Walk.danim"
local KNIGHT_ANIM_RUN   = "cooked/Sword And Shield Run.danim"
local KNIGHT_ANIM_ATTACK = "cooked/Sword And Shield Attack.danim"

local KNIGHT_TEX_DIFFUSE = "assets/textures/Paladin_diffuse.png"
local KNIGHT_TEX_NORMAL  = "assets/textures/Paladin_normal.png"

--------------------------------------------------------------------------------
-- 实体句柄
--------------------------------------------------------------------------------
local camera
local sun
local sky_light
local ground
local knight
local post_process

--------------------------------------------------------------------------------
-- Awake — 引擎启动时调用一次
--------------------------------------------------------------------------------
function Awake()
    app.set_window_title("KF_Framework Demo")
    app.set_data_root("examples/KF_Framework")

    -- 1. Camera (第三人称: 模型172单位高, 距离300, 俯角15°)
    camera = ecs.create_entity()
    ecs.add_transform(camera, 0, 160, 290)
    ecs.set_transform_rotation(camera, -15, 0, 0)
    ecs.add_camera_3d(camera, 60, 0, 10, 100000)
    ecs.add_free_camera_controller(camera, 200.0, 0.15)

    -- 2. Directional light (KF: (-1,-4,+1)→DSE Z取反: (-1,-4,-1))
    sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1+16+1) -- ≈ 4.24
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,  -- direction (KF Z取反)
         0.8,  0.8,  0.8,           -- diffuse color (KF原始)
         1.5,                        -- intensity
         0.3,                        -- ambient
         0.35                        -- shadow strength
    )
    ecs.set_directional_light_shadow(sun, true, 0.4, 500, 2000, 8000)

    -- 3. Sky light (hemisphere ambient)
    sky_light = ecs.create_entity()
    ecs.add_transform(sky_light, 0, 0, 0)
    ecs.add_sky_light(sky_light,
        0.25, 0.30, 0.40,   -- upper hemisphere
        0.05, 0.06, 0.08,   -- lower hemisphere
        1.0                  -- intensity
    )

    -- 4. Ground plane (引擎示例同款cube地面, ×100缩放)
    ground = ecs.create_entity()
    ecs.add_transform(ground, 0, -6, 0, 1100, 12, 700)
    ecs.add_mesh_renderer(ground, 0.32, 0.38, 0.36, 1.0,
        { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
          -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 },
        { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 })
    ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    ecs.set_mesh_material(ground, 0.0, 0.55, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)

    -- 5. Knight (参照引擎示例 3d_character_third_person.lua)
    knight = ecs.create_entity()
    ecs.add_transform(knight, 0, 0, 0)
    ecs.add_mesh_renderer(knight, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(knight, KNIGHT_MESH)
    ecs.set_mesh_shader_variant(knight, "MESH_LIT")
    ecs.set_mesh_material(knight, 0.0, 0.50, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)

    -- 手动绑定纹理（.dmat 中路径是无效的绝对路径）
    ecs.set_mesh_texture(knight, "albedo", KNIGHT_TEX_DIFFUSE)
    ecs.set_mesh_texture(knight, "normal", KNIGHT_TEX_NORMAL)

    -- Animation: Idle 动画 (骨骼名称重映射: 52ch → 54 bones)
    ecs.add_animator_3d(knight, KNIGHT_ANIM_IDLE, KNIGHT_DSKEL)

    -- 6. Post-processing (bloom)
    post_process = ecs.create_entity()
    ecs.add_post_process(post_process, true, 1.0, 0.8, 1.0)
    ecs.set_post_process_color(post_process, true, 1.0, 2.2)

    print("[KF_Framework] Scene loaded.")
end

--------------------------------------------------------------------------------
-- Update — 每帧调用
--------------------------------------------------------------------------------
function Update(dt)
    -- 后续 Phase 将在这里添加角色控制逻辑
end
