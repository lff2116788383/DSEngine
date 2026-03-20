local Font = require("renderer/font")

local CharacterPanel = {}
CharacterPanel.__index = CharacterPanel

function CharacterPanel.new()
    local self = setmetatable({}, CharacterPanel)
    self.root_go = nil
    self.texts = {}
    self.font = nil
    return self
end

function CharacterPanel:Init(canvas)
    -- 加载字体
    self.font = Font.LoadFromFile("font/msyh.ttc", 24)

    -- 创建根节点
    self.root_go = GameObject.new("CharacterPanel")
    local transform = self.root_go:AddComponent(Transform)
    transform:set_parent(canvas)
    
    -- 背景
    local bg = GameObject.new("CharPanelBackground")
    local bg_tf = bg:AddComponent(Transform)
    bg_tf:set_parent(transform)
    bg_tf:set_position(glm.vec3(0, 0, 0))
    bg_tf:set_scale(glm.vec3(6, 8, 1))
    local renderer = bg:AddComponent(SpriteRenderer)
    
    local Texture2D = require("renderer/texture_2d")
    local Sprite = require("renderer/sprite")
    local bg_tex = Texture2D.LoadFromFile("../bin/data/images/gray.png") or Texture2D.LoadFromFile("images/gray.png")
    if bg_tex then
        local sprite = Sprite.Create(bg_tex)
        sprite:set_ppu(100)
        renderer:set_sprite(sprite)
        renderer:set_color(0.1, 0.1, 0.1, 0.95)
        renderer:set_sorting_layer(3)
        renderer:set_order_in_layer(100)
    end
    
    -- 属性文本
    self.texts["Name"] = self:CreateText("Name", 0, 3)
    self.texts["HP"] = self:CreateText("HP", 0, 1.5)
    self.texts["DC"] = self:CreateText("DC", 0, 0)
    self.texts["AC"] = self:CreateText("AC", 0, -1.5)
    
    -- 默认隐藏
    self.root_go:SetActive(false)
end

function CharacterPanel:CreateText(key, x, y)
    require("ui/ui_text")
    local go = GameObject.new("Text_" .. key)
    local tf = go:AddComponent(Transform)
    tf:set_parent(self.root_go:GetComponent(Transform))
    tf:set_position(glm.vec3(x, y, 0))
    
    local text_comp = go:AddComponent(UIText)
    if self.font then
        text_comp:set_font(self.font)
    end
    text_comp:set_text(key .. ": ???")
    text_comp:set_color(glm.vec4(1, 1, 1, 1))
    
    return text_comp
end

function CharacterPanel:Open()
    self.root_go:SetActive(true)
    self:Refresh()
    print("Character Panel Opened")
end

function CharacterPanel:Close()
    self.root_go:SetActive(false)
    print("Character Panel Closed")
end

function CharacterPanel:Refresh()
    local GameManager = require("script/mirror_game/core/game_manager")
    local player = GameManager.player
    if not player then return end
    
    local human = player:GetCurrentHuman()
    if human then
        if self.texts["Name"] then self.texts["Name"]:set_text("Player (Lv." .. human.level .. ")") end
        if self.texts["HP"] then self.texts["HP"]:set_text(string.format("HP: %d / %d", human.hp, human.max_hp)) end
        if self.texts["DC"] then self.texts["DC"]:set_text(string.format("Attack (DC): %d - %d", human.dc.min, human.dc.max)) end
        if self.texts["AC"] then self.texts["AC"]:set_text(string.format("Defense (AC): %d - %d", human.ac.min, human.ac.max)) end
    end
end

function CharacterPanel:Update()
    -- 实时更新逻辑（可选）
end

return CharacterPanel
