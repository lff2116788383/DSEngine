# DSEngine 使用手册

## 快速开始

### 1. 场景创建
在 `examples/` 目录下创建新的 Lua 脚本，例如 `my_scene.lua`。

```lua
MyScene = class("MyScene", Component)

function MyScene:ctor()
    MyScene.super.ctor(self)
end

function MyScene:Awake()
    print("MyScene Awake")
    MyScene.super.Awake(self)
    
    -- 创建相机
    self:CreateCamera()
    -- 创建物体
    self:CreateObject()
end

function MyScene:CreateCamera()
    local go = GameObject.new("MainCamera")
    local transform = go:AddComponent(Transform)
    transform:set_position(glm.vec3(0, 0, 10))
    go:AddComponent(Camera)
end

function MyScene:CreateObject()
    local go = GameObject.new("Player")
    local transform = go:AddComponent(Transform)
    local sprite_renderer = go:AddComponent(SpriteRenderer)
    
    local texture = Texture2D.LoadFromFile("images/plane_albedo.cpt")
    if texture then
        local sprite = Sprite.Create(texture)
        sprite_renderer:set_sprite(sprite)
    end
end
```

### 2. 注册场景
在 `examples/main.lua` 中引用并添加你的场景组件：

```lua
require("my_scene")

function main()
    Debug.ConnectDebugServer()

    local go = GameObject.new("SceneRoot")
    go:AddComponent(MyScene)
end
```

## 核心概念

### GameObject (游戏对象)
所有场景中的实体都是 `GameObject`。它是一个容器，可以挂载多个 `Component`。
`GameObject` 具有层级关系，可以有父节点和子节点。

### Component (组件)
组件定义了游戏对象的行为。常见的内置组件包括：
*   `Transform`: 变换组件（位置、旋转、缩放），每个 GameObject 必有。
*   `SpriteRenderer`: 2D 精灵渲染器。
*   `MeshRenderer`: 3D 网格渲染器。
*   `Camera`: 相机组件。
*   `BoxCollider` / `SphereCollider`: 物理碰撞体。
*   `RigidDynamic`: 动态刚体。

### Lua API
引擎的大部分功能都已绑定到 Lua。
*   `glm.vec3(x, y, z)`: 向量操作。
*   `GameObject.new(name)`: 创建对象。
*   `go:AddComponent(Type)`: 添加组件。
*   `Input.GetKey(KeyCode)`: 获取输入。

## 资源管理
资源文件放在 `data/` 目录下。加载资源时，使用相对于 `data/` 的路径。
例如：`Texture2D.LoadFromFile("images/plane_albedo.cpt")`。
