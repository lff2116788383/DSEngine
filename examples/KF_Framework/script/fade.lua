--------------------------------------------------------------------------------
-- KF_Framework — Fade 过渡系统
-- 参考: KF source_code/fade_system.cpp
--
-- 完整状态机:
--   FadeOut (1.0s)  → 黑幕 alpha 0→1
--   WaitOut (0.5s)  → Loading 动画淡入
--   Wait            → Loading 动画播放 (等待回调完成)
--   WaitIn  (0.5s)  → Loading 动画淡出
--   FadeIn  (1.0s)  → 黑幕 alpha 1→0
--
-- Loading 动画: sprite-sheet loading.png (2col × 15row = 30帧)
--   位置: 屏幕右下 (x=408, y=-276), 尺寸 412×64
--   UV scroll: 每 2 帧切换 1 格 (每帧 ~33ms @60fps → 每格 ~66ms)
--------------------------------------------------------------------------------
local ecs    = dse.ecs
local ui     = dse.ui
local assets = dse.assets

local Fade = {}

-- 状态
local fade_entity = nil        -- 全屏黑幕
local loading_entity = nil     -- Loading 动画
local loading_tex = 0          -- loading.png handle

local fade_state = "none"
-- "none" | "out" | "wait_out" | "wait" | "wait_in" | "in"
local fade_timer = 0
local fade_duration = 1.0      -- FadeIn/FadeOut 时间 (KF: 1.0s)
local wait_fade_time = 0.5     -- Loading 淡入淡出时间 (KF: 0.5s)
local wait_timer = 0
local fade_callback = nil

-- Loading sprite-sheet 参数 (KF: Short2(2,15), frame_per_pattern=2)
local LOADING_COLS = 2
local LOADING_ROWS = 15
local LOADING_TOTAL = LOADING_COLS * LOADING_ROWS  -- 30
local LOADING_FPP = 2          -- game-frames per pattern
local loading_frame_counter = 0
local loading_uv_w = 1.0 / LOADING_COLS  -- 0.5
local loading_uv_h = 1.0 / LOADING_ROWS  -- 0.0667
local WAIT_MIN_TIME = 1.5   -- 最小等待时间 (让 loading 动画充分播放)

--------------------------------------------------------------------------------
-- 初始化
--------------------------------------------------------------------------------
function Fade.setup()
    -- 全屏黑色遮罩 (z_order=200)
    fade_entity = ecs.create_entity()
    ecs.add_transform(fade_entity, 0, 0, 0)
    ui.add_renderer(fade_entity, 0, 0, 0, 0, 0, 200, 1920, 1080)
    ui.set_anchor(fade_entity, 0.5, 0.5)
    ui.set_visible(fade_entity, false)

    -- Loading 动画 (z_order=201, 在黑幕之上)
    loading_tex = assets.load_texture("assets/textures/loading.png")
    if loading_tex ~= 0 then
        loading_entity = ecs.create_entity()
        ecs.add_transform(loading_entity, 0, 0, 0)
        ui.add_renderer(loading_entity, loading_tex, 1, 1, 1, 0, 201, 412, 64)
        ui.set_anchor(loading_entity, 0.5, 0.5)
        ui.set_position(loading_entity, 408, -276)
        ui.set_uv(loading_entity, 0, 0, loading_uv_w, loading_uv_h)
        ui.set_visible(loading_entity, false)
    end
end

--------------------------------------------------------------------------------
-- 启动时调用: 从 Loading 动画开始 (KF: 初始状态 kFadeWait)
-- 黑幕全覆盖 + Loading 动画播放 → Loading 淡出 → 黑幕淡出
--------------------------------------------------------------------------------
function Fade.start_with_loading()
    if not fade_entity then return end
    -- 黑幕全覆盖
    ui.set_visible(fade_entity, true)
    ui.set_color(fade_entity, 0, 0, 0, 1)
    -- 显示 loading
    loading_frame_counter = 0
    if loading_entity then
        ui.set_visible(loading_entity, true)
        ui.set_color(loading_entity, 1, 1, 1, 1)
    end
    -- 进入 wait 状态 (Loading 动画全透明度播放, KF: kFadeWait)
    fade_state = "wait"
    fade_duration = 1.0
    wait_timer = 0
    fade_callback = nil
end

--------------------------------------------------------------------------------
-- FadeOut: 黑幕 alpha 0→1, 然后进入 Loading 等待
-- callback 在 Loading 等待期间执行 (模拟 KF 的 Mode::Change)
--------------------------------------------------------------------------------
function Fade.fade_out(duration, callback)
    if not fade_entity then return end
    if fade_state == "out" then return end  -- KF: 防止重复触发
    fade_state = "out"
    fade_duration = duration or 1.0
    fade_timer = 0
    fade_callback = callback
    ui.set_visible(fade_entity, true)
    ui.set_color(fade_entity, 0, 0, 0, 0)
end

--------------------------------------------------------------------------------
-- FadeIn: 黑幕 alpha 1→0 (直接调用, 跳过 Loading)
--------------------------------------------------------------------------------
function Fade.fade_in(duration, callback)
    if not fade_entity then return end
    fade_state = "in"
    fade_duration = duration or 1.0
    fade_timer = fade_duration
    fade_callback = callback
    ui.set_visible(fade_entity, true)
    ui.set_color(fade_entity, 0, 0, 0, 1)
    -- 隐藏 loading
    if loading_entity then
        ui.set_visible(loading_entity, false)
        ui.set_color(loading_entity, 1, 1, 1, 0)
    end
end

--------------------------------------------------------------------------------
-- Loading 动画帧更新 (KF: Scroll2dController)
--------------------------------------------------------------------------------
local function update_loading_uv()
    if not loading_entity then return end
    local real_frame = math.floor(loading_frame_counter / LOADING_FPP)
    local col = real_frame % LOADING_COLS
    local row = math.floor(real_frame / LOADING_COLS) % LOADING_ROWS
    local u = col * loading_uv_w
    -- V-flip 补偿: stbi_set_flip_vertically_on_load(true) 使 v=0 对应图片底部
    local v = 1.0 - row * loading_uv_h - loading_uv_h
    ui.set_uv(loading_entity, u, v, loading_uv_w, loading_uv_h)
    loading_frame_counter = (loading_frame_counter + 1) % (LOADING_TOTAL * LOADING_FPP)
end

--------------------------------------------------------------------------------
-- 每帧更新
--------------------------------------------------------------------------------
function Fade.update(dt)
    if fade_state == "none" or not fade_entity then return end

    -- FadeOut: 黑幕 alpha 0→1
    if fade_state == "out" then
        fade_timer = fade_timer + dt
        local alpha = math.min(1.0, fade_timer / fade_duration)
        ui.set_color(fade_entity, 0, 0, 0, alpha)
        if fade_timer >= fade_duration then
            fade_state = "wait_out"
            wait_timer = 0
            loading_frame_counter = 0
            -- 显示 loading, alpha=0 准备淡入
            if loading_entity then
                ui.set_visible(loading_entity, true)
                ui.set_color(loading_entity, 1, 1, 1, 0)
            end
        end

    -- WaitOut: Loading 淡入 (alpha 0→1, 0.5s)
    elseif fade_state == "wait_out" then
        wait_timer = wait_timer + dt
        update_loading_uv()
        if loading_entity then
            local a = math.min(1.0, wait_timer / wait_fade_time)
            ui.set_color(loading_entity, 1, 1, 1, a)
        end
        if wait_timer >= wait_fade_time then
            fade_state = "wait"
            wait_timer = 0
            -- 执行回调 (KF: MainSystem::Change → 切换 Mode)
            if fade_callback then
                local cb = fade_callback
                fade_callback = nil
                cb()
            end
        end

    -- Wait: Loading 动画持续播放 (KF: 等待 IsCompleteLoading, DSE: 最小等待时间)
    elseif fade_state == "wait" then
        wait_timer = wait_timer + dt
        update_loading_uv()
        if wait_timer >= WAIT_MIN_TIME then
            fade_state = "wait_in"
            wait_timer = wait_fade_time
        end

    -- WaitIn: Loading 淡出 (alpha 1→0, 0.5s)
    elseif fade_state == "wait_in" then
        wait_timer = wait_timer - dt
        update_loading_uv()
        if loading_entity then
            local a = math.max(0.0, wait_timer / wait_fade_time)
            ui.set_color(loading_entity, 1, 1, 1, a)
        end
        if wait_timer <= 0 then
            if loading_entity then
                ui.set_visible(loading_entity, false)
            end
            -- 自动进入 FadeIn
            fade_state = "in"
            fade_timer = fade_duration
        end

    -- FadeIn: 黑幕 alpha 1→0
    elseif fade_state == "in" then
        fade_timer = fade_timer - dt
        local alpha = math.max(0.0, fade_timer / fade_duration)
        ui.set_color(fade_entity, 0, 0, 0, alpha)
        if fade_timer <= 0 then
            fade_state = "none"
            ui.set_visible(fade_entity, false)
            if fade_callback then
                local cb = fade_callback
                fade_callback = nil
                cb()
            end
        end
    end
end

--------------------------------------------------------------------------------
-- 查询
--------------------------------------------------------------------------------
function Fade.is_fading()
    return fade_state ~= "none"
end

--------------------------------------------------------------------------------
-- 强制清除 (自动截图模式用, 立即移除黑幕和 loading)
--------------------------------------------------------------------------------
function Fade.force_clear()
    fade_state = "none"
    fade_timer = 0
    fade_callback = nil
    if fade_entity then
        ui.set_visible(fade_entity, false)
        ui.set_color(fade_entity, 0, 0, 0, 0)
    end
    if loading_entity then
        ui.set_visible(loading_entity, false)
        ui.set_color(loading_entity, 1, 1, 1, 0)
    end
end

return Fade
