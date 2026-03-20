local Font = require("renderer/font")

local InventoryPanel = {}
InventoryPanel.__index = InventoryPanel

function InventoryPanel.new()
    local self = setmetatable({}, InventoryPanel)
    self.root_go = nil
    self.font = nil
    self.item_texts = {}
    self.buttons = {}
    return self
end

function InventoryPanel:Init(canvas)
    self.font = Font.LoadFromFile("font/msyh.ttc", 18)
    
    self.root_go = GameObject.new("InventoryPanel")
    local transform = self.root_go:AddComponent(Transform)
    transform:set_parent(canvas)
    
    -- 背景
    local bg = GameObject.new("InvPanelBackground")
    local bg_tf = bg:AddComponent(Transform)
    bg_tf:set_parent(transform)
    bg_tf:set_position(glm.vec3(0, 0, 0))
    bg_tf:set_scale(glm.vec3(8, 6, 1))
    local renderer = bg:AddComponent(SpriteRenderer)
    
    local Texture2D = require("renderer/texture_2d")
    local Sprite = require("renderer/sprite")
    local bg_tex = Texture2D.LoadFromFile("../bin/data/images/gray.png") or Texture2D.LoadFromFile("images/gray.png")
    if bg_tex then
        local sprite = Sprite.Create(bg_tex)
        sprite:set_ppu(100)
        renderer:set_sprite(sprite)
        renderer:set_color(0.1, 0.1, 0.2, 0.95)
        renderer:set_sorting_layer(3)
        renderer:set_order_in_layer(100)
    end
    
    -- 标题
    self:CreateText("Inventory Title", "--- Backpack ---", 0, 2.5)
    
    self.root_go:SetActive(false)
end

function InventoryPanel:CreateText(name, text, x, y)
    require("ui/ui_text")
    local go = GameObject.new("Text_" .. name)
    local tf = go:AddComponent(Transform)
    tf:set_parent(self.root_go:GetComponent(Transform))
    tf:set_position(glm.vec3(x, y, 0))
    
    local text_comp = go:AddComponent(UIText)
    if self.font then
        text_comp:set_font(self.font)
    end
    text_comp:set_text(text)
    text_comp:set_color(glm.vec4(1, 1, 1, 1))
    
    return text_comp
end

function InventoryPanel:ClearItems()
    for _, text_comp in ipairs(self.item_texts) do
        text_comp:game_object():Destroy()
    end
    self.item_texts = {}
    
    for _, btn_go in ipairs(self.buttons) do
        btn_go:Destroy()
    end
    self.buttons = {}
end

function InventoryPanel:CreateButton(name, text, x, y, callback)
    require("ui/ui_button")
    local go = GameObject.new("Btn_" .. name)
    local tf = go:AddComponent(Transform)
    tf:set_parent(self.root_go:GetComponent(Transform))
    tf:set_position(glm.vec3(x, y, 0))
    tf:set_scale(glm.vec3(0.5, 0.25, 1))
    
    -- 背景图
    local renderer = go:AddComponent(SpriteRenderer)
    local Texture2D = require("renderer/texture_2d")
    local Sprite = require("renderer/sprite")
    local bg_tex = Texture2D.LoadFromFile("../bin/data/images/gray.png") or Texture2D.LoadFromFile("images/gray.png")
    if bg_tex then
        local sprite = Sprite.Create(bg_tex)
        sprite:set_ppu(100)
        renderer:set_sprite(sprite)
        renderer:set_color(0.5, 0.5, 0.5, 1.0)
        renderer:set_sorting_layer(4)
        renderer:set_order_in_layer(101)
    end
    
    -- 按钮组件
    local btn = go:AddComponent(UIButton)
    if callback then
        btn:set_click_callback(callback)
    end
    
    -- 按钮文字
    local text_go = GameObject.new("BtnText_" .. name)
    local text_tf = text_go:AddComponent(Transform)
    text_tf:set_parent(tf)
    text_tf:set_position(glm.vec3(0, 0, 0))
    local text_comp = text_go:AddComponent(UIText)
    if self.font then
        text_comp:set_font(self.font)
    end
    text_comp:set_text(text)
    text_comp:set_color(glm.vec4(1, 1, 1, 1))
    
    table.insert(self.buttons, go)
    return btn
end

function InventoryPanel:Open()
    self.root_go:SetActive(true)
    self:Refresh()
end

function InventoryPanel:Close()
    self.root_go:SetActive(false)
    self:ClearItems()
end

function InventoryPanel:Refresh()
    self:ClearItems()
    
    local GameManager = require("script/mirror_game/core/game_manager")
    local player = GameManager.player
    if not player then return end
    
    local start_y = 1.5
    local y_offset = -0.5
    
    -- Items
    table.insert(self.item_texts, self:CreateText("Title_Items", "[Items]", -2, start_y))
    local current_y = start_y + y_offset
    for i, item in ipairs(player.bag_items) do
        local text = string.format("%s x%d", item.name, item.count)
        table.insert(self.item_texts, self:CreateText("Item_"..i, text, -2, current_y))
        current_y = current_y + y_offset
    end
    
    -- Equips
    table.insert(self.item_texts, self:CreateText("Title_Equips", "[Equipments]", 2, start_y))
    current_y = start_y + y_offset
    for i, equip in ipairs(player.bag_equips) do
        -- 判断是否已穿戴
        local is_equipped = false
        local human = player:GetCurrentHuman()
        for _, eq in pairs(human.equipments) do
            if eq == equip then is_equipped = true break end
        end
        
        local status = is_equipped and "[E]" or ""
        local text = string.format("%s %s (Lv.%d)", status, equip.name, equip.static_data.lv)
        table.insert(self.item_texts, self:CreateText("Equip_"..i, text, 1.5, current_y))
        
        -- 穿戴/卸下 按钮
        local btn_text = is_equipped and "Unequip" or "Equip"
        self:CreateButton("EquipBtn_"..i, btn_text, 3.5, current_y, function()
            if is_equipped then
                -- 简单假设卸下武器部位1
                human:TakeOffEquip(1)
                print("Unequipped " .. equip.name)
            else
                human:WearEquip(1, equip)
                print("Equipped " .. equip.name)
            end
            self:Refresh() -- 刷新界面
            
            -- 同时刷新角色面板如果打开的话
            local UIManager = require("script/mirror_game/systems/ui_manager")
            if UIManager.panels["character_panel"] and UIManager.panels["character_panel"].is_open then
                UIManager.panels["character_panel"]:Refresh()
            end
        end)
        
        current_y = current_y + y_offset
    end
end

return InventoryPanel
