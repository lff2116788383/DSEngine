--------------------------------------------------------------------------------
-- KF_Framework — 音效管理 (Phase 7)
-- 参考: KF source_code/sound/sound_system.h (SE/BGM 标签)
-- 所有 SE/BGM 对应 KF 原始 wav 文件
--------------------------------------------------------------------------------
local Config = require("script.config")
local ASSET = Config.ASSET

local ecs   = dse.ecs
local audio = dse.audio

local Audio = {}

-- 音频实体 (lazy create, 共用位置)
local entities = {}

-- 音效注册表
local SE = {
    sord_attack    = ASSET.se_sord_attack,
    attack_voice1  = ASSET.se_attack_voice1,
    attack_voice2  = ASSET.se_attack_voice2,
    attack_voice3  = ASSET.se_attack_voice3,
    block          = ASSET.se_block,
    guard_voice    = ASSET.se_guard_voice,
    damage_voice1  = ASSET.se_damage_voice1,
    damage_voice2  = ASSET.se_damage_voice2,
    death_voice    = ASSET.se_death_voice,
    zombie_beat    = ASSET.se_zombie_beat,
    zombie_warning = ASSET.se_zombie_warning,
    zombie_death   = ASSET.se_zombie_death,
    submit         = ASSET.se_submit,
    cursor         = ASSET.se_cursor,
}

local BGM = {
    title  = ASSET.bgm_title,
    game   = ASSET.bgm_game,
    result = ASSET.bgm_result,
}

local current_bgm = nil

--------------------------------------------------------------------------------
-- 初始化: 预创建所有 SE + BGM 实体
--------------------------------------------------------------------------------
function Audio.setup()
    -- SE entities (不循环)
    for name, path in pairs(SE) do
        local e = ecs.create_entity()
        ecs.add_transform(e, 0, 0, 0)
        audio.add_source(e, path, false, false, 0.6)
        entities[name] = e
    end
    -- BGM entities (循环)
    for name, path in pairs(BGM) do
        local e = ecs.create_entity()
        ecs.add_transform(e, 0, 0, 0)
        audio.add_source(e, path, false, true, 0.4)
        entities["bgm_" .. name] = e
    end
end

--------------------------------------------------------------------------------
-- 播放音效 (SE)
--------------------------------------------------------------------------------
function Audio.play_se(name)
    local e = entities[name]
    if e then
        audio.restart(e)
    end
end

-- 播放攻击语音 — KF 源码: step1→voice1, step2→voice2, step3→voice3
function Audio.play_attack_voice(step)
    if step and step >= 1 and step <= 3 then
        Audio.play_se("attack_voice" .. step)
    else
        Audio.play_se("attack_voice" .. math.random(1, 3))
    end
end

-- 随机播放受伤语音 (KF: kDamageVoice1Se ~ kDamageVoice2Se)
function Audio.play_damage_voice()
    local r = math.random(1, 2)
    Audio.play_se("damage_voice" .. r)
end

--------------------------------------------------------------------------------
-- BGM 控制
--------------------------------------------------------------------------------
function Audio.play_bgm(name)
    -- 停止当前 BGM
    if current_bgm and entities["bgm_" .. current_bgm] then
        audio.set_playing(entities["bgm_" .. current_bgm], false)
    end
    current_bgm = name
    local e = entities["bgm_" .. name]
    if e then
        audio.restart(e)
    end
end

function Audio.stop_bgm()
    if current_bgm and entities["bgm_" .. current_bgm] then
        audio.set_playing(entities["bgm_" .. current_bgm], false)
    end
    current_bgm = nil
end

return Audio
