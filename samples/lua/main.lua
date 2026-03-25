local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

local RuntimeEntry = nil
local game_entry = type(Config.game_entry) == "string" and Config.game_entry or "phase1_2d_mvp"
if game_entry == "frog_jump" then
    RuntimeEntry = require("frog_jump")
else
    RuntimeEntry = require("phase1_2d_mvp")
end

function Awake()
    if type(Config.title) == "string" and Config.title ~= "" then
        dse.app.set_window_title(Config.title)
    end
    if type(Config.data_path) == "string" and Config.data_path ~= "" then
        dse.app.set_data_root(Config.data_path)
    end
    if game_entry == "frog_jump" then
        RuntimeEntry.Setup(Config.frog_jump or {})
    else
        RuntimeEntry.Setup(Config.phase1_2d)
    end
end

function Update(delta_time)
    RuntimeEntry.Update(delta_time)
end

function exit()
end

function main()
    Awake()
end
