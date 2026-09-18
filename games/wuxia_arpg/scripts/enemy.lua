-- New game bandit enemies: lit Sprite3D, simple chase/attack, drops.
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
    local ent=ecs.create_entity()
    ecs.add_transform(ent,x,0,z,1,1,1)
    ecs.add_sprite3d(ent,0,cfg.size[1]/32.0,cfg.size[2]/32.0,{billboard="yaw",anchor=0.0,lit=true,receive_shadow=true})
    ecs.set_sprite3d_lit(ent,true)
    ecs.set_sprite3d_receive_shadow(ent,true)
    ecs.set_sprite3d_contact_shadow(ent,true,0.4,0.42)
    local e={kind=kind,cfg=cfg,ent=ent,x=x,z=z,home_x=x,home_z=z,hp=cfg.hp,hp_max=cfg.hp,
             dir="d",action="idle",state="idle",cd=math.random()*0.8,dead=false,dead_t=-1}
    E.list[#E.list+1]=e
    e.play=function(self,action,force)
        if self.action==action and not force then return end
        self.action=action
        A.play(self.ent,"bandit",self.dir,action, action=="walk" and 8.0 or 6.0, action=="idle" or action=="walk")
    end
    e.play(e,"idle",true)
    return e
end
function E.damage(e,dmg)
    if e.dead then return end
    local def = 0
    local real=math.max(1,math.floor(dmg-def*0.5))
    e.hp=e.hp-real
    Fx.popup("-"..real,e.x,e.z)
    if e.hp<=0 then
        e.dead=true; e.state="dead"; e.dead_t=0
        e.play(e,"die",true)
        A.sfx("hit",0.5)
        local P=require("player")
        P.gain_exp(e.cfg.exp); P.gain_gold(e.cfg.gold)
        local item=Loot.drop(e.cfg.ilvl or 1,"normal")
        if item then P.add_equipment(item) end
        core.accept_log("kill kind=%s x=%.1f z=%.1f", e.kind, e.x, e.z)
    end
end
local function move(e,dx,dz)
    local nx,nz=e.x+dx,e.z+dz
    if not solid(nx+core.sign(dx)*0.25,e.z) then e.x=nx end
    if not solid(e.x,nz+core.sign(dz)*0.25) then e.z=nz end
end
function E.update(dt,P)
    for _,e in ipairs(E.list) do
        if e.dead then
            if e.dead_t>=0 then e.dead_t=e.dead_t+dt; if e.dead_t>1.2 then e.dead=true end end
                ecs.set_transform_position(e.ent,e.x,0,e.z)
                -- destroy after fade window (keep entity until end)
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
return E