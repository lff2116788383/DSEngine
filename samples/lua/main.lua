local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

local RuntimeEntry = nil
local game_entry = type(Config.game_entry) == "string" and Config.game_entry or "phase2_3d_mvp"
if game_entry == "frog_jump" then
    RuntimeEntry = require("frog_jump")
elseif game_entry == "phase1_2d_mvp" then
    RuntimeEntry = require("phase1_2d_mvp")
elseif game_entry == "phase1_2d_showcase" then
    RuntimeEntry = require("phase1_2d_showcase")
else
    RuntimeEntry = require("phase2_3d_mvp")
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
    elseif game_entry == "phase1_2d_mvp" then
        RuntimeEntry.Setup(Config.phase1_2d or {})
    elseif game_entry == "phase1_2d_showcase" then
        RuntimeEntry.Setup(Config.phase1_2d_showcase or {})
    else
        RuntimeEntry.Setup(Config.phase2_3d or {})
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
