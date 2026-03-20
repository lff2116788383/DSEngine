-- 游戏入口脚本

-- 加载核心组件依赖
require("script/component/game_object")
require("script/component/transform")
-- require("script/ui/ui_text") -- 如果有 UI 绑定

-- 加载游戏管理器
local GameManager = require("script/mirror_game/core/game_manager")

local MirrorGame = {}

function MirrorGame.Awake()
    print("MirrorGame Awake")
    GameManager.init()
end

function MirrorGame.Update()
    GameManager.update()
end

function MirrorGame.exit()
    GameManager.shutdown()
end

-- 暴露全局函数供 C++ 调用
_G.Awake = MirrorGame.Awake
_G.Update = MirrorGame.Update
_G.exit = MirrorGame.exit

return MirrorGame
