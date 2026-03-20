local BaseScene = require("script/mirror_game/core/base_scene")
local Match3Board = require("script/mirror_game/core/match3_board")
require("renderer/camera")
require("renderer/sprite_renderer")
require("renderer/texture_2d")
require("renderer/sprite")
require("control/input")
require("utils/screen")
require("utils/time")

local Match3Scene = setmetatable({}, {__index = BaseScene})
Match3Scene.__index = Match3Scene

local function LoadTextureFromCandidates(paths)
    for _, path in ipairs(paths) do
        local tex = Texture2D.LoadFromFile(path)
        if tex then
            return tex
        end
    end
    return nil
end

function Match3Scene.new()
    local self = BaseScene.new()
    setmetatable(self, Match3Scene)
    
    self.board = nil
    self.gem_objects = {} -- 2D array [y][x] -> GameObject
    self.selected_gem_pos = nil -- {x, y}
    self.camera_size = 5
    self.board_offset = glm.vec3(-3.5, -3.5, 0)
    
    self.moves_left = 20
    self.score = 0
    self.is_animating = false
    
    -- RPG Elements
    self.enemy_hp = 100
    self.max_enemy_hp = 100
    self.player_hp = 120
    self.max_player_hp = 120
    self.player_shield_points = 0
    self.player_shield_cap = 80
    self.fury_charges = 0
    self.fury_charge_cap = 8
    self.fury_bonus_per_charge = 0.12
    self.shield_bar_bg_go = nil
    self.shield_bar_fill_go = nil
    self.shield_segment_gos = {}
    self.shield_segment_max = 8
    self.shield_bar_width = 2.6
    self.shield_bar_left_x = 1.4
    self.shield_bar_y = -4.2
    self.fury_bar_bg_go = nil
    self.fury_bar_fill_go = nil
    self.fury_burst_go = nil
    self.fury_burst_phase_time = 0
    self.fury_bar_width = 2.6
    self.fury_bar_left_x = 4.2
    self.fury_bar_y = -4.2
    self.shield_flash_timer = 0
    self.fury_flash_timer = 0
    self.shield_break_go = nil
    self.shield_break_timer = 0
    self.shield_break_duration = 0.35
    self.fury_ready_prompt_go = nil
    self.fury_ready_prompt_timer = 0
    self.fury_ready_prompt_duration = 1.0
    
    self.gem_sprites = {}
    self.ui_sprites = {}
    self.enemy_hp_bar_bg_go = nil
    self.enemy_hp_bar_fill_go = nil
    self.enemy_hp_bar_width = 6
    self.enemy_hp_bar_left_x = -3.8
    self.enemy_hp_bar_y = 4.2
    self.player_hp_bar_bg_go = nil
    self.player_hp_bar_fill_go = nil
    self.player_hp_bar_width = 5.2
    self.player_hp_bar_left_x = -3.8
    self.player_hp_bar_y = -4.2
    self.player_hp_shake_timer = 0
    self.player_hp_shake_duration = 0.22
    self.player_hp_shake_strength = 0
    self.hit_flash_go = nil
    self.hit_flash_timer = 0
    self.hit_flash_duration = 0.3
    self.hit_flash_runtime_duration = 0.3
    self.hit_flash_color = {r = 0.95, g = 0.1, b = 0.1}
    self.hit_flash_alpha = 0.45
    self.player_hp_pulse_time = 0
    self.player_hp_pulse_threshold = 0.3
    self.round_prompt_go = nil
    self.round_prompt_timer = 0
    self.round_index = 1
    self.combo_prompt_go = nil
    self.combo_prompt_timer = 0
    self.combo_prompt_duration = 1.1
    self.combo_count = 0
    self.combo_tick_slots = {}
    self.combo_tick_max = 8
    self.combo_overflow = false
    self.combo_overflow_phase_time = 0
    self.combo_overflow_start = 9
    self.combo_overflow_bonus_step = 0.10
    self.combo_overflow_bonus_by_type = {
        [Match3Board.GemType.RED] = 0.08,
        [Match3Board.GemType.GREEN] = 0.04,
        [Match3Board.GemType.BLUE] = 0.06,
        [Match3Board.GemType.YELLOW] = 0.05,
        [Match3Board.GemType.PURPLE] = 0.07
    }
    self.combo_theme_colors = {
        [Match3Board.GemType.RED] = {r = 1.0, g = 0.35, b = 0.28},
        [Match3Board.GemType.GREEN] = {r = 0.36, g = 0.95, b = 0.45},
        [Match3Board.GemType.BLUE] = {r = 0.36, g = 0.68, b = 1.0},
        [Match3Board.GemType.YELLOW] = {r = 1.0, g = 0.88, b = 0.3},
        [Match3Board.GemType.PURPLE] = {r = 0.9, g = 0.42, b = 1.0}
    }
    self.combo_theme_type = Match3Board.GemType.RED
    self.current_turn_combo = 0
    self.drop_prompt_slots = {}
    self.drop_prompt_timers = {0, 0, 0}
    self.drop_prompt_types = {nil, nil, nil}
    self.drop_prompt_counts = {0, 0, 0}
    
    return self
end

function Match3Scene:Init()
    print("Match3Scene:Init()")
    
    local GameManager = require("script/mirror_game/core/game_manager")
    local BattleSystem = require("script/mirror_game/systems/battle")
    local Monster = require("script/mirror_game/object/monster")
    
    self:CreateCamera()
    self:CreateBackground()
    
    -- Initialize Board Logic
    self.board = Match3Board.new(8, 8)
    
    -- 初始化敌人
    self.monster = Monster.new(1001) -- 稻草人
    self.enemy_hp = self.monster.hp
    self.max_enemy_hp = self.monster.max_hp
    
    -- 设置消除回调
    self.board:SetMatchCallback(function(gem_type, count)
        if GameManager.player and self.monster then
            BattleSystem.ProcessMatch(GameManager.player, self.monster, gem_type, count)
            
            -- 同步 UI 数据
            local human = GameManager.player:GetCurrentHuman()
            if human then
                self.player_hp = human.hp
                self.max_player_hp = human.max_hp
            end
            
            if self.monster then
                self.enemy_hp = self.monster.hp
                self.max_enemy_hp = self.monster.max_hp
                
                if self.monster.hp <= 0 then
                    -- 怪物死亡，重新生成
                    self.monster = Monster.new(math.random(1001, 1004))
                    self.enemy_hp = self.monster.hp
                    self.max_enemy_hp = self.monster.max_hp
                    print("New Monster appeared: " .. self.monster.name)
                end
            end
            
            self:PrintBattleStatus()
            self:UpdateVisuals()
        end
    end)
    
    -- Initialize Visual Pool
    self:CreateBoardVisuals()
    self:CreateCombatUI()
    
    self:UpdateVisuals()
    self:PrintBattleStatus()
end

function Match3Scene:CreateCamera()
    local go = GameObject.new("MainCamera")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 10))
    
    local camera = go:AddComponent(Camera)
    -- Default aspect, will update in Update if needed
    local aspect = Screen.width() / Screen.height()
    local height = self.camera_size
    local width = height * aspect
    
    camera:SetOrthographic(-width, width, -height, height, -10, 100)
    camera:set_depth(10) 
    camera:set_clear_color(0.1, 0.1, 0.1, 1.0)
    
    self:AddGameObject(go)
    self.camera_go = go
end

function Match3Scene:CreateBackground()
    -- Optional: Create a board background
    local go = GameObject.new("Background")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 0))
    transform:set_scale(glm.vec3(2, 2, 1)) -- Adjust scale as needed
    
    local renderer = go:AddComponent(SpriteRenderer)
    
    local bg_texture = LoadTextureFromCandidates({
        "mirror_assets/Resources/bg00_2.png",
        "../bin/data/mirror_assets/Resources/bg00_2.png"
    })
    
    if bg_texture then
        local sprite = Sprite.Create(bg_texture)
        sprite:set_ppu(100)
        renderer:set_sprite(sprite)
        renderer:set_sorting_layer(0)
        renderer:set_order_in_layer(-10)
    else
        print("Error: Failed to load background texture")
    end
    
    self:AddGameObject(go)
end

function Match3Scene:PrintBattleStatus()
    print("Score: " .. self.score .. " | Moves: " .. self.moves_left .. " | Player HP: " .. self.player_hp .. "/" .. self.max_player_hp .. " | Shield: " .. self.player_shield_points .. " | Fury: " .. self.fury_charges .. " | Enemy HP: " .. self.enemy_hp .. "/" .. self.max_enemy_hp)
end

function Match3Scene:CreateBoardVisuals()
    local gem_paths = {
        [Match3Board.GemType.RED] = {"mirror_assets/Resources/1001.png", "../bin/data/mirror_assets/Resources/1001.png"},
        [Match3Board.GemType.GREEN] = {"mirror_assets/Resources/1002.png", "../bin/data/mirror_assets/Resources/1002.png"},
        [Match3Board.GemType.BLUE] = {"mirror_assets/Resources/1003.png", "../bin/data/mirror_assets/Resources/1003.png"},
        [Match3Board.GemType.YELLOW] = {"mirror_assets/Resources/1004.png", "../bin/data/mirror_assets/Resources/1004.png"},
        [Match3Board.GemType.PURPLE] = {"mirror_assets/Resources/1005.png", "../bin/data/mirror_assets/Resources/1005.png"}
    }

    for gem_type, paths in pairs(gem_paths) do
        local texture = LoadTextureFromCandidates(paths)
        if texture then
            local sprite = Sprite.Create(texture)
            sprite:set_ppu(80)
            self.gem_sprites[gem_type] = sprite
        else
            print("Error: Failed to load gem texture for type " .. tostring(gem_type))
        end
    end

    if not self.gem_sprites[Match3Board.GemType.RED] then
        return
    end

    for y = 1, self.board.height do
        self.gem_objects[y] = {}
        for x = 1, self.board.width do
            local go = GameObject.new("Gem_" .. x .. "_" .. y)
            local transform = go:AddComponent(Transform)
            transform:set_position(self:GridToWorld(x, y))
            transform:set_scale(glm.vec3(0.9, 0.9, 1.0))
            
            local renderer = go:AddComponent(SpriteRenderer)
            renderer:set_sprite(self.gem_sprites[Match3Board.GemType.RED])
            renderer:set_sorting_layer(0)
            renderer:set_order_in_layer(10) 
            
            self:AddGameObject(go)
            self.gem_objects[y][x] = go
        end
    end
end

function Match3Scene:CreateCombatUI()
    local gray_texture = LoadTextureFromCandidates({"images/gray.png", "../bin/data/images/gray.png"})
    local red_texture = LoadTextureFromCandidates({"images/red.png", "../bin/data/images/red.png"})
    if gray_texture then
        local gray_sprite = Sprite.Create(gray_texture)
        gray_sprite:set_ppu(100)
        self.ui_sprites.gray = gray_sprite
    end
    if red_texture then
        local red_sprite = Sprite.Create(red_texture)
        red_sprite:set_ppu(100)
        self.ui_sprites.red = red_sprite
    end

    local hp_bg_go = GameObject.new("EnemyHpBarBg")
    local hp_bg_transform = hp_bg_go:AddComponent(Transform)
    hp_bg_transform:set_position(glm.vec3(self.enemy_hp_bar_left_x + self.enemy_hp_bar_width * 0.5, self.enemy_hp_bar_y, 0))
    hp_bg_transform:set_scale(glm.vec3(self.enemy_hp_bar_width, 0.35, 1))
    local hp_bg_renderer = hp_bg_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.gray then
        hp_bg_renderer:set_sprite(self.ui_sprites.gray)
    elseif self.ui_sprites.red then
        hp_bg_renderer:set_sprite(self.ui_sprites.red)
    end
    hp_bg_renderer:set_color(0.12, 0.12, 0.12, 0.92)
    hp_bg_renderer:set_sorting_layer(2)
    hp_bg_renderer:set_order_in_layer(0)
    self:AddGameObject(hp_bg_go)
    self.enemy_hp_bar_bg_go = hp_bg_go

    local hp_fill_go = GameObject.new("EnemyHpBarFill")
    local hp_fill_transform = hp_fill_go:AddComponent(Transform)
    hp_fill_transform:set_position(glm.vec3(self.enemy_hp_bar_left_x + self.enemy_hp_bar_width * 0.5, self.enemy_hp_bar_y, 0))
    hp_fill_transform:set_scale(glm.vec3(self.enemy_hp_bar_width, 0.24, 1))
    local hp_fill_renderer = hp_fill_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        hp_fill_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        hp_fill_renderer:set_sprite(self.ui_sprites.gray)
    end
    hp_fill_renderer:set_color(0.92, 0.18, 0.18, 0.96)
    hp_fill_renderer:set_sorting_layer(2)
    hp_fill_renderer:set_order_in_layer(1)
    self:AddGameObject(hp_fill_go)
    self.enemy_hp_bar_fill_go = hp_fill_go

    local player_bg_go = GameObject.new("PlayerHpBarBg")
    local player_bg_transform = player_bg_go:AddComponent(Transform)
    player_bg_transform:set_position(glm.vec3(self.player_hp_bar_left_x + self.player_hp_bar_width * 0.5, self.player_hp_bar_y, 0))
    player_bg_transform:set_scale(glm.vec3(self.player_hp_bar_width, 0.32, 1))
    local player_bg_renderer = player_bg_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.gray then
        player_bg_renderer:set_sprite(self.ui_sprites.gray)
    elseif self.ui_sprites.red then
        player_bg_renderer:set_sprite(self.ui_sprites.red)
    end
    player_bg_renderer:set_color(0.1, 0.1, 0.1, 0.92)
    player_bg_renderer:set_sorting_layer(2)
    player_bg_renderer:set_order_in_layer(6)
    self:AddGameObject(player_bg_go)
    self.player_hp_bar_bg_go = player_bg_go

    local player_fill_go = GameObject.new("PlayerHpBarFill")
    local player_fill_transform = player_fill_go:AddComponent(Transform)
    player_fill_transform:set_position(glm.vec3(self.player_hp_bar_left_x + self.player_hp_bar_width * 0.5, self.player_hp_bar_y, 0))
    player_fill_transform:set_scale(glm.vec3(self.player_hp_bar_width, 0.22, 1))
    local player_fill_renderer = player_fill_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        player_fill_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        player_fill_renderer:set_sprite(self.ui_sprites.gray)
    end
    player_fill_renderer:set_color(0.22, 0.86, 0.35, 0.96)
    player_fill_renderer:set_sorting_layer(2)
    player_fill_renderer:set_order_in_layer(7)
    self:AddGameObject(player_fill_go)
    self.player_hp_bar_fill_go = player_fill_go

    local shield_bg_go = GameObject.new("ShieldBarBg")
    local shield_bg_transform = shield_bg_go:AddComponent(Transform)
    shield_bg_transform:set_position(glm.vec3(self.shield_bar_left_x + self.shield_bar_width * 0.5, self.shield_bar_y, 0))
    shield_bg_transform:set_scale(glm.vec3(self.shield_bar_width, 0.18, 1))
    local shield_bg_renderer = shield_bg_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.gray then
        shield_bg_renderer:set_sprite(self.ui_sprites.gray)
    elseif self.ui_sprites.red then
        shield_bg_renderer:set_sprite(self.ui_sprites.red)
    end
    shield_bg_renderer:set_color(0.1, 0.1, 0.1, 0.86)
    shield_bg_renderer:set_sorting_layer(2)
    shield_bg_renderer:set_order_in_layer(10)
    self:AddGameObject(shield_bg_go)
    self.shield_bar_bg_go = shield_bg_go

    local shield_fill_go = GameObject.new("ShieldBarFill")
    local shield_fill_transform = shield_fill_go:AddComponent(Transform)
    shield_fill_transform:set_position(glm.vec3(self.shield_bar_left_x + self.shield_bar_width * 0.5, self.shield_bar_y, 0))
    shield_fill_transform:set_scale(glm.vec3(0.001, 0.14, 1))
    local shield_fill_renderer = shield_fill_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        shield_fill_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        shield_fill_renderer:set_sprite(self.ui_sprites.gray)
    end
    shield_fill_renderer:set_color(0.25, 0.62, 1.0, 0.95)
    shield_fill_renderer:set_sorting_layer(2)
    shield_fill_renderer:set_order_in_layer(11)
    self:AddGameObject(shield_fill_go)
    self.shield_bar_fill_go = shield_fill_go

    local shield_seg_width = (self.shield_bar_width - 0.12) / self.shield_segment_max
    for i = 1, self.shield_segment_max do
        local seg_go = GameObject.new("ShieldSegment_" .. i)
        local seg_transform = seg_go:AddComponent(Transform)
        local seg_center_x = self.shield_bar_left_x + shield_seg_width * (i - 0.5)
        seg_transform:set_position(glm.vec3(seg_center_x, self.shield_bar_y, 0))
        seg_transform:set_scale(glm.vec3(shield_seg_width - 0.03, 0.08, 1))
        local seg_renderer = seg_go:AddComponent(SpriteRenderer)
        if self.ui_sprites.red then
            seg_renderer:set_sprite(self.ui_sprites.red)
        elseif self.ui_sprites.gray then
            seg_renderer:set_sprite(self.ui_sprites.gray)
        end
        seg_renderer:set_color(0.2, 0.62, 1.0, 0.0)
        seg_renderer:set_sorting_layer(2)
        seg_renderer:set_order_in_layer(12)
        self:AddGameObject(seg_go)
        seg_go:set_active_self(false)
        self.shield_segment_gos[i] = seg_go
    end

    local fury_bg_go = GameObject.new("FuryBarBg")
    local fury_bg_transform = fury_bg_go:AddComponent(Transform)
    fury_bg_transform:set_position(glm.vec3(self.fury_bar_left_x + self.fury_bar_width * 0.5, self.fury_bar_y, 0))
    fury_bg_transform:set_scale(glm.vec3(self.fury_bar_width, 0.18, 1))
    local fury_bg_renderer = fury_bg_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.gray then
        fury_bg_renderer:set_sprite(self.ui_sprites.gray)
    elseif self.ui_sprites.red then
        fury_bg_renderer:set_sprite(self.ui_sprites.red)
    end
    fury_bg_renderer:set_color(0.1, 0.1, 0.1, 0.86)
    fury_bg_renderer:set_sorting_layer(2)
    fury_bg_renderer:set_order_in_layer(12)
    self:AddGameObject(fury_bg_go)
    self.fury_bar_bg_go = fury_bg_go

    local fury_fill_go = GameObject.new("FuryBarFill")
    local fury_fill_transform = fury_fill_go:AddComponent(Transform)
    fury_fill_transform:set_position(glm.vec3(self.fury_bar_left_x + self.fury_bar_width * 0.5, self.fury_bar_y, 0))
    fury_fill_transform:set_scale(glm.vec3(0.001, 0.14, 1))
    local fury_fill_renderer = fury_fill_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        fury_fill_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        fury_fill_renderer:set_sprite(self.ui_sprites.gray)
    end
    fury_fill_renderer:set_color(0.85, 0.28, 1.0, 0.95)
    fury_fill_renderer:set_sorting_layer(2)
    fury_fill_renderer:set_order_in_layer(13)
    self:AddGameObject(fury_fill_go)
    self.fury_bar_fill_go = fury_fill_go

    local fury_burst_go = GameObject.new("FuryBurstAura")
    local fury_burst_transform = fury_burst_go:AddComponent(Transform)
    fury_burst_transform:set_position(glm.vec3(self.fury_bar_left_x + self.fury_bar_width * 0.5, self.fury_bar_y, 0))
    fury_burst_transform:set_scale(glm.vec3(self.fury_bar_width + 0.3, 0.36, 1))
    local fury_burst_renderer = fury_burst_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        fury_burst_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        fury_burst_renderer:set_sprite(self.ui_sprites.gray)
    end
    fury_burst_renderer:set_color(0.88, 0.26, 1.0, 0.0)
    fury_burst_renderer:set_sorting_layer(2)
    fury_burst_renderer:set_order_in_layer(14)
    self:AddGameObject(fury_burst_go)
    self.fury_burst_go = fury_burst_go
    fury_burst_go:set_active_self(false)

    local shield_break_go = GameObject.new("ShieldBreakFlash")
    local shield_break_transform = shield_break_go:AddComponent(Transform)
    shield_break_transform:set_position(glm.vec3(self.shield_bar_left_x + self.shield_bar_width * 0.5, self.shield_bar_y, 0))
    shield_break_transform:set_scale(glm.vec3(self.shield_bar_width + 0.4, 0.44, 1))
    local shield_break_renderer = shield_break_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        shield_break_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        shield_break_renderer:set_sprite(self.ui_sprites.gray)
    end
    shield_break_renderer:set_color(0.62, 0.88, 1.0, 0.0)
    shield_break_renderer:set_sorting_layer(2)
    shield_break_renderer:set_order_in_layer(15)
    self:AddGameObject(shield_break_go)
    self.shield_break_go = shield_break_go
    shield_break_go:set_active_self(false)

    local fury_ready_go = GameObject.new("FuryReadyPrompt")
    local fury_ready_transform = fury_ready_go:AddComponent(Transform)
    fury_ready_transform:set_position(glm.vec3(self.fury_bar_left_x + self.fury_bar_width * 0.5, self.fury_bar_y + 0.38, 0))
    fury_ready_transform:set_scale(glm.vec3(self.fury_bar_width + 0.1, 0.2, 1))
    local fury_ready_renderer = fury_ready_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        fury_ready_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        fury_ready_renderer:set_sprite(self.ui_sprites.gray)
    end
    fury_ready_renderer:set_color(1.0, 0.86, 0.25, 0.0)
    fury_ready_renderer:set_sorting_layer(2)
    fury_ready_renderer:set_order_in_layer(16)
    self:AddGameObject(fury_ready_go)
    self.fury_ready_prompt_go = fury_ready_go
    fury_ready_go:set_active_self(false)

    local round_go = GameObject.new("RoundPrompt")
    local round_transform = round_go:AddComponent(Transform)
    round_transform:set_position(glm.vec3(0, 4.2, 0))
    round_transform:set_scale(glm.vec3(2.7, 0.42, 1))
    local round_renderer = round_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        round_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        round_renderer:set_sprite(self.ui_sprites.gray)
    end
    round_renderer:set_color(0.95, 0.82, 0.22, 0.0)
    round_renderer:set_sorting_layer(2)
    round_renderer:set_order_in_layer(2)
    self:AddGameObject(round_go)
    self.round_prompt_go = round_go
    round_go:set_active_self(false)

    local combo_go = GameObject.new("ComboPrompt")
    local combo_transform = combo_go:AddComponent(Transform)
    combo_transform:set_position(glm.vec3(0, 3.55, 0))
    combo_transform:set_scale(glm.vec3(0.8, 0.22, 1))
    local combo_renderer = combo_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        combo_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        combo_renderer:set_sprite(self.ui_sprites.gray)
    end
    combo_renderer:set_color(1.0, 0.55, 0.18, 0.0)
    combo_renderer:set_sorting_layer(2)
    combo_renderer:set_order_in_layer(8)
    self:AddGameObject(combo_go)
    self.combo_prompt_go = combo_go
    combo_go:set_active_self(false)

    for i = 1, self.combo_tick_max do
        local tick_go = GameObject.new("ComboTick_" .. i)
        local tick_transform = tick_go:AddComponent(Transform)
        tick_transform:set_position(glm.vec3(-2.4 + (i - 1) * 0.68, 3.02, 0))
        tick_transform:set_scale(glm.vec3(0.42, 0.12, 1))
        local tick_renderer = tick_go:AddComponent(SpriteRenderer)
        if self.ui_sprites.red then
            tick_renderer:set_sprite(self.ui_sprites.red)
        elseif self.ui_sprites.gray then
            tick_renderer:set_sprite(self.ui_sprites.gray)
        end
        tick_renderer:set_color(1, 1, 1, 0.0)
        tick_renderer:set_sorting_layer(2)
        tick_renderer:set_order_in_layer(9)
        self:AddGameObject(tick_go)
        tick_go:set_active_self(false)
        self.combo_tick_slots[i] = tick_go
    end

    local hit_flash_go = GameObject.new("HitFlash")
    local hit_flash_transform = hit_flash_go:AddComponent(Transform)
    hit_flash_transform:set_position(glm.vec3(0, 0, 0))
    hit_flash_transform:set_scale(glm.vec3(16, 10, 1))
    local hit_flash_renderer = hit_flash_go:AddComponent(SpriteRenderer)
    if self.ui_sprites.red then
        hit_flash_renderer:set_sprite(self.ui_sprites.red)
    elseif self.ui_sprites.gray then
        hit_flash_renderer:set_sprite(self.ui_sprites.gray)
    end
    hit_flash_renderer:set_color(0.95, 0.1, 0.1, 0.0)
    hit_flash_renderer:set_sorting_layer(3)
    hit_flash_renderer:set_order_in_layer(30)
    self:AddGameObject(hit_flash_go)
    self.hit_flash_go = hit_flash_go
    hit_flash_go:set_active_self(false)

    for i = 1, 3 do
        local slot_go = GameObject.new("DropPromptSlot_" .. i)
        local slot_transform = slot_go:AddComponent(Transform)
        slot_transform:set_position(glm.vec3(4.2, 4.25 - (i - 1) * 0.9, 0))
        slot_transform:set_scale(glm.vec3(0.65, 0.65, 1))
        local slot_renderer = slot_go:AddComponent(SpriteRenderer)
        if self.gem_sprites[Match3Board.GemType.RED] then
            slot_renderer:set_sprite(self.gem_sprites[Match3Board.GemType.RED])
        end
        slot_renderer:set_color(1, 1, 1, 0.0)
        slot_renderer:set_sorting_layer(2)
        slot_renderer:set_order_in_layer(5 - i)
        self:AddGameObject(slot_go)
        slot_go:set_active_self(false)
        self.drop_prompt_slots[i] = slot_go
    end

    self:UpdateEnemyHpBar()
    self:UpdatePlayerHpBar()
    self:UpdateShieldBar()
    self:UpdateFuryBar()
end

function Match3Scene:UpdateEnemyHpBar()
    if not self.enemy_hp_bar_fill_go then
        return
    end
    local ratio = self.enemy_hp / self.max_enemy_hp
    if ratio < 0 then ratio = 0 end
    if ratio > 1 then ratio = 1 end
    local width = math.max(0.001, self.enemy_hp_bar_width * ratio)
    local fill_transform = self.enemy_hp_bar_fill_go:GetComponent(Transform)
    fill_transform:set_scale(glm.vec3(width, 0.24, 1))
    fill_transform:set_position(glm.vec3(self.enemy_hp_bar_left_x + width * 0.5, self.enemy_hp_bar_y, 0))
end

function Match3Scene:UpdatePlayerHpBar()
    if not self.player_hp_bar_fill_go then
        return
    end
    local ratio = self.player_hp / self.max_player_hp
    if ratio < 0 then ratio = 0 end
    if ratio > 1 then ratio = 1 end
    local width = math.max(0.001, self.player_hp_bar_width * ratio)
    local fill_transform = self.player_hp_bar_fill_go:GetComponent(Transform)
    fill_transform:set_scale(glm.vec3(width, 0.22, 1))
    local shake_x = 0
    local shake_y = 0
    if self.player_hp_shake_timer > 0 then
        local n = self.player_hp_shake_timer / self.player_hp_shake_duration
        local amp = self.player_hp_shake_strength * n
        shake_x = math.sin(self.player_hp_pulse_time * 70.0) * amp
        shake_y = math.cos(self.player_hp_pulse_time * 60.0) * amp * 0.35
    end
    fill_transform:set_position(glm.vec3(self.player_hp_bar_left_x + width * 0.5 + shake_x, self.player_hp_bar_y + shake_y, 0))
    local renderer = self.player_hp_bar_fill_go:GetComponent(SpriteRenderer)
    if ratio <= self.player_hp_pulse_threshold then
        local pulse = 0.5 + 0.5 * math.sin(self.player_hp_pulse_time * 10.0)
        local g = 0.2 + 0.25 * pulse
        local b = 0.2 + 0.1 * pulse
        local alpha = 0.82 + 0.18 * pulse
        renderer:set_color(0.95, g, b, alpha)
    else
        renderer:set_color(0.22, 0.86, 0.35, 0.96)
    end
end

function Match3Scene:UpdateShieldBar()
    if not self.shield_bar_fill_go then
        return
    end
    local ratio = self.player_shield_points / self.player_shield_cap
    if ratio < 0 then ratio = 0 end
    if ratio > 1 then ratio = 1 end
    local width = math.max(0.001, self.shield_bar_width * ratio)
    local transform = self.shield_bar_fill_go:GetComponent(Transform)
    transform:set_scale(glm.vec3(width, 0.14, 1))
    transform:set_position(glm.vec3(self.shield_bar_left_x + width * 0.5, self.shield_bar_y, 0))
    local renderer = self.shield_bar_fill_go:GetComponent(SpriteRenderer)
    local pulse = 0
    if self.shield_flash_timer > 0 then
        pulse = self.shield_flash_timer / 0.5
    end
    renderer:set_color(0.2 + 0.2 * pulse, 0.55 + 0.35 * pulse, 1.0, 0.88 + 0.12 * pulse)

    local segment_value = ratio * self.shield_segment_max
    for i = 1, self.shield_segment_max do
        local seg_go = self.shield_segment_gos[i]
        if seg_go then
            local seg_fill = segment_value - (i - 1)
            if seg_fill > 0 then
                seg_go:set_active_self(true)
                local clamped = math.max(0, math.min(1, seg_fill))
                local seg_renderer = seg_go:GetComponent(SpriteRenderer)
                seg_renderer:set_color(0.22, 0.62 + 0.22 * clamped, 1.0, 0.3 + 0.7 * clamped)
            else
                seg_go:set_active_self(false)
            end
        end
    end
end

function Match3Scene:UpdateFuryBar()
    if not self.fury_bar_fill_go then
        return
    end
    local ratio = self.fury_charges / self.fury_charge_cap
    if ratio < 0 then ratio = 0 end
    if ratio > 1 then ratio = 1 end
    local width = math.max(0.001, self.fury_bar_width * ratio)
    local transform = self.fury_bar_fill_go:GetComponent(Transform)
    transform:set_scale(glm.vec3(width, 0.14, 1))
    transform:set_position(glm.vec3(self.fury_bar_left_x + width * 0.5, self.fury_bar_y, 0))
    local renderer = self.fury_bar_fill_go:GetComponent(SpriteRenderer)
    local pulse = 0
    if self.fury_flash_timer > 0 then
        pulse = self.fury_flash_timer / 0.5
    end
    renderer:set_color(0.75 + 0.25 * pulse, 0.18 + 0.24 * pulse, 0.92 + 0.08 * pulse, 0.88 + 0.12 * pulse)

    if self.fury_burst_go then
        if ratio >= 1 then
            self.fury_burst_go:set_active_self(true)
            local aura_renderer = self.fury_burst_go:GetComponent(SpriteRenderer)
            local wave = 0.5 + 0.5 * math.sin(self.fury_burst_phase_time * 10.0)
            aura_renderer:set_color(0.88 + 0.12 * wave, 0.25 + 0.2 * wave, 1.0, 0.16 + 0.26 * wave)
        else
            self.fury_burst_go:set_active_self(false)
        end
    end
end

function Match3Scene:TriggerShieldBreak()
    self.shield_break_timer = self.shield_break_duration
    if self.shield_break_go then
        self.shield_break_go:set_active_self(true)
        local renderer = self.shield_break_go:GetComponent(SpriteRenderer)
        renderer:set_color(0.62, 0.88, 1.0, 0.85)
    end
end

function Match3Scene:TriggerFuryReadyPrompt()
    self.fury_ready_prompt_timer = self.fury_ready_prompt_duration
    if self.fury_ready_prompt_go then
        self.fury_ready_prompt_go:set_active_self(true)
        local renderer = self.fury_ready_prompt_go:GetComponent(SpriteRenderer)
        renderer:set_color(1.0, 0.86, 0.25, 0.92)
    end
end

function Match3Scene:TriggerHitFlash(intensity)
    intensity = intensity or 1.0
    self.hit_flash_runtime_duration = self.hit_flash_duration * (1.0 + 0.9 * (intensity - 1.0))
    self.hit_flash_runtime_duration = math.max(0.2, math.min(0.8, self.hit_flash_runtime_duration))
    self.hit_flash_alpha = math.max(0.35, math.min(0.9, 0.45 * intensity))
    local t = math.max(0.0, math.min(1.0, (intensity - 1.0) / 1.5))
    self.hit_flash_color.r = 0.95
    self.hit_flash_color.g = 0.1 + 0.15 * t
    self.hit_flash_color.b = 0.1 + 0.25 * t
    self.hit_flash_timer = self.hit_flash_runtime_duration
    if self.hit_flash_go then
        self.hit_flash_go:set_active_self(true)
        local renderer = self.hit_flash_go:GetComponent(SpriteRenderer)
        renderer:set_color(self.hit_flash_color.r, self.hit_flash_color.g, self.hit_flash_color.b, self.hit_flash_alpha)
    end
end

function Match3Scene:TriggerPlayerHpShake(intensity)
    intensity = intensity or 1.0
    self.player_hp_shake_timer = self.player_hp_shake_duration
    self.player_hp_shake_strength = math.max(0.02, math.min(0.2, 0.07 * intensity))
end

function Match3Scene:ShowComboPrompt(combo, match_count, main_type)
    self.combo_count = combo
    self.combo_prompt_timer = self.combo_prompt_duration
    if main_type and self.combo_theme_colors[main_type] then
        self.combo_theme_type = main_type
    end
    local overflow_steps = math.max(0, combo - self.combo_overflow_start + 1)
    local theme_color = self.combo_theme_colors[self.combo_theme_type] or self.combo_theme_colors[Match3Board.GemType.RED]
    if self.combo_prompt_go then
        self.combo_prompt_go:set_active_self(true)
        local transform = self.combo_prompt_go:GetComponent(Transform)
        local width = math.min(5.2, 0.8 + combo * 0.35 + match_count * 0.03 + overflow_steps * 0.18)
        transform:set_scale(glm.vec3(width, 0.22, 1))
        local renderer = self.combo_prompt_go:GetComponent(SpriteRenderer)
        if overflow_steps > 0 then
            renderer:set_color(math.min(1.0, theme_color.r + 0.15), math.min(1.0, theme_color.g + 0.12), math.min(1.0, theme_color.b + 0.08), 0.95)
        else
            local t = combo % 3
            if t == 1 then
                renderer:set_color(1.0, 0.55, 0.18, 0.95)
            elseif t == 2 then
                renderer:set_color(1.0, 0.3, 0.3, 0.95)
            else
                renderer:set_color(0.9, 0.3, 1.0, 0.95)
            end
        end
    end
    self.combo_overflow = combo > self.combo_tick_max
    self.combo_overflow_phase_time = 0
    local active_ticks = math.min(self.combo_tick_max, combo)
    for i = 1, self.combo_tick_max do
        local tick_go = self.combo_tick_slots[i]
        if tick_go then
            local tick_renderer = tick_go:GetComponent(SpriteRenderer)
            if i <= active_ticks then
                tick_go:set_active_self(true)
                local t = (i - 1) / math.max(1, self.combo_tick_max - 1)
                tick_renderer:set_color(1.0, 0.35 + 0.55 * t, 0.2 + 0.6 * (1 - t), 0.95)
            else
                tick_go:set_active_self(false)
            end
        end
    end
end

function Match3Scene:ApplyEnemyCounterAttack(damage)
    if self.enemy_hp <= 0 then
        return
    end
    local prev_shield = self.player_shield_points
    local blocked = math.min(damage, self.player_shield_points)
    if blocked > 0 then
        self.player_shield_points = self.player_shield_points - blocked
        self.shield_flash_timer = math.min(0.5, self.shield_flash_timer + 0.22)
        self:UpdateShieldBar()
        if prev_shield > 0 and self.player_shield_points <= 0 then
            self:TriggerShieldBreak()
        end
    end
    local real_damage = damage - blocked
    if real_damage > 0 then
        self.player_hp = math.max(0, self.player_hp - real_damage)
        self:UpdatePlayerHpBar()
        local hp_ratio = self.player_hp / self.max_player_hp
        local intensity = 1.0 + (1.0 - hp_ratio) * 1.2
        self:TriggerHitFlash(intensity)
        self:TriggerPlayerHpShake(intensity)
    else
        self:TriggerHitFlash(0.85)
    end
    print("Player took " .. real_damage .. " damage (blocked " .. blocked .. ")! HP: " .. self.player_hp .. " | Shield: " .. self.player_shield_points)
end

function Match3Scene:ShowRoundPrompt()
    self.round_prompt_timer = 0.9
    if self.round_prompt_go then
        self.round_prompt_go:set_active_self(true)
        local renderer = self.round_prompt_go:GetComponent(SpriteRenderer)
        local t = self.round_index % 4
        if t == 1 then
            renderer:set_color(0.94, 0.82, 0.22, 0.95)
        elseif t == 2 then
            renderer:set_color(0.22, 0.82, 0.94, 0.95)
        elseif t == 3 then
            renderer:set_color(0.82, 0.44, 0.94, 0.95)
        else
            renderer:set_color(0.42, 0.94, 0.52, 0.95)
        end
    end
end

function Match3Scene:PushDropPrompt(main_type, count)
    for i = 3, 2, -1 do
        self.drop_prompt_types[i] = self.drop_prompt_types[i - 1]
        self.drop_prompt_counts[i] = self.drop_prompt_counts[i - 1]
        self.drop_prompt_timers[i] = self.drop_prompt_timers[i - 1]
    end

    self.drop_prompt_types[1] = main_type
    self.drop_prompt_counts[1] = count
    self.drop_prompt_timers[1] = 2.2

    for i = 1, 3 do
        local go = self.drop_prompt_slots[i]
        if go then
            local renderer = go:GetComponent(SpriteRenderer)
            local transform = go:GetComponent(Transform)
            local gem_type = self.drop_prompt_types[i]
            local gem_count = self.drop_prompt_counts[i] or 0
            if gem_type and self.gem_sprites[gem_type] then
                renderer:set_sprite(self.gem_sprites[gem_type])
            end
            local scale = 0.5 + math.min(0.5, gem_count * 0.05)
            transform:set_scale(glm.vec3(scale, scale, 1))
            if self.drop_prompt_timers[i] > 0 then
                go:set_active_self(true)
                renderer:set_color(1, 1, 1, math.min(1.0, self.drop_prompt_timers[i] / 2.2))
            else
                go:set_active_self(false)
            end
        end
    end
end

function Match3Scene:UpdateCombatUI(delta_time)
    self.player_hp_pulse_time = self.player_hp_pulse_time + delta_time
    self.fury_burst_phase_time = self.fury_burst_phase_time + delta_time
    if self.player_hp_shake_timer > 0 then
        self.player_hp_shake_timer = math.max(0, self.player_hp_shake_timer - delta_time)
    end
    if self.shield_flash_timer > 0 then
        self.shield_flash_timer = math.max(0, self.shield_flash_timer - delta_time)
    end
    if self.fury_flash_timer > 0 then
        self.fury_flash_timer = math.max(0, self.fury_flash_timer - delta_time)
    end
    if self.shield_break_timer > 0 then
        self.shield_break_timer = math.max(0, self.shield_break_timer - delta_time)
    end
    if self.fury_ready_prompt_timer > 0 then
        self.fury_ready_prompt_timer = math.max(0, self.fury_ready_prompt_timer - delta_time)
    end
    self.combo_overflow_phase_time = self.combo_overflow_phase_time + delta_time
    if self.round_prompt_timer > 0 and self.round_prompt_go then
        self.round_prompt_timer = self.round_prompt_timer - delta_time
        local renderer = self.round_prompt_go:GetComponent(SpriteRenderer)
        if self.round_prompt_timer > 0 then
            local alpha = math.min(1.0, self.round_prompt_timer / 0.9)
            local color = renderer:color()
            renderer:set_color(color.r, color.g, color.b, alpha)
        else
            self.round_prompt_go:set_active_self(false)
        end
    end

    for i = 1, 3 do
        if self.drop_prompt_timers[i] and self.drop_prompt_timers[i] > 0 then
            self.drop_prompt_timers[i] = self.drop_prompt_timers[i] - delta_time
            local go = self.drop_prompt_slots[i]
            if go then
                local renderer = go:GetComponent(SpriteRenderer)
                if self.drop_prompt_timers[i] > 0 then
                    local alpha = math.min(1.0, self.drop_prompt_timers[i] / 2.2)
                    renderer:set_color(1, 1, 1, alpha)
                    go:set_active_self(true)
                else
                    go:set_active_self(false)
                end
            end
        end
    end

    if self.hit_flash_timer > 0 and self.hit_flash_go then
        self.hit_flash_timer = self.hit_flash_timer - delta_time
        local renderer = self.hit_flash_go:GetComponent(SpriteRenderer)
        if self.hit_flash_timer > 0 then
            local alpha = self.hit_flash_alpha * (self.hit_flash_timer / self.hit_flash_runtime_duration)
            renderer:set_color(self.hit_flash_color.r, self.hit_flash_color.g, self.hit_flash_color.b, alpha)
        else
            self.hit_flash_go:set_active_self(false)
        end
    end

    if self.shield_break_timer > 0 and self.shield_break_go then
        local renderer = self.shield_break_go:GetComponent(SpriteRenderer)
        local alpha = 0.9 * (self.shield_break_timer / self.shield_break_duration)
        renderer:set_color(0.62, 0.88, 1.0, alpha)
    elseif self.shield_break_go then
        self.shield_break_go:set_active_self(false)
    end

    if self.fury_ready_prompt_timer > 0 and self.fury_ready_prompt_go then
        local renderer = self.fury_ready_prompt_go:GetComponent(SpriteRenderer)
        local wave = 0.5 + 0.5 * math.sin(self.fury_burst_phase_time * 12.0)
        local alpha = (0.4 + 0.6 * wave) * (self.fury_ready_prompt_timer / self.fury_ready_prompt_duration)
        renderer:set_color(1.0, 0.86, 0.25, alpha)
    elseif self.fury_ready_prompt_go then
        self.fury_ready_prompt_go:set_active_self(false)
    end

    if self.combo_prompt_timer > 0 and self.combo_prompt_go then
        self.combo_prompt_timer = self.combo_prompt_timer - delta_time
        local renderer = self.combo_prompt_go:GetComponent(SpriteRenderer)
        if self.combo_prompt_timer > 0 then
            local alpha = math.min(0.95, self.combo_prompt_timer / self.combo_prompt_duration)
            local color = renderer:color()
            renderer:set_color(color.r, color.g, color.b, alpha)
            for i = 1, self.combo_tick_max do
                local tick_go = self.combo_tick_slots[i]
                if tick_go and tick_go:active_self() then
                    local tick_renderer = tick_go:GetComponent(SpriteRenderer)
                    if self.combo_overflow then
                        local phase = (i - 1) / self.combo_tick_max
                        local pulse = 0.5 + 0.5 * math.sin(self.combo_overflow_phase_time * 10.0 - phase * 6.28318)
                        local theme = self.combo_theme_colors[self.combo_theme_type] or self.combo_theme_colors[Match3Board.GemType.RED]
                        local r = math.min(1.0, theme.r * (0.65 + 0.5 * pulse))
                        local g = math.min(1.0, theme.g * (0.65 + 0.5 * (1 - pulse)))
                        local b = math.min(1.0, theme.b * (0.65 + 0.5 * pulse))
                        tick_renderer:set_color(r, g, b, alpha)
                    else
                        local tick_color = tick_renderer:color()
                        tick_renderer:set_color(tick_color.r, tick_color.g, tick_color.b, alpha)
                    end
                end
            end
        else
            self.combo_prompt_go:set_active_self(false)
            self.combo_overflow = false
            for i = 1, self.combo_tick_max do
                local tick_go = self.combo_tick_slots[i]
                if tick_go then
                    tick_go:set_active_self(false)
                end
            end
        end
    end

    self:UpdatePlayerHpBar()
    self:UpdateShieldBar()
    self:UpdateFuryBar()
end

function Match3Scene:UpdateVisuals()
    if not self.gem_objects or next(self.gem_objects) == nil then
        return 
    end

    for y = 1, self.board.height do
        if not self.gem_objects[y] then self.gem_objects[y] = {} end
        for x = 1, self.board.width do
            local gem_data = self.board:GetGem(x, y)
            local go = self.gem_objects[y][x]
            
            if go then
                local renderer = go:GetComponent(SpriteRenderer)
                
                if gem_data.type == Match3Board.GemType.EMPTY then
                    go:set_active_self(false)
                else
                    go:set_active_self(true)
                    if self.gem_sprites[gem_data.type] then
                        renderer:set_sprite(self.gem_sprites[gem_data.type])
                    end
                    if self.selected_gem_pos and self.selected_gem_pos.x == x and self.selected_gem_pos.y == y then
                        renderer:set_color(0.6, 0.6, 0.6, 1.0)
                        go:GetComponent(Transform):set_scale(glm.vec3(0.7, 0.7, 1.0))
                    else
                        renderer:set_color(1.0, 1.0, 1.0, 1.0)
                        go:GetComponent(Transform):set_scale(glm.vec3(0.9, 0.9, 1.0))
                    end
                end
            end
        end
    end
end

function Match3Scene:GridToWorld(x, y)
    return glm.vec3(x - 1 + self.board_offset.x, y - 1 + self.board_offset.y, 0)
end

function Match3Scene:ScreenToWorld(screen_pos)
    local screen_w = Screen.width()
    local screen_h = Screen.height()
    
    -- Normalize to [-1, 1] (NDC)
    -- Note: Screen origin might be top-left or bottom-left. 
    -- Typically Input.mousePosition is bottom-left (0,0).
    -- If it's top-left, we need to flip y.
    -- Assuming bottom-left based on typical OpenGL engines.
    
    local ndc_x = (screen_pos.x / screen_w) * 2 - 1
    local ndc_y = (screen_pos.y / screen_h) * 2 - 1
    
    local aspect = screen_w / screen_h
    local cam_height = self.camera_size
    local cam_width = cam_height * aspect
    
    -- Camera position (assuming at 0,0,10)
    local cam_pos = self.camera_go:GetComponent(Transform):position()
    
    local world_x = cam_pos.x + ndc_x * cam_width
    local world_y = cam_pos.y + ndc_y * cam_height
    
    return glm.vec3(world_x, world_y, 0)
end

function Match3Scene:Update()
    self:UpdateCombatUI(Time.delta_time())
    -- Input Handling
    if Input.GetMouseButtonDown(0) then
        local mouse_pos = Input.mousePosition()
        local world_pos = self:ScreenToWorld(mouse_pos)
        
        -- Convert World to Grid
        local x = math.floor(world_pos.x - self.board_offset.x + 0.5) + 1
        local y = math.floor(world_pos.y - self.board_offset.y + 0.5) + 1
        
        if x >= 1 and x <= self.board.width and y >= 1 and y <= self.board.height then
            self:OnGemClicked(x, y)
        else
            -- Clicked outside, deselect
            self.selected_gem_pos = nil
            self:UpdateVisuals()
        end
    end
end

function Match3Scene:OnGemClicked(x, y)
    if self.moves_left <= 0 or self.is_animating then return end

    print("Clicked gem: " .. x .. ", " .. y)
    
    if not self.selected_gem_pos then
        self.selected_gem_pos = {x=x, y=y}
        self:UpdateVisuals()
    else
        local sx, sy = self.selected_gem_pos.x, self.selected_gem_pos.y
        
        -- Check if adjacent
        if math.abs(sx - x) + math.abs(sy - y) == 1 then
            print("Swapping " .. sx .. "," .. sy .. " with " .. x .. "," .. y)
            
            self.is_animating = true
            -- Try Swap
            self.board:SwapGems(sx, sy, x, y)
            
            -- Check Matches
            local matches = self.board:FindMatches()
            if #matches > 0 then
                print("Match found! Removing " .. #matches .. " gems")
                self.moves_left = self.moves_left - 1
                self.round_index = self.round_index + 1
                self:ShowRoundPrompt()
                self.current_turn_combo = 0
                self:HandleMatches(matches, 1)
            else
                print("No match, swapping back")
                self.board:SwapGems(sx, sy, x, y) -- Swap back
                local counter_damage = 6 + math.floor(self.round_index * 0.35)
                self:ApplyEnemyCounterAttack(counter_damage)
            end
            
            self.is_animating = false
            self.selected_gem_pos = nil
            self:UpdateVisuals()
            self:PrintBattleStatus()
            self:UpdateEnemyHpBar()
            self:UpdatePlayerHpBar()
            
            if self.enemy_hp <= 0 then
                print("Victory! Final Score: " .. self.score)
                local GameManager = require("script/mirror_game/core/game_manager")
                GameManager.change_state(GameManager.State.VICTORY)
                return
            end
            if self.player_hp <= 0 then
                print("Defeat! Player down.")
                local GameManager = require("script/mirror_game/core/game_manager")
                GameManager.change_state(GameManager.State.DEFEAT)
                return
            end
            if self.moves_left <= 0 then
                print("Defeat! Final Score: " .. self.score)
                local GameManager = require("script/mirror_game/core/game_manager")
                GameManager.change_state(GameManager.State.DEFEAT)
                return
            end
        else
            -- Select new if not adjacent (or same gem)
            if sx == x and sy == y then
                self.selected_gem_pos = nil -- Deselect
            else
                self.selected_gem_pos = {x=x, y=y}
            end
            self:UpdateVisuals()
        end
    end
end

function Match3Scene:HandleMatches(matches, chain_index)
    chain_index = chain_index or 1
    local damage = 0
    local type_counts = {}
    for _, pos in ipairs(matches) do
        local gem = self.board:GetGem(pos.x, pos.y)
        if gem.type == 1 then damage = damage + 2 end
        damage = damage + 1
        self.score = self.score + 10
        type_counts[gem.type] = (type_counts[gem.type] or 0) + 1
    end

    local main_type = Match3Board.GemType.RED
    local main_count = 0
    for gem_type, count in pairs(type_counts) do
        if count > main_count then
            main_type = gem_type
            main_count = count
        end
    end
    if chain_index > self.current_turn_combo then
        self.current_turn_combo = chain_index
    end
    self:ShowComboPrompt(chain_index, #matches, main_type)
    if main_count > 0 then
        self:PushDropPrompt(main_type, main_count)
    end
    
    local overflow_steps = math.max(0, chain_index - self.combo_overflow_start + 1)
    local overflow_bonus_step = self.combo_overflow_bonus_step + (self.combo_overflow_bonus_by_type[main_type] or 0)
    local combo_multiplier = 1 + (chain_index - 1) * 0.25 + overflow_steps * overflow_bonus_step
    local fury_spent = 0
    if self.fury_charges > 0 then
        fury_spent = math.min(self.fury_charges, 1)
        self.fury_charges = self.fury_charges - fury_spent
        combo_multiplier = combo_multiplier + fury_spent * self.fury_bonus_per_charge
        self.fury_flash_timer = 0.45
    end
    local final_damage = math.floor(damage * combo_multiplier + 0.5)
    self.enemy_hp = math.max(0, self.enemy_hp - final_damage)
    local extra_text = ""
    if overflow_steps > 0 and main_type == Match3Board.GemType.GREEN then
        local heal = math.max(1, math.floor(overflow_steps * 2))
        self.player_hp = math.min(self.max_player_hp, self.player_hp + heal)
        self:UpdatePlayerHpBar()
        extra_text = " | Heal +" .. heal
    elseif overflow_steps > 0 and main_type == Match3Board.GemType.YELLOW then
        local bonus_score = overflow_steps * 25
        self.score = self.score + bonus_score
        extra_text = " | Score +" .. bonus_score
    elseif overflow_steps > 0 and main_type == Match3Board.GemType.BLUE then
        local shield_gain = overflow_steps * 4
        self.player_shield_points = math.min(self.player_shield_cap, self.player_shield_points + shield_gain)
        self.shield_flash_timer = 0.5
        self:UpdateShieldBar()
        extra_text = " | Shield +" .. shield_gain
    elseif overflow_steps > 0 and main_type == Match3Board.GemType.PURPLE then
        local prev_fury = self.fury_charges
        local fury_gain = overflow_steps
        self.fury_charges = math.min(self.fury_charge_cap, self.fury_charges + fury_gain)
        self.fury_flash_timer = 0.5
        self:UpdateFuryBar()
        if prev_fury < self.fury_charge_cap and self.fury_charges >= self.fury_charge_cap then
            self:TriggerFuryReadyPrompt()
        end
        extra_text = " | Fury +" .. fury_gain
    end
    if fury_spent > 0 then
        extra_text = extra_text .. " | FuryBoost +" .. tostring(math.floor(fury_spent * self.fury_bonus_per_charge * 100 + 0.5)) .. "%"
    end
    local overflow_bonus_text = ""
    if overflow_steps > 0 then
        overflow_bonus_text = " | Overflow +" .. tostring(math.floor(overflow_steps * overflow_bonus_step * 100 + 0.5)) .. "%"
    end
    print("Enemy took " .. final_damage .. " damage! HP: " .. self.enemy_hp .. " | Combo x" .. chain_index .. overflow_bonus_text .. extra_text)
    self:UpdateEnemyHpBar()

    self.board:RemoveMatches(matches)
    self.board:ApplyGravity()

    local new_matches = self.board:FindMatches()
    if #new_matches > 0 then
        self:HandleMatches(new_matches, chain_index + 1)
    end
end

return Match3Scene
