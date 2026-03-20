local UIManager = {}

UIManager.panels = {}
UIManager.canvas = nil

function UIManager.Init()
    print("UIManager Initialized")
    -- 创建 UI Canvas
    local go = GameObject.new("UICanvas")
    UIManager.canvas = go:AddComponent(Transform)
    
    -- 可以在这里加载通用的 UI 资源
end

function UIManager.OpenPanel(panel_name)
    if UIManager.panels[panel_name] then
        if not UIManager.panels[panel_name].is_open then
            UIManager.panels[panel_name]:Open()
            UIManager.panels[panel_name].is_open = true
        end
    else
        -- 尝试加载
        local panel_class = require("script/mirror_game/ui/" .. panel_name)
        if panel_class then
            local panel = panel_class.new()
            panel:Init(UIManager.canvas)
            panel:Open()
            panel.is_open = true
            UIManager.panels[panel_name] = panel
        else
            print("Error: Failed to load panel " .. panel_name)
        end
    end
end

function UIManager.ClosePanel(panel_name)
    if UIManager.panels[panel_name] and UIManager.panels[panel_name].is_open then
        UIManager.panels[panel_name]:Close()
        UIManager.panels[panel_name].is_open = false
    end
end

function UIManager.TogglePanel(panel_name)
    if UIManager.panels[panel_name] and UIManager.panels[panel_name].is_open then
        UIManager.ClosePanel(panel_name)
    else
        UIManager.OpenPanel(panel_name)
    end
end

function UIManager.Update()
    -- 更新所有打开的面板
    for _, panel in pairs(UIManager.panels) do
        if panel.is_open and panel.Update then
            panel:Update()
        end
    end
end

return UIManager
