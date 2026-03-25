local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

local Runtime2DTest = require("phase1_2d_mvp")

function Awake()
    if type(Config.title) == "string" and Config.title ~= "" then
        dse.app.set_window_title(Config.title)
    end
    if type(Config.data_path) == "string" and Config.data_path ~= "" then
        dse.app.set_data_root(Config.data_path)
    end
    Runtime2DTest.Setup(Config.phase1_2d)
end

function Update(delta_time)
    Runtime2DTest.Update(delta_time)
end

function exit()
end

function main()
    Awake()
end
