require("utils/debug")
require("utils/application")
require("component/game_object")
require("component/transform")
-- require("login_scene")
-- require("test_2d_scene")
require("flare_rpg_demo")

function main()
    Debug.ConnectDebugServer()

    local go=GameObject.new("FlareDemoRoot")
    local transform=go:AddComponent(Transform)
    print("transform:" .. tostring(transform))
    local pos=transform:position()
    print("pos:" .. tostring(pos))

    go:AddComponent(FlareDemoScene)
end

function exit()
    print("exit")
end
