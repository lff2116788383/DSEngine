-- New game enemies (R4): normal/elite/champion plus multi-phase boss.
local core = require("core")
local A = require("assets")
local D = require("data")
local Fx = require("fx")
local Loot = require("loot")
local E = { list = {} }
local ecs = dse.ecs
local function solid(x,z)
    local col,row=math.floor(x)+1,math.floor(z)+1
    if row<1 or row>D.h or col<1 or col>D.w then return true end
    return D.solid[row][col]==true
end
local function set_dir(e,dir)
    if e.dir==dir then return end
    e.dir=dir
    e.play(e, e.action, true)
end
function E.spawn(kind,x,z)
    local cfg=D.enemy[kind]
    if not cfg then return nil end
    local actor = cfg.actor or "bandit"
    local ent=ecs.create_entity()
    ecs.add_transform(ent,x,0,z,1,1,1)
    ecs.add_sprite3d(ent,0,cfg.size[1]/32.0,cfg.size[2]/32.0,{billboard="yaw",anchor=0.0,lit=true,receive_shadow=true})
    ecs.set_sprite3d_lit(ent,true)
    ecs.set_sprite3d_receive_shadow(ent,true)
    ecs.set_sprite3d_contact_shadow(ent,true,cfg.rank=="boss" and 1.2 or 0.4,0.42)
    if cfg.rank=="elite" then ecs.set_sprite3d_emissive(ent,0.05,0.10,0.22)
    elseif cfg.rank=="champion" then ecs.set_sprite3d_emissive(ent,0.25,0.18,0.03)
    elseif cfg.rank=="boss" then ecs.set_sprite3d_emissive(ent,0.28,0.04,0.02) end
    local e={kind=kind,cfg=cfg,actor=actor,ent=ent,x=x,z=z,home_x=x,home_z=z,hp=cfg.hp,hp_max=cfg.hp,
             base_atk=cfg.atk, base_speed=cfg.speed, rank=cfg.rank or "normal", phase=1,
             dir="d",action="idle",state="idle",cd=math.random()*0.8,dead=false,dead_t=-1}
    E.list[#E.list+1]=e
    e.play=function(self,action,force)
        if self.action==action and not force then return end
        self.action=action
        A.play(self.ent,self.actor,self.dir,action, action=="walk" and 8.0 or 6.0, action=="idle" or action=="walk")
    end
    e.play(e,"idle",true)
    return e
end
function E.damage(e,dmg,crit)
    if e.dead then return end
    local real=math.max(1,math.floor(dmg))
    if crit then real=math.floor(real*1.6) end
    e.hp=e.hp-real
    Fx.popup((crit and "暴击 -" or "-")..real,e.x,e.z)
    if e.hp<=0 then
        e.dead=true; e.state="dead"; e.dead_t=0
        e.play(e,"die",true)
        A.sfx("hit",0.5)
        local P=require("player")
        P.gain_exp(e.cfg.exp); P.gain_gold(e.cfg.gold)
        local item=Loot.drop(e.cfg.ilvl or 1,e.rank)
        if item then P.add_equipment(item) end
        core.accept_log("kill kind=%s rank=%s x=%.1f z=%.1f", e.kind, e.rank, e.x, e.z)
        if e.rank=="boss" then core.accept_log("boss_dead kind=%s", e.kind) end
    else
        local pct=e.hp/e.hp_max
        if e.rank=="boss" and e.phase==1 and pct<=0.66 then
            e.phase=2; e.cfg.speed=e.base_speed*1.15; e.cfg.atk=e.base_atk*1.2
            core.accept_log("boss_phase=%d kind=%s", e.phase, e.kind)
        elseif e.rank=="boss" and e.phase==2 and pct<=0.33 then
            e.phase=3; e.cfg.speed=e.base_speed*1.3; e.cfg.atk=e.base_atk*1.4
            core.accept_log("boss_phase=%d kind=%s", e.phase, e.kind)
        end
    end
end
local function move(e,dx,dz)
    local nx,nz=e.x+dx,e.z+dz
    local rad=e.rank=="boss" and 0.55 or 0.25
    if not solid(nx+core.sign(dx)*rad,e.z) then e.x=nx end
    if not solid(e.x,nz+core.sign(dz)*rad) then e.z=nz end
end
function E.update(dt,P)
    for _,e in ipairs(E.list) do
        if e.dead then
            if e.dead_t>=0 then e.dead_t=e.dead_t+dt; if e.dead_t>1.2 then e.dead=true end end
            ecs.set_transform_position(e.ent,e.x,0,e.z)
            if e.dead_t>1.2 then pcall(ecs.destroy_entity,e.ent); e.dead_t=-2 end
        else
            local d=core.dist(e.x,e.z,P.x,P.z)
            e.cd=e.cd-dt
            if d<=e.cfg.sight and P.state~="dead" then
                if d<=e.cfg.range and e.cd<=0 then
                    P.damage(e.cfg.atk,e.x,e.z); e.cd=e.cfg.cd
                else
                    local dx,dz=P.x-e.x,P.z-e.z; local l=math.max(0.001,math.sqrt(dx*dx+dz*dz))
                    move(e,dx/l*e.cfg.speed*dt,dz/l*e.cfg.speed*dt)
                    set_dir(e,core.dir_from_vec(dx,dz))
                    e.play(e,"walk")
                end
            else
                e.play(e,"idle")
            end
            ecs.set_transform_position(e.ent,e.x,0,e.z)
        end
    end
end
function E.clear()
    for _,e in ipairs(E.list) do
        if e.ent then pcall(ecs.destroy_entity, e.ent) end
    end
    E.list = {}
end
return E