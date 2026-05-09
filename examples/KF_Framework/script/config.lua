--------------------------------------------------------------------------------
-- KF_Framework — 全局配置 & 常量
-- ⚠️ AI 注意: 转换规则已验证，禁止修改！
--------------------------------------------------------------------------------
-- KF_Framework (DX9 LH)  ↔  DSEngine (OpenGL RH) 转换对照表
-- ▌位置转换: DSE_pos = KF_pos × 100, DSE_z = -KF_z
-- ▌模型比例: FBX cm 单位, 角色 ~172u (1.7m), KF .model RootNode scale=0.02 → DSE scale=2.0
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
    -- 场景 (场景装饰物从 scenes/kf_demo_stage.json 加载, 纹理在 scene.lua tex_map)
    skybox_pano     = "assets/textures/skybox000.jpg",
    ground_tex      = "assets/textures/demoField.jpg",
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

-- 玩家参数 (KF: CreatePlayer, actor_parameter.h)
-- 空间值 ×100, 非空间值保持原始
Config.PLAYER = {
    move_speed  = 1000.0,   -- KF: 10.0 × 100
    run_speed   = 1400.0,   -- KF 无 run (knight 没有 run state但 DSE 保留)
    turn_speed  = 720.0,    -- KF: minTurn=π, maxTurn=2π (DSE 简化为度/秒)
    jump_speed  = 2000.0,   -- KF: 20.0 × 100
    gravity     = -4900.0,  -- KF: gravity_multiplier=4.0 × 9.8 × 100
    max_hp      = 10,       -- KF: ActorParameter default max_life_=10
    attack      = 1,        -- KF: ActorParameter default attack_=1
    defence     = 1,        -- KF: ActorParameter default defence_=1
    invincible_time = 0.5,  -- KF: kInvincibleTime = 0.5f
    collision_radius = 50.0, -- KF: kCollisionRadius ~0.5 × 100
}

-- 敌人参数 (KF: EnemyController + EnemyZombieFollowState)
-- follow_state: kMovementMultiplier=0.1, move_speed=10 → effective=1.0 KF → 100 DSE
-- attack_range: kAttackRange=2.0 → 200 DSE
-- lose_range: warning_range * kWarningRangeMultiplier(1.5) (per-enemy)
Config.ENEMY = {
    move_speed    = 100.0,   -- KF: 0.1 * 10.0 × 100
    turn_speed    = 360.0,
    detect_range  = 1670.0,  -- KF: 默认 warning_range_=10 × 100, 但每敌人独立
    attack_range  = 200.0,   -- KF: kAttackRange=2.0 × 100
    lose_range    = 2500.0,  -- KF: warning_range * 1.5 × 100, 每敌人独立
    max_hp        = 3,       -- KF: demo.enemy param[0]=3 (默认敌人)
    attack        = 1,       -- KF: demo.enemy param[2]=1
    defence       = 1,       -- KF: demo.enemy param[3]=1
    collision_radius = 50.0, -- KF: kCollisionRadius ~0.5 × 100
}

-- 摄像机参数 (KF third_person_camera.h + camera.cpp)
Config.CAMERA = {
    distance     = 500.0,   -- kDistanceDefault=5.0 × 100
    offset_y     = 350.0,   -- kOffsetY=3.5 × 100
    pitch        = -15.0,   -- kPitchDefault=15° (DSE 负值=俯视)
    fov          = 75.0,    -- camera.cpp: fov_=75°/180°×π
    lerp_speed   = 4.5,     -- kMoveLerpTime=0.075 × 60fps = 4.5
    near_clip    = 10.0,    -- 0.1m × 100
    far_clip     = 100000.0,-- 1000m × 100
}

return Config
