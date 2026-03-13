require("script/rpg_framework/init")
local RPGFramework = require("script/rpg_framework/init")

-- Define a new Scene component
FlareDemoScene = class("FlareDemoScene", Component)

function FlareDemoScene:ctor()
    FlareDemoScene.super.ctor(self)
end

function FlareDemoScene:Awake()
    print("Flare RPG Demo Awake")
    
    -- Load Map
    -- Path to Flare mod map file (adjusted for where user put it)
    -- Assuming user copied 'flare-game-master' to 'data/flare' or we use absolute path for demo
    local map_path = "C:/Users/wenbilin/Desktop/Engine/flare-game-master/mods/alpha_demo/maps/frontier_outpost.txt"
    
    local map_go = RPGFramework.MapLoader.load(map_path)
    if map_go then
        map_go:SetParent(self:game_object())
        print("Map loaded successfully!")
        
        -- Setup Camera
        local camera_go = GameObject.new("MainCamera")
        local cam = camera_go:AddComponent(Camera)
        camera_go:AddComponent(Transform)
        camera_go:GetComponent(Transform):set_position(glm.vec3(0, 0, 10))
        cam:set_clear_color(glm.vec3(0.1, 0.1, 0.1))
        cam:SetOrthographic(10.0, -100, 100) -- Size 10
        
        -- Create Player (Placeholder)
        local player = GameObject.new("Player")
        player:AddComponent(Transform)
        local stats = player:AddComponent(RPGFramework.Components.RPGStats)
        print("Player HP: " .. stats.hp)
    else
        print("Failed to load map.")
    end
end

return FlareDemoScene
