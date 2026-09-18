local Rng = {}
Rng.__index = Rng
local MASK = 0xFFFFFFFF
function Rng.new(seed)
    seed = math.floor(tonumber(seed) or 1) & MASK
    if seed == 0 then seed = 0x9E3779B9 end
    return setmetatable({state=seed}, Rng)
end
function Rng:next_u32()
    local x = self.state
    x = (x ~ (x << 13)) & MASK
    x = (x ~ (x >> 17)) & MASK
    x = (x ~ (x << 5)) & MASK
    self.state = x
    return x
end
function Rng:float() return self:next_u32()/4294967296.0 end
function Rng:int(a,b) a=math.floor(a or 0); b=math.floor(b or a); if b<a then a,b=b,a end; return a+math.floor(self:float()*(b-a+1)) end
function Rng:range(a,b) a=a or 0; b=b or a; return a+(b-a)*self:float() end
return Rng