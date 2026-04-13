local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

local RuntimeEntry = nil
local runtime_config = nil
local game_entry = type(Config.game_entry) == "string" and Config.game_entry or "phase2_3d_mvp"
if game_entry == "frog_jump" then
    RuntimeEntry = require("frog_jump")
    runtime_config = Config.frog_jump or {}
elseif game_entry == "phase1_2d_mvp" then
    RuntimeEntry = require("phase1_2d_mvp")
    runtime_config = Config.phase1_2d or {}
elseif game_entry == "phase1_2d_showcase" then
    RuntimeEntry = require("phase1_2d_showcase")
    runtime_config = Config.phase1_2d_showcase or {}
elseif game_entry == "vse_demo_15_7" then
    RuntimeEntry = require("vse_demo.demo15_7")
    runtime_config = Config.vse_demo_15_7 or {}
elseif game_entry == "vse_demo_15_8" then
    RuntimeEntry = require("vse_demo.demo15_8")
    runtime_config = Config.vse_demo_15_8 or {}
elseif game_entry == "vse_demo_15_9" then
    RuntimeEntry = require("vse_demo.demo15_9")
    runtime_config = Config.vse_demo_15_9 or {}
else

    RuntimeEntry = require("phase2_3d_mvp")
    runtime_config = Config.phase2_3d or {}
end


function Awake()
    if type(Config.title) == "string" and Config.title ~= "" then
        dse.app.set_window_title(Config.title)
    end
    if type(Config.data_path) == "string" and Config.data_path ~= "" then
        dse.app.set_data_root(Config.data_path)
    end
    RuntimeEntry.Setup(runtime_config or {})
end


function Update(delta_time)
    RuntimeEntry.Update(delta_time)
end

function exit()
end

function main()
    Awake()
end
