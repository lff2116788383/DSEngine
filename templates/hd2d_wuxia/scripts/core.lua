-- ============================================================================
-- core.lua  通用工具：数学 / 输入 / 路径 / 中文位图文字 / 飘字
-- ============================================================================
local M = {}

M.TILE = 32
M.SCREEN_W, M.SCREEN_H = 1280, 720

M.KEY = {
    W = 87, A = 65, S = 83, D = 68, J = 74, K = 75, U = 85, I = 73, E = 69,
    R = 82, Q = 81, F = 70, H = 72, M = 77, TAB = 258, SPACE = 32, ENTER = 257,
    ESC = 256, UP = 265, DOWN = 264, LEFT = 263, RIGHT = 262, ONE = 49, TWO = 50, THREE = 51,
}

function M.clamp(v, a, b) if v < a then return a end if v > b then return b end return v end
function M.lerp(a, b, t) return a + (b - a) * t end
function M.sign(v) if v > 0 then return 1 end if v < 0 then return -1 end return 0 end
function M.dist(x1, y1, x2, y2) return math.sqrt((x1 - x2) ^ 2 + (y1 - y2) ^ 2) end
function M.now() return dse.app.time_since_startup() end
function M.round(v, n) local p = 10 ^ (n or 0) return math.floor(v * p + 0.5) / p end

local app = dse.app
-- 自动化演示钩子（CI/截图用，正常游玩时为 nil）
M.demo = nil
M.demo_attack = false
M.demo_dodge = false

function M.key(k) return app.get_key(k) end
function M.key_down(k) return app.get_key_down(k) end
function M.axis_x()
    if M.demo then return M.demo.x or 0 end
    local x = 0
    if M.key(M.KEY.A) or M.key(M.KEY.LEFT) then x = x - 1 end
    if M.key(M.KEY.D) or M.key(M.KEY.RIGHT) then x = x + 1 end
    return x
end
function M.axis_y()
    if M.demo then return M.demo.y or 0 end
    local y = 0
    if M.key(M.KEY.W) or M.key(M.KEY.UP) then y = y + 1 end
    if M.key(M.KEY.S) or M.key(M.KEY.DOWN) then y = y - 1 end
    return y
end
function M.dir_from_vec(x, y)
    if math.abs(x) > math.abs(y) then
        return x >= 0 and "r" or "l"
    end
    if y ~= 0 then return y > 0 and "u" or "d" end
    return "d"
end

-- 路径解析：兼容「工程根/assets」与「仓库内模板目录」两种运行方式
local ROOT_CANDIDATES = { "", "templates/hd2d_wuxia/", "assets/", "templates/hd2d_wuxia/assets/" }
function M.file_exists(p)
    local f = io.open(p, "rb")
    if f then f:close() return true end
    return false
end
function M.resolve(p)
    for _, c in ipairs(ROOT_CANDIDATES) do
        local cand = c .. p
        if M.file_exists(cand) then return cand end
    end
    return p
end

--  中文位图文字（自绘字体图集 + UI 逐字排版） 
local Text = {}
Text.__index = Text

function Text.new(fontdata, tex, x, y, cr, cg, cb, order)
    local t = setmetatable({}, Text)
    t.font = fontdata
    t.tex = tex
    t.x, t.y = x, y
    t.color = { cr or 1, cg or 1, cb or 1 }
    t.order = order or 900000
    t.objs = {}
    t.text = nil
    return t
end

function M.utf8_sub(s, i, j)
    local n = utf8.len(s) or 0
    i = i or 1
    j = j or n
    if i < 0 then i = math.max(1, n + i + 1) end
    if j < 0 then j = math.max(1, n + j + 1) end
    if i > n or j < i then return "" end
    local si = utf8.offset(s, i)
    if not si then return "" end
    local sj = utf8.offset(s, j + 1)
    return s:sub(si, (sj or (#s + 1)) - 1)
end

function Text:set(str, force)
    if self.text == str and not force then return end
    self.text = str
    local font, objs = self.font, self.objs
    local pen, used, line = self.x, 0, 0
    local lh = (font.line or 22) * 1.2
    for _, cp in utf8.codes(str or "") do
        if cp == 10 then
            line = line + 1
            pen = self.x
        else
        local gl = font.glyphs[cp]
        if gl then
            used = used + 1
            local e = objs[used]
            if not e then
                e = dse.ecs.create_entity()
                dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
                dse.ui.add_renderer(e, self.tex, self.color[1], self.color[2], self.color[3], 1.0,
                                    self.order + used, gl.w, gl.h)
                dse.ui.set_anchor(e, 0, 0)
                objs[used] = e
            end
            dse.ui.set_uv(e, gl.u0, gl.v0, gl.u1, gl.v1)
            dse.ui.set_size(e, gl.w, gl.h)
            dse.ui.set_color(e, self.color[1], self.color[2], self.color[3], 1.0)
            dse.ui.set_position(e, pen + gl.ox + gl.w * 0.5, self.y + gl.oy - gl.h * 0.5 - line * lh)
            dse.ui.set_visible(e, 1)
            pen = pen + gl.adv
        end
        end
    end
    for i = used + 1, #objs do
        dse.ui.set_visible(objs[i], 0)
    end
    self.width = pen - self.x
    self.height = (line + 1) * lh
    return self
end

function Text:set_color(r, g, b)
    self.color = { r, g, b }
    self:set(self.text, true)
end
function Text:set_pos(x, y)
    self.x, self.y = x, y
    self:set(self.text, true)
end
function Text:visible(v)
    for _, e in ipairs(self.objs) do dse.ui.set_visible(e, v and 1 or 0) end
end
function Text:destroy()
    for _, e in ipairs(self.objs) do pcall(dse.ecs.destroy_entity, e) end
    self.objs = {}
end

M.Text = Text

-- 数字（ASCII）也走同一图集，避免额外字体依赖
function M.fmt_num(v) return string.format("%d", math.floor(v + 0.5)) end

return M