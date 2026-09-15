-- ============================================================================
-- audio.lua  BGM / 音效快捷封装（注意 play_sfx 的 loop 形参在引擎侧是 int）
-- ============================================================================
local A = require("assets")
local AU = { cur = nil }

function AU.bgm(key, vol)
    if AU.cur == key then return end
    local path = A.snd[key]
    if not path then return end
    AU.cur = key
    dse.audio.play_bgm(path, vol or 0.55, true)
end

function AU.sfx(key, vol)
    local path = A.snd[key]
    if path then dse.audio.play_sfx(path, vol or 0.7, 0) end
end

function AU.random(key, vol)
    local path = A.snd[key]
    if path then dse.audio.play_sfx_random(path, vol or 0.6, 0.92, 1.12) end
end

function AU.stop() dse.audio.stop_bgm() AU.cur = nil end
return AU