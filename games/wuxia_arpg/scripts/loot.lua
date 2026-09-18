-- Deterministic prototype loot for the new game.
local Rng = require("rng")
local L = {}
L.rng = nil
local RARITY = { common="普通", magic="魔法", rare="稀有", legendary="传奇" }
local BASES = {
    {id="iron_sword", name="铁剑", slot="weapon", req=1, atk=6},
    {id="cloth_robe", name="布衣", slot="armor", req=1, def=4, hp=10},
    {id="jade_pendant", name="玉佩", slot="accessory", req=3, mp=20},
}
local PREFIX = {
    {id="atk", stat="atk", name="攻击", min=1, max=3},
    {id="def", stat="def", name="防御", min=1, max=3},
    {id="hp", stat="hp", name="生命", min=5, max=15},
    {id="mp", stat="mp", name="内力", min=4, max=12},
    {id="crit", stat="crit", name="暴击", min=1, max=4, pct=true},
}
local SUFFIX = {
    {id="move", stat="move", name="身法", min=1, max=4, pct=true},
    {id="leech", stat="leech", name="吸血", min=1, max=3, pct=true},
    {id="luck", stat="luck", name="幸运", min=2, max=7, pct=true},
    {id="regen", stat="regen", name="回气", min=1, max=3},
}
function L.init(seed) L.rng=Rng.new(seed or 20260918); return L.rng end
function L.name(item) return item and item.name or "" end
function L.rarity(r) return RARITY[r] or r end
local function pick(rng, pool, used, ilvl)
    local choices={}
    for _,a in ipairs(pool) do if not used[a.id] then choices[#choices+1]=a end end
    if #choices==0 then return nil end
    local a=choices[rng:int(1,#choices)]; used[a.id]=true
    local v=a.min+math.floor(rng:float()*(a.max-a.min+1))+math.floor((ilvl-1)*0.5)
    return {id=a.id, stat=a.stat, name=a.name, value=v, pct=a.pct and true or false}
end
function L.roll(ilvl, rank)
    if not L.rng then L.init() end
    local rng=L.rng; ilvl=math.max(1,math.floor(ilvl or 1)); rank=rank or "normal"
    local b=({normal=0,elite=1,champion=2,boss=3})[rank] or 0
    local roll=rng:float(); local rarity
    local w_common=math.max(2,60-15*b); local w_magic=30+5*b; local w_rare=9+7*b; local w_legend=1+4*b
    local total=w_common+w_magic+w_rare+w_legend; roll=roll*total
    if roll<=w_common then rarity="common" elseif roll<=w_common+w_magic then rarity="magic"
    elseif roll<=w_common+w_magic+w_rare then rarity="rare" else rarity="legendary" end
    local count=rarity=="common" and 0 or (rarity=="magic" and rng:int(1,2) or (rarity=="rare" and rng:int(3,4) or rng:int(4,5)))
    local base=BASES[rng:int(1,#BASES)]; local affixes={}; local used={}
    for i=1,count do local pool=(i%2==1) and PREFIX or SUFFIX; local a=pick(rng,pool,used,ilvl); if a then affixes[#affixes+1]=a end end
    return {uid=0, base_id=base.id, name=base.name..""..L.rarity(rarity), slot=base.slot,
            rarity=rarity, ilvl=ilvl, req=base.req, affixes=affixes, seed=rng.state}
end
function L.drop(ilvl, rank)
    if not L.rng then L.init() end
    local chance=rank=="elite" and 0.6 or (rank=="champion" and 0.9 or (rank=="boss" and 1.0 or 0.35))
    if L.rng:float()>chance then return nil end
    return L.roll(ilvl, rank)
end
return L