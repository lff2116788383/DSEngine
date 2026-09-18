-- New game asset loader. Prefers imported CC0 Ninja Adventure assets, falls back to generated ones.
local core = require("core")
local A = { atlas = {}, tex = {}, snd = {} }
local ecs = dse.ecs
local function tex(p) return dse.assets.load_texture_ex(core.resolve(p), "nearest", "clamp") end
local function first_existing(paths)
    for _,p in ipairs(paths) do
        local r = core.resolve(p)
        if core.file_exists(r) then return r end
    end
    return core.resolve(paths[#paths])
end
local function actor_atlas(actor, dir)
    local p = first_existing({
        "assets/external/ninja_adventure/actor/"..actor.."_"..dir.."_atlas.dsprite.json",
        "assets/actor/"..actor.."_"..dir.."_atlas.dsprite.json",
    })
    return dse.assets.load_sprite_atlas(p)
end
function A.load()
    for _,k in ipairs({"tree","bamboo","house","lantern","rock"}) do
        A.tex[k] = tex("assets/props/"..k..".png")
    end
    for _,k in ipairs({"rain","snow","fog","leaf"}) do
        A.tex["weather_"..k] = tex("assets/weather/"..k..".png")
    end
    A.tex.font = tex("assets/ui/font.png")
    A.font = require("font")
    for _,actor in ipairs({"hero","bandit","boss"}) do
        for _,dir in ipairs({"d","u","l","r"}) do
            local h = actor_atlas(actor, dir)
            if not h or h < 0 then print("[assets] missing atlas "..actor.."_"..dir) end
            A.atlas[actor.."_"..dir] = h
        end
    end
    A.snd.slash = core.resolve("assets/audio/sfx_slash.wav")
    A.snd.hit = core.resolve("assets/audio/sfx_hit.wav")
    A.snd.pickup = core.resolve("assets/audio/sfx_pickup.wav")
    A.snd.levelup = core.resolve("assets/audio/sfx_levelup.wav")
    A.snd.click = core.resolve("assets/audio/sfx_click.wav")
    A.snd.bgm_plain = first_existing({"assets/external/ninja_adventure/audio/theme_plain.ogg", "assets/audio/bgm_loop.wav"})
    A.snd.bgm_swamp = first_existing({"assets/external/ninja_adventure/audio/theme_swamp.ogg", "assets/audio/bgm_loop.wav"})
    A.snd.bgm_lost = first_existing({"assets/external/ninja_adventure/audio/theme_lost_village.ogg", "assets/audio/bgm_loop.wav"})
    A.snd.bgm_dream = first_existing({"assets/external/ninja_adventure/audio/theme_dream.ogg", "assets/audio/bgm_loop.wav"})
    A.snd.bgm = A.snd.bgm_plain
    print("[assets] new wuxia assets loaded (ninja external preferred)")
end
function A.play(e, actor, dir, action, fps, loop)
    local h = A.atlas[actor.."_"..dir]
    if not h or h < 0 then return end
    ecs.set_sprite3d_atlas(e, h, action)
    ecs.set_sprite3d_anim(e, action, { fps = fps or 8.0, loop = loop and true or false })
end
function A.sfx(name, vol)
    local p = A.snd[name]
    if p then dse.audio.play_sfx(p, vol or 0.6, 0) end
end
function A.play_bgm(track)
    local p = A.snd["bgm_"..(track or "plain")]
    if p then dse.audio.play_bgm(p, 0.32, 1) end
end
return A