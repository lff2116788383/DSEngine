local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

local Phase1Test = require("phase1_2d_mvp")

function Awake()
    if type(Config.title) == "string" and Config.title ~= "" then
        DSE_SetWindowTitle(Config.title)
    end
    if type(Config.data_path) == "string" and Config.data_path ~= "" then
        DSE_SetDataRoot(Config.data_path)
    end
    Phase1Test.Setup(Config.phase1_2d)
end

function Update(delta_time)
    Phase1Test.Update(delta_time)
end

function exit()
end

function main()
    Awake()
end
