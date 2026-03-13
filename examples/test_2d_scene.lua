require("lua_extension")
require("lua_extension_math")
require("renderer/camera")
require("renderer/sprite")
require("renderer/sprite_renderer")
require("renderer/animation_2d")
require("renderer/animation_clip_2d")
require("renderer/texture_2d")
require("renderer/grid")
require("renderer/tilemap")
require("renderer/tilemap_renderer")
require("physics/rigid_dynamic")
require("physics/box_collider")
require("component/transform")

Test2DScene = class("Test2DScene", Component)

function Test2DScene:ctor()
    Test2DScene.super.ctor(self)
end

function Test2DScene:Awake()
    print("Test2DScene Awake")
    Test2DScene.super.Awake(self)

    self:CreateCamera()
    self:CreateBackground()
    self:CreateCharacter()
    self:CreatePhysicsBox()
    self:CreateTilemapDemo()
end

function Test2DScene:CreateTilemapDemo()
    local go = GameObject.new("Tilemap")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 0))
    
    local grid = go:AddComponent(Grid)
    grid:set_cell_size(glm.vec2(1, 1))
    
    local tilemap = go:AddComponent(Tilemap)
    local tilemap_renderer = go:AddComponent(TilemapRenderer)
    tilemap_renderer:set_sorting_layer(0)
    tilemap_renderer:set_order_in_layer(-5) -- Behind character, in front of background
    
    local texture = Texture2D.LoadFromFile("images/plane_albedo.cpt")
    if texture then
        local sprite = Sprite.Create(texture)
        sprite:set_ppu(100)
        
        -- Create a simple floor pattern
        for x = -5, 5 do
            tilemap:SetTile(glm.vec2(x, -2), sprite)
        end
        
        -- Create some floating platforms
        tilemap:SetTile(glm.vec2(-2, 0), sprite)
        tilemap:SetTile(glm.vec2(2, 1), sprite)
    end
end

function Test2DScene:CreateCamera()
    local go = GameObject.new("MainCamera")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 10))
    
    local camera = go:AddComponent(Camera)
    -- camera:set_orthographic(true) -- Assuming this method exists or similar
    -- If not, we might need to rely on default or check Camera Lua binding
    camera:set_depth(0)
    camera:set_culling_mask(0x01)
end

function Test2DScene:CreateBackground()
    local go = GameObject.new("Background")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 0))
    
    local sprite_renderer = go:AddComponent(SpriteRenderer)
    -- Using a known existing texture from CMakeLists.txt copy list
    local texture = Texture2D.LoadFromFile("images/plane_albedo.cpt") 
    if texture then
        local sprite = Sprite.Create(texture)
        sprite:set_ppu(100)
        sprite_renderer:set_sprite(sprite)
        sprite_renderer:set_sorting_layer(0)
        sprite_renderer:set_order_in_layer(-10)
    else
        print("Failed to load texture: images/plane_albedo.cpt")
    end
end

function Test2DScene:CreateCharacter()
    local go = GameObject.new("Character")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(-2, 0, 0))
    
    local sprite_renderer = go:AddComponent(SpriteRenderer)
    local texture = Texture2D.LoadFromFile("images/plane_albedo.cpt")
    if texture then
        local sprite = Sprite.Create(texture)
        sprite:set_ppu(100)
        sprite_renderer:set_sprite(sprite)
        sprite_renderer:set_sorting_layer(0)
        sprite_renderer:set_order_in_layer(0)
        
        -- Animation Test
        local anim = go:AddComponent(Animation2D)
        local clip = AnimationClip2D.new()
        clip:set_name("Idle")
        clip:set_is_looping(true)
        clip:AddFrame(sprite, 0.5)
        clip:AddFrame(sprite, 0.5) 
        anim:set_clip(clip)
        anim:Play()
    end
end

function Test2DScene:CreatePhysicsBox()
    local go = GameObject.new("PhysicsBox")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(2, 5, 0))
    
    local sprite_renderer = go:AddComponent(SpriteRenderer)
    local texture = Texture2D.LoadFromFile("images/plane_albedo.cpt")
    if texture then
        local sprite = Sprite.Create(texture)
        sprite:set_ppu(100)
        sprite_renderer:set_sprite(sprite)
    end
    
    local rigid_body = go:AddComponent(RigidDynamic)
    rigid_body:set_2d_mode(true)
    
    local collider = go:AddComponent(BoxCollider)
    -- collider:set_size(glm.vec3(1, 1, 1)) -- Verify BoxCollider API
end
