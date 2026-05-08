--------------------------------------------------------------------------------
-- KF_Framework — 全局配置 & 常量
-- ⚠️ AI 注意: 转换规则已验证，禁止修改！
--------------------------------------------------------------------------------
-- KF_Framework (DX9 LH)  ↔  DSEngine (OpenGL RH) 转换对照表
-- ▌位置转换: DSE_pos = KF_pos × 100, DSE_z = -KF_z
-- ▌模型比例: FBX cm 单位, 角色 ~172u (1.7m)
-- ▌缩放规则: 场景模型 scale=1.0, Pine_tree scale=0.1
-- ▌旋转转换: DSE euler_y = -原始Y轴角度
-- ▌运动: KF moveSpeed=10 → DSE 1000
--------------------------------------------------------------------------------

local Config = {}

-- AnimConditionMode 枚举 (对应 C++ enum)
Config.COND_GREATER  = 0
Config.COND_LESS     = 1
Config.COND_EQUALS   = 2
Config.COND_NOTEQUAL = 3
Config.COND_IF       = 4
Config.COND_IFNOT    = 5

-- 资产路径
Config.ASSET = {
    -- Knight
    knight_mesh     = "cooked/paladin_prop_j_nordstrom.dmesh",
    knight_dskel    = "cooked/paladin_prop_j_nordstrom.dskel",
    knight_tex_diff = "assets/textures/Paladin_diffuse.png",
    knight_tex_norm = "assets/textures/Paladin_normal.png",
    knight_tex_spec = "assets/textures/Paladin_specular.png",
    -- Knight 动画
    anim_idle       = "cooked/Sword And Shield Idle.danim",
    anim_walk       = "cooked/Sword And Shield Walk.danim",
    anim_run        = "cooked/Sword And Shield Run.danim",
    anim_attack1    = "cooked/Sword And Shield Attack.danim",
    anim_attack2    = "cooked/Sword And Shield Attack (1).danim",
    anim_attack3    = "cooked/Sword And Shield Attack (2).danim",
    anim_block      = "cooked/Sword And Shield Block.danim",
    anim_block_idle = "cooked/Sword And Shield Block Idle.danim",
    anim_jump       = "cooked/Sword And Shield Jump.danim",
    anim_impact     = "cooked/Sword And Shield Impact.danim",
    anim_death      = "cooked/Sword And Shield Death.danim",
    anim_cast       = "cooked/Sword And Shield Casting.danim",
    anim_kick       = "cooked/Sword And Shield Kick.danim",
    -- Mutant (敌人)
    mutant_mesh     = "cooked/mutant.dmesh",
    mutant_dskel    = "cooked/mutant.dskel",
    mutant_tex_diff = "assets/textures/Mutant_diffuse.png",
    mutant_tex_norm = "assets/textures/Mutant_normal.png",
    mutant_idle     = "cooked/Mutant Breathing Idle.danim",
    mutant_walk     = "cooked/Mutant Walking.danim",
    mutant_run      = "cooked/Mutant Run.danim",
    mutant_punch    = "cooked/Mutant Punch.danim",
    mutant_swipe    = "cooked/Mutant Swiping.danim",
    mutant_dying    = "cooked/Mutant Dying.danim",
    mutant_roar     = "cooked/Mutant Roaring.danim",
    -- 场景
    skybox_pano     = "assets/textures/skybox000.jpg",
    ground_tex      = "assets/textures/demoField.jpg",
    baker_house     = "cooked/Baker_house.dmesh",
    med_house       = "cooked/Medieval house.dmesh",
    med_house1      = "cooked/Medieval_house_1.dmesh",
    windmill        = "cooked/Medieval_Windmill.dmesh",
    bridge          = "cooked/Medieval Bridge.dmesh",
    tavern          = "cooked/Fancy_Tavern.dmesh",
    fence           = "cooked/Fence.dmesh",
    barrel          = "cooked/Barrel.dmesh",
    well            = "cooked/cartoon_well.dmesh",
    rock1           = "cooked/rock_1.dmesh",
    rock2           = "cooked/rock_2.dmesh",
    rock3           = "cooked/rock_3.dmesh",
    rock4           = "cooked/rock_4.dmesh",
    pine_tree       = "cooked/Pine_tree.dmesh",
    -- 场景纹理
    tex_baker       = "assets/textures/Baker_house.jpg",
    tex_tavern      = "assets/textures/Fancy_Tavern.jpg",
    tex_windmill    = "assets/textures/WindmillAtlas.tga",
    tex_barrel      = "assets/textures/Barrel.jpg",
    tex_rock        = "assets/textures/RockCliff.jpg",
    tex_pine        = "assets/textures/Pine_tree.png",
    tex_fence       = "assets/textures/Medieval house_wood.jpg",
    tex_med_house1  = "assets/textures/Medieval_house_1_House_D.tga",
    -- 音频 (Phase 7)
    bgm_title       = "assets/audio/bgm/title.wav",
    bgm_game        = "assets/audio/bgm/game.wav",
    bgm_result      = "assets/audio/bgm/result.wav",
    se_submit        = "assets/audio/se/submit.wav",
    se_cursor        = "assets/audio/se/cursor.wav",
    se_sord_attack   = "assets/audio/se/sord_attack.wav",
    se_attack_voice1 = "assets/audio/se/attack_voice_1.wav",
    se_attack_voice2 = "assets/audio/se/attack_voice_2.wav",
    se_attack_voice3 = "assets/audio/se/attack_voice_3.wav",
    se_block         = "assets/audio/se/block.wav",
    se_damage_voice1 = "assets/audio/se/damage_voice_1.wav",
    se_damage_voice2 = "assets/audio/se/damage_voice_2.wav",
    se_death_voice   = "assets/audio/se/death_voice.wav",
    se_zombie_beat   = "assets/audio/se/zombie_beat.wav",
    se_zombie_warning= "assets/audio/se/zombie_warning.wav",
    se_guard_voice   = "assets/audio/se/guard_voice.wav",
    se_zombie_death  = "assets/audio/se/zombie_death.wav",
}

-- 玩家参数 (KF原始 ×100)
Config.PLAYER = {
    move_speed  = 800.0,
    run_speed   = 1400.0,
    turn_speed  = 720.0,
    jump_speed  = 2000.0,
    gravity     = -4900.0,  -- 0.5g ≈ -4.9m/s² × 100²
    max_hp      = 100,
    attack      = 15,
    defence     = 3,
    invincible_time = 0.5,  -- KF: kInvincibleTime = 0.5f
}

-- 敌人参数
Config.ENEMY = {
    move_speed    = 400.0,
    turn_speed    = 360.0,
    detect_range  = 1000.0,  -- KF: warning_range_=10.0f × 100
    attack_range  = 200.0,   -- 2m × 100
    lose_range    = 2500.0,  -- 25m × 100
    max_hp        = 30,
    attack        = 8,
    defence       = 1,
}

-- 摄像机参数 (KF third_person_camera.h + camera.cpp)
Config.CAMERA = {
    distance     = 500.0,   -- kDistanceDefault=5.0 × 100
    offset_y     = 350.0,   -- kOffsetY=3.5 × 100
    pitch        = -15.0,   -- kPitchDefault=15° (DSE 负值=俯视)
    fov          = 75.0,    -- camera.cpp: fov_=75°/180°×π
    lerp_speed   = 8.0,     -- ~kMoveLerpTime=0.075 对应
    near_clip    = 10.0,    -- 0.1m × 100
    far_clip     = 100000.0,-- 1000m × 100
}

return Config
