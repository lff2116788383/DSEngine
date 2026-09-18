-- New game asset loader.
local core = require("core")
local A = { atlas = {}, tex = {}, snd = {} }
local ecs = dse.ecs
local function tex(p) return dse.assets.load_texture_ex(core.resolve(p), "nearest", "clamp") end
local function actor_atlas(actor, dir)
    return dse.assets.load_sprite_atlas(core.resolve("assets/actor/"..actor.."_"..dir.."_atlas.dsprite.json"))
end
function A.load()
    for _,k in ipairs({"tree","bamboo","house","lantern","rock"}) do
        A.tex[k] = tex("assets/props/"..k..".png")
    end
    A.tex.font = tex("assets/ui/font.png")
    A.font = require("font")
    for _,actor in ipairs({"hero","bandit"}) do
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
    A.snd.bgm = core.resolve("assets/audio/bgm_loop.wav")
    print("[assets] new wuxia assets loaded")
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
return A