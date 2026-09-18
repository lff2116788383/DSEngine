-- R4 combat test: 3-hit combo, AoE skill, elite/boss phases, guaranteed boss drop.
local A = require("assets")
local P = require("player")
local E = require("enemy")
local Loot = require("loot")
local function check(c,m) if not c then error("[r4-combat] FAIL "..(m or "assert")) end end
function Awake()
    Loot.init(12345); math.randomseed(12345)
    A.load(); P.spawn()
    -- 3-hit combo
    P.attack_cd=0; P.combo_t=0; P.combo=0; P.try_attack(); local d1=P.attack_dmg
    P.attack_cd=0; P.try_attack(); local d2=P.attack_dmg
    P.attack_cd=0; P.try_attack(); local d3=P.attack_dmg
    check(P.combo==3 and d3>d2 and d2>=d1, "combo")
    -- AoE skill
    P.mp=100; P.skill_cd.fenhua=0; check(P.try_skill("fenhua"), "fenhua skill")
    check(P.skill_hit and P.skill_hit.kind=="fenhua", "fenhua hit")
    P.hp = math.floor(P.stats().hp_max * 0.5)
    local hp_before = P.hp
    P.mp = 100; P.skill_cd.zixia = 0
    check(P.try_skill("zixia"), "zixia skill")
    check(P.hp > hp_before and P.buff_t > 0, "zixia effect")
    -- Boss phases and death
    local boss = E.spawn("boss_blood_blade", 0, 0)
    check(boss and boss.rank=="boss", "boss spawn")
    E.damage(boss, 120, false); check(boss.phase==2, "boss phase2")
    E.damage(boss, 120, false); check(boss.phase==3, "boss phase3")
    local items_before = #P.items
    E.damage(boss, 999, true); check(boss.dead, "boss dead")
    check(#P.items > items_before, "boss drop")
    print(string.format("[r4-combat] PASS combo=%d d1=%d d3=%d boss_phase=3 items=%d", P.combo, d1, d3, #P.items))
    dse.app.quit()
end
function Update(dt) local _=dt end