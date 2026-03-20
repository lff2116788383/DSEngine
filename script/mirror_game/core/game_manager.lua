local Player = require("script/mirror_game/object/player")
local UIManager = require("script/mirror_game/systems/ui_manager")
local SaveSystem = require("script/mirror_game/systems/save_system")
require("control/key_code")
require("control/input")

local GameManager = {}

-- 游戏状态枚举
GameManager.State = {
    MENU = "MENU",
    STORY = "STORY",
    MATCH3 = "MATCH3",
    VICTORY = "VICTORY",
    DEFEAT = "DEFEAT"
}

GameManager.current_state = nil
GameManager.current_scene = nil

-- 初始化游戏
function GameManager.init()
    print("[MirrorGame] GameManager initialized")
    
    -- 初始化 UI 管理器
    UIManager.Init()
    
    -- 初始化玩家数据
    GameManager.player = Player.GetInstance()
    
    -- 尝试加载存档，如果没有则给初始数据
    if not SaveSystem.LoadGame(GameManager.player) then
        -- 测试数据：给予初始物品和装备
        GameManager.player:AddItem(1001, 5) -- 5个小型金疮药
        GameManager.player:AddItem(1101, 3) -- 3个魔法药(小)
        GameManager.player:AddEquip(101)    -- 木剑
        GameManager.player:AddEquip(201)    -- 布衣(男)
        
        -- 测试装备穿戴
        local human = GameManager.player:GetCurrentHuman()
        local weapon = GameManager.player.bag_equips[1]
        if weapon then
            human:WearEquip(1, weapon) -- 假设位置1是武器
            print("Equipped weapon: " .. weapon.name)
        end
    end
    
    GameManager.change_state(GameManager.State.MENU)
end

-- 切换游戏状态
function GameManager.change_state(new_state)
    print("[MirrorGame] Changing state from " .. tostring(GameManager.current_state) .. " to " .. new_state)
    
    -- 退出旧状态
    if GameManager.current_state then
        -- 这里可以添加清理逻辑
    end
    
    GameManager.current_state = new_state
    
    -- 进入新状态
    if new_state == GameManager.State.MENU then
        GameManager.load_scene("scenes/menu_scene")
    elseif new_state == GameManager.State.STORY then
        GameManager.load_scene("scenes/menu_scene")
    elseif new_state == GameManager.State.MATCH3 then
        GameManager.load_scene("scenes/match3_scene")
    elseif new_state == GameManager.State.VICTORY then
        print("[MirrorGame] Victory!")
        GameManager.load_scene("scenes/menu_scene")
    elseif new_state == GameManager.State.DEFEAT then
        print("[MirrorGame] Defeat!")
        GameManager.load_scene("scenes/menu_scene")
    end
end

-- 加载场景（模拟）
function GameManager.load_scene(scene_script_path)
    if GameManager.current_scene then
        if GameManager.current_scene.Destroy then
            GameManager.current_scene:Destroy()
        end
    end
    
    -- 这里假设场景是一个返回 table 的模块，并且有 Init 方法
    local scene_class = require("script/mirror_game/" .. scene_script_path)
    if scene_class then
        GameManager.current_scene = scene_class.new()
        if GameManager.current_scene.Init then
            GameManager.current_scene:Init()
        end
    else
        print("Error: Failed to load scene: " .. scene_script_path)
    end
end

function GameManager.update()
    if GameManager.current_scene and GameManager.current_scene.Update then
        GameManager.current_scene:Update()
    end
    UIManager.Update()
    
    -- 测试按键切换 UI (按 C 打开角色，按 B 打开背包)
    if Input.GetKeyDown(KeyCode.C) then
        UIManager.TogglePanel("character_panel")
    end
    if Input.GetKeyDown(KeyCode.B) then
        UIManager.TogglePanel("inventory_panel")
    end
    if Input.GetKeyDown(KeyCode.S) then
        SaveSystem.SaveGame(GameManager.player)
    end
end

function GameManager.shutdown()
    if GameManager.current_scene and GameManager.current_scene.Destroy then
        GameManager.current_scene:Destroy()
    end
    GameManager.current_scene = nil
end

return GameManager
