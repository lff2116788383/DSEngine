-- New game HUD text using the generated bitmap font atlas (R4: equipment + inventory).
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
    U.combat = Text.new(24, 590, 0.85,0.95,0.75, 100000)
    U.toast = Text.new(420, 84, 1.0,0.88,0.55, 100001)
    U.title = Text.new(24, 34, 1.0,0.95,0.80, 100000)
    U.inv_title = Text.new(400, 560, 1.0,0.95,0.75, 100002)
    U.inv_lines = {}
    for i=1,8 do U.inv_lines[i] = Text.new(400, 520 - (i-1)*24, 0.95,0.95,0.95, 100002) end
    U.inv_hint = Text.new(400, 300, 0.85,0.88,0.95, 100002)
end
function U.update(dt, P, state)
    local s = P.stats()
    U.hp:set(string.format("生命 %d/%d", math.floor(P.hp), math.floor(s.hp_max)))
    U.mp:set(string.format("内力 %d/%d", math.floor(P.mp), math.floor(s.mp_max)))
    U.info:set(string.format("等级 %d  金钱 %d  攻击 %d  防御 %d  暴击 %d%%",
        P.level, P.gold, math.floor(s.atk), math.floor(s.def), math.floor(s.crit*100)))
    U.combat:set(string.format("连招 %d  分花 %0.1f  紫霞 %0.1f  装备 %d",
        P.combo, P.skill_cd.fenhua, P.skill_cd.zixia, #P.items))
    U.toast:set(Fx.msg_t > 0 and Fx.msg or "")
    U.title:set("青溪问剑  R4 战斗与成长")
    if state and state.inventory_open then
        U.inv_title:set(string.format("背包 %d 件", #P.items))
        for i,line in ipairs(U.inv_lines) do
            local item = P.items[i]
            if item then
                local mark = ""
                if P.equip[item.slot] == item then mark = " 已装备" end
                line:set(string.format("%s%s 战力%d%s", i == state.inv_sel and ">" or " ",
                    item.name, math.floor(require("loot").power(item)), mark))
            else line:set("") end
        end
        U.inv_hint:set("W/S 选择  J 装备")
    else
        U.inv_title:set("")
        for _,line in ipairs(U.inv_lines) do line:set("") end
        U.inv_hint:set("")
    end
    local _ = dt
end
return U