-- New game HUD text using the generated bitmap font atlas.
local A = require("assets")
local Fx = require("fx")
local U = { texts = {} }
local ecs = dse.ecs
local function make_obj(gl, tex, x, y, r, g, b, order)
    local e = ecs.create_entity()
    ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
    dse.ui.add_renderer(e, tex, r, g, b, 1.0, order or 100000, gl.w, gl.h)
    dse.ui.set_anchor(e, 0, 0)
    dse.ui.set_uv(e, gl.u0, gl.v0, gl.u1, gl.v1)
    dse.ui.set_position(e, x + gl.ox + gl.w * 0.5, y + gl.oy - gl.h * 0.5)
    return e
end
local Text = {}
Text.__index = Text
function Text.new(x, y, cr, cg, cb, order)
    return setmetatable({x=x,y=y,cr=cr or 1,cg=cg or 1,cb=cb or 1,order=order or 100000,objs={},str="!"}, Text)
end
function Text:set(str)
    str = str or ""
    if str == self.str then return end
    self.str = str
    for _,e in ipairs(self.objs) do pcall(ecs.destroy_entity, e) end
    self.objs = {}
    local pen = self.x
    for _,cp in utf8.codes(str) do
        local gl = A.font.glyphs[cp]
        if gl then
            local e = make_obj(gl, A.tex.font, pen, self.y, self.cr, self.cg, self.cb, self.order)
            self.objs[#self.objs+1] = e
            pen = pen + gl.adv
        end
    end
end
function U.init()
    U.hp = Text.new(24, 680, 0.95,0.45,0.45, 100000)
    U.mp = Text.new(24, 650, 0.55,0.75,1.0, 100000)
    U.info = Text.new(24, 620, 1.0,0.92,0.70, 100000)
    U.toast = Text.new(420, 84, 1.0,0.88,0.55, 100001)
    U.title = Text.new(24, 34, 1.0,0.95,0.80, 100000)
end
function U.update(dt, P)
    U.hp:set(string.format("生命 %d/%d", P.hp, P.hp_max))
    U.mp:set(string.format("内力 %d/%d", P.mp, P.mp_max))
    U.info:set(string.format("等级 %d  金币 %d  装备 %d", P.level, P.gold, #P.items))
    U.toast:set(Fx.msg_t > 0 and Fx.msg or "")
    U.title:set("青溪问剑  新作 R3 垂直切片")
    local _ = dt
end
return U