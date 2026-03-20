local BaseScene = {}
BaseScene.__index = BaseScene

function BaseScene.new()
    local self = setmetatable({}, BaseScene)
    self.root_game_objects = {} -- 管理场景内的所有根游戏对象
    return self
end

function BaseScene:Init()
    print("BaseScene:Init()")
end

function BaseScene:Destroy()
    print("BaseScene:Destroy()")
    for _, go in ipairs(self.root_game_objects) do
        -- 假设 GameObject 有 Destroy 方法
        if go.Destroy then
            go:Destroy()
        end
    end
    self.root_game_objects = {}
end

function BaseScene:AddGameObject(go)
    table.insert(self.root_game_objects, go)
end

return BaseScene