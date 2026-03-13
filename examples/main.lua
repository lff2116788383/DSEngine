require("utils/debug")
require("utils/application")
require("component/game_object")
require("component/transform")
-- require("login_scene")
require("test_2d_scene")

function main()
    Debug.ConnectDebugServer()

    local go=GameObject.new("Test2DSceneGo")
    local transform=go:AddComponent(Transform)
    print("transform:" .. tostring(transform))
    local pos=transform:position()
    print("pos:" .. tostring(pos))

    go:AddComponent(Test2DScene)
end

function exit()
    print("exit")
end
