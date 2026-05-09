--------------------------------------------------------------------------------
-- KF_Framework — 战斗 HUD (Phase 6 补充)
-- 玩家 HP 条 (左上角)
-- 参考: KF 战斗中 HP 条 UI
--------------------------------------------------------------------------------
local Config = require("script.config")

local ecs    = dse.ecs
local ui     = dse.ui
local assets = dse.assets

local HUD = {}

-- KF: PlayerUiController 布局参数 (player_ui_controller.cpp 精确值)
-- SCREEN_RATE = 1280/1920 = 0.66667
local SR = 1280.0 / 1920.0
local HALF_W = 640.0  -- SCREEN_WIDTH / 2
local HALF_H = 360.0  -- SCREEN_HEIGHT / 2

-- Cover: kCoverSize=(562,113), kCoverLeftTop=(-935,-520)
local COVER_W  = 562 * SR    -- 374.7
local COVER_H  = 113 * SR    -- 75.3
local COVER_SCREEN_LT_X = HALF_W + (-935) * SR  -- 16.7
local COVER_SCREEN_LT_Y = HALF_H + (-520) * SR  -- 13.3
local COVER_CX = COVER_SCREEN_LT_X + COVER_W / 2  -- 204.0
local COVER_CY = COVER_SCREEN_LT_Y + COVER_H / 2  -- 51.0

-- Gauge: kLifeGaugeSize=(540,33), kLifeGaugeLeftTop=(-923,-500)
local GAUGE_MAX_W = 540 * SR  -- 360.0
local GAUGE_H     = 33 * SR   -- 22.0
local GAUGE_LT_X  = HALF_W + (-923) * SR  -- 24.7  (左边缘)
local GAUGE_LT_Y  = HALF_H + (-500) * SR  -- 26.7  (上边缘)
local GAUGE_CY    = GAUGE_LT_Y + GAUGE_H / 2  -- 37.7

-- Button prompts: kButtonTextSize=(320,40)
-- KF: kJoystickButton — size=(40,40), buttons.png 3×4 atlas
local BTN_TEXT_W  = 320 * SR  -- 213.3
local BTN_TEXT_H  = 40 * SR   -- 26.7
local BTN_ICON_W  = 40 * SR   -- 26.7 (joystick: 正方形)
local BTN_ICON_H  = 40 * SR   -- 26.7

-- Button rows (KF: kJoystickButton positions, Y=300/350/400/450)
-- UV: buttons.png 3×4 grid, each cell = (1/3, 0.25)
-- NOTE: OpenGL loads textures with stbi_set_flip_vertically_on_load(true)
--   so v=0 = image bottom. Formula: flipped_v = 1.0 - orig_v - cell_h
local BTN_ROWS = {
    -- Jump: A button (orig row0,col1) → flipped v = 1.0-0.0-0.25 = 0.75
    { kf_text_x = 410, kf_icon_x = 750, kf_y = 300,
      uv_u = 1/3, uv_v = 0.75, uv_w = 1/3, uv_h = 0.25 },
    -- Light Attack: B button (orig row0,col2) → flipped v = 0.75
    { kf_text_x = 410, kf_icon_x = 750, kf_y = 350,
      uv_u = 2/3, uv_v = 0.75, uv_w = 1/3, uv_h = 0.25 },
    -- Strong Attack: X button (orig row1,col0) → flipped v = 1.0-0.25-0.25 = 0.5
    { kf_text_x = 410, kf_icon_x = 750, kf_y = 400,
      uv_u = 0.0, uv_v = 0.5,  uv_w = 1/3, uv_h = 0.25 },
    -- Guard: LB button (orig row3,col1) → flipped v = 1.0-0.75-0.25 = 0.0
    { kf_text_x = 410, kf_icon_x = 750, kf_y = 450,
      uv_u = 1/3, uv_v = 0.0,  uv_w = 1/3, uv_h = 0.25 },
}

-- UI 实体
local hp_cover = nil   -- 装饰框 (KF: cover_, playerUICover.png)
local hp_fill  = nil   -- 填充 (KF: life_gauge_, playerUILifeGauge.png)
local hp_warn  = nil   -- 警告层 (KF: warning_gauge_, HP<50%时红色闪烁)
local ctrl_entities = {} -- 操作提示 UI 实体 (text + icon)

-- KF: PlayerUiController 闪烁参数
local warning_flash_speed = 2.0   -- KF: warning_flash_speed_=2.0
local warning_alpha = 0.0
local life_flash_speed = 0.4      -- KF: life_flash_speed_=0.4
local life_flash_r = 0.0          -- KF: diffuse_.r_ (0.0–0.5 振荡)

function HUD.setup()
    -- 加载 KF 原版 UI 纹理
    local tex_cover = assets.load_texture("assets/textures/playerUICover.png")
    local tex_gauge = assets.load_texture("assets/textures/playerUILifeGauge.png")
    local tex_btn_text = assets.load_texture("assets/textures/button_text.png")

    -- HP 填充条 (KF: life_gauge_ — 绿色, order 50 最底层)
    hp_fill = ecs.create_entity()
    ecs.add_transform(hp_fill, 0, 0, 0)
    ui.add_renderer(hp_fill, tex_gauge, 0.0, 1.0, 0.0, 1.0, 50, GAUGE_MAX_W, GAUGE_H)
    ui.set_anchor(hp_fill, 0.0, 1.0)
    ui.set_position(hp_fill, GAUGE_LT_X + GAUGE_MAX_W / 2, -GAUGE_CY)

    -- 警告层 (KF: warning_gauge_ — HP<50%时红色半透明闪烁, order 51)
    hp_warn = ecs.create_entity()
    ecs.add_transform(hp_warn, 0, 0, 0)
    ui.add_renderer(hp_warn, tex_gauge, 0.5, 0.2, 0.2, 0.0, 51, GAUGE_MAX_W, GAUGE_H)
    ui.set_anchor(hp_warn, 0.0, 1.0)
    ui.set_position(hp_warn, GAUGE_LT_X + GAUGE_MAX_W / 2, -GAUGE_CY)

    -- 装饰框 (KF: cover_ — playerUICover.png, order 52 最顶层)
    hp_cover = ecs.create_entity()
    ecs.add_transform(hp_cover, 0, 0, 0)
    ui.add_renderer(hp_cover, tex_cover, 1.0, 1.0, 1.0, 1.0, 52, COVER_W, COVER_H)
    ui.set_anchor(hp_cover, 0.0, 1.0)
    ui.set_position(hp_cover, COVER_CX, -COVER_CY)

    -- 操作提示 (KF: PlayerUiController — button_text + buttons.png joystick atlas)
    local tex_btn_icon = assets.load_texture("assets/textures/buttons.png")

    for i, row in ipairs(BTN_ROWS) do
        -- text UV: V-flipped. orig row i-1 → flipped_v = 1.0 - (i-1)*0.25 - 0.25
        local text_uv_v = 1.0 - (i - 1) * 0.25 - 0.25

        -- Y position: KF top-down → DSE bottom-up (no mirror needed)
        local text_cx = HALF_W + (row.kf_text_x + 160) * SR  -- left + textW/2
        local y = 720 - (HALF_H + (row.kf_y + 20) * SR)
        local et = ecs.create_entity()
        ecs.add_transform(et, 0, 0, 0)
        ui.add_renderer(et, tex_btn_text, 1.0, 1.0, 1.0, 0.9, 53, BTN_TEXT_W, BTN_TEXT_H)
        ui.set_anchor(et, 1.0, 0.0)
        ui.set_position(et, text_cx - 1280, y)
        ui.set_uv(et, 0, text_uv_v, 1.0, 0.25)
        table.insert(ctrl_entities, et)

        -- 手柄图标 (KF: buttons.png 3×4 atlas, kJoystickButton)
        local icon_cx = HALF_W + (row.kf_icon_x + 20) * SR  -- left + iconW/2 (40/2=20)
        local ei = ecs.create_entity()
        ecs.add_transform(ei, 0, 0, 0)
        ui.add_renderer(ei, tex_btn_icon, 1.0, 1.0, 1.0, 0.9, 53, BTN_ICON_W, BTN_ICON_H)
        ui.set_anchor(ei, 1.0, 0.0)
        ui.set_position(ei, icon_cx - 1280, y)
        ui.set_uv(ei, row.uv_u, row.uv_v, row.uv_w, row.uv_h)
        table.insert(ctrl_entities, ei)
    end
end

function HUD.update(current_hp, max_hp, dt)
    if not hp_fill then return end
    dt = dt or 0.016
    local ratio = math.max(0, math.min(1, current_hp / max_hp))

    -- KF: life_gauge_ 宽度按 HP 比例缩放, 左边缘固定
    local gauge_w = GAUGE_MAX_W * ratio
    local gauge_cx = GAUGE_LT_X + gauge_w / 2
    ui.set_size(hp_fill, gauge_w, GAUGE_H)
    ui.set_position(hp_fill, gauge_cx, -GAUGE_CY)
    -- KF: SetUvScale(Vector2(life_rate, 1.0f)) — UV 同步缩放
    ui.set_uv(hp_fill, 0, 0, ratio, 1.0)

    -- KF: UpdateWarningGauge — HP<50% 红色半透明闪烁
    if hp_warn then
        if ratio >= 0.5 then
            warning_alpha = 0.0
        else
            warning_alpha = warning_alpha + warning_flash_speed * dt
            if warning_alpha >= 0.5 then
                warning_alpha = 0.5
                warning_flash_speed = -math.abs(warning_flash_speed)
            elseif warning_alpha <= 0.0 then
                warning_alpha = 0.0
                warning_flash_speed = math.abs(warning_flash_speed)
            end
        end
        ui.set_color(hp_warn, 0.5, 0.2, 0.2, warning_alpha)
        ui.set_size(hp_warn, gauge_w, GAUGE_H)
        ui.set_position(hp_warn, gauge_cx, -GAUGE_CY)
        ui.set_uv(hp_warn, 0, 0, ratio, 1.0)
    end

    -- KF: UpdateLifeGauge — diffuse_.r_ 振荡 0–0.5, g=1.0, b=r
    life_flash_r = life_flash_r + life_flash_speed * dt
    if life_flash_r >= 0.5 then
        life_flash_r = 0.5
        life_flash_speed = -math.abs(life_flash_speed)
    elseif life_flash_r <= 0.0 then
        life_flash_r = 0.0
        life_flash_speed = math.abs(life_flash_speed)
    end
    ui.set_color(hp_fill, life_flash_r, 1.0, life_flash_r, 1.0)
end

function HUD.hide()
    if hp_cover then ui.set_visible(hp_cover, false) end
    if hp_fill then ui.set_visible(hp_fill, false) end
    if hp_warn then ui.set_visible(hp_warn, false) end
    for _, e in ipairs(ctrl_entities) do ui.set_visible(e, false) end
end

function HUD.show()
    if hp_cover then ui.set_visible(hp_cover, true) end
    if hp_fill then ui.set_visible(hp_fill, true) end
    if hp_warn then ui.set_visible(hp_warn, true) end
    for _, e in ipairs(ctrl_entities) do ui.set_visible(e, true) end
end

return HUD
