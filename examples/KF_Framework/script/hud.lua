--------------------------------------------------------------------------------
-- KF_Framework — 战斗 HUD (Phase 6 补充)
-- 玩家 HP 条 (左上角)
-- 参考: KF 战斗中 HP 条 UI
--------------------------------------------------------------------------------
local Config = require("script.config")

local ecs = dse.ecs
local ui  = dse.ui

local HUD = {}

-- HP 条参数
local BAR_W   = 260    -- 血条总宽度
local BAR_H   = 20     -- 血条高度
local BAR_X   = 150    -- 锚点偏移 X (右移, 左上角 anchor=(0,1), pivot=(0.5,0.5))
local BAR_Y   = -30    -- 锚点偏移 Y (下移)
local BG_PAD  = 3      -- 背景边距

-- UI 实体
local hp_bg   = nil    -- 背景 (深灰)
local hp_fill = nil    -- 填充 (绿→红渐变)
local hp_label = nil   -- 文字 (可选)

function HUD.setup()
    -- 背景条 (深灰半透明)
    hp_bg = ecs.create_entity()
    ecs.add_transform(hp_bg, 0, 0, 0)
    ui.add_renderer(hp_bg, 0, 0.15, 0.15, 0.15, 0.8, 50, BAR_W + BG_PAD * 2, BAR_H + BG_PAD * 2)
    ui.set_anchor(hp_bg, 0.0, 1.0)  -- 左上角
    ui.set_position(hp_bg, BAR_X, BAR_Y)

    -- HP 填充条 (绿色)
    hp_fill = ecs.create_entity()
    ecs.add_transform(hp_fill, 0, 0, 0)
    ui.add_renderer(hp_fill, 0, 0.2, 0.85, 0.2, 1.0, 51, BAR_W, BAR_H)
    ui.set_anchor(hp_fill, 0.0, 1.0)
    ui.set_position(hp_fill, BAR_X, BAR_Y)
end

function HUD.update(current_hp, max_hp)
    if not hp_fill then return end
    local ratio = math.max(0, math.min(1, current_hp / max_hp))

    -- 更新宽度
    ui.set_size(hp_fill, BAR_W * ratio, BAR_H)

    -- 颜色: 绿(>50%) → 黄(25-50%) → 红(<25%)
    local r, g, b
    if ratio > 0.5 then
        r, g, b = 0.2, 0.85, 0.2
    elseif ratio > 0.25 then
        r, g, b = 0.9, 0.8, 0.1
    else
        r, g, b = 0.9, 0.15, 0.15
    end
    ui.set_color(hp_fill, r, g, b, 1.0)
end

function HUD.hide()
    if hp_bg then ui.set_visible(hp_bg, false) end
    if hp_fill then ui.set_visible(hp_fill, false) end
end

function HUD.show()
    if hp_bg then ui.set_visible(hp_bg, true) end
    if hp_fill then ui.set_visible(hp_fill, true) end
end

return HUD
