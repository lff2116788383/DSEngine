local BaseScene = require("script/mirror_game/core/base_scene")
require("renderer/camera")
require("renderer/sprite_renderer")
require("renderer/texture_2d")
require("renderer/sprite")
require("control/input")
require("utils/screen")
local MenuScene = setmetatable({}, {__index = BaseScene})
MenuScene.__index = MenuScene

local function LoadTextureFromCandidates(paths)
    for _, path in ipairs(paths) do
        local tex = Texture2D.LoadFromFile(path)
        if tex then
            return tex
        end
    end
    return nil
end

function MenuScene.new()
    local self = BaseScene.new()
    setmetatable(self, MenuScene)
    self.started = false
    return self
end

function MenuScene:Init()
    print("MenuScene:Init()")
    self:CreateCamera()
    self:CreateBackground()
    print("MenuScene initialized. Click to start battle.")
end

function MenuScene:CreateCamera()
    local go = GameObject.new("MenuCamera")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 10))

    local camera = go:AddComponent(Camera)
    local aspect = Screen.width() / Screen.height()
    local height = 5
    local width = height * aspect

    camera:SetOrthographic(-width, width, -height, height, -10, 100)
    camera:set_depth(10)
    camera:set_clear_color(0.1, 0.1, 0.1, 1.0)

    self:AddGameObject(go)
end

function MenuScene:CreateBackground()
    local go = GameObject.new("MenuBackground")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 0))
    transform:set_scale(glm.vec3(6, 6, 1))

    local renderer = go:AddComponent(SpriteRenderer)
    local bg_texture = LoadTextureFromCandidates({
        "mirror_assets/Resources/bg00_1.png",
        "../bin/data/mirror_assets/Resources/bg00_1.png"
    })
    if bg_texture then
        local sprite = Sprite.Create(bg_texture)
        sprite:set_ppu(100)
        renderer:set_sprite(sprite)
        renderer:set_sorting_layer(0)
        renderer:set_order_in_layer(-10)
    end

    self:AddGameObject(go)
end

function MenuScene:Update()
    if self.started then
        return
    end
    if Input.GetMouseButtonDown(0) then
        self.started = true
        local GameManager = require("script/mirror_game/core/game_manager")
        GameManager.change_state(GameManager.State.MATCH3)
    end
end

return MenuScene
