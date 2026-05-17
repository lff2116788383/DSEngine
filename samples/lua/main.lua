local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

-- ============================================================
-- Demo 解析：优先级 DSE_DEMO 环境变量 > config.game_entry > 默认
-- 用法: DSEngine_Game_release.exe --demo=3d_fracture
--       或 set DSE_DEMO=3d_fracture
--       或修改 config.lua 的 Config.game_entry
-- ============================================================

local env_demo = os.getenv("DSE_DEMO")
local game_entry = (env_demo and env_demo ~= "") and env_demo
    or (type(Config.game_entry) == "string" and Config.game_entry)
    or "phase1_2d_physics_showcase"

-- 3D 基础图元：module 名不带 "3d_" 前缀
local primitives_3d = {triangle = true, square = true, cube = true}

-- 顶层 demo（非 3d/ 子目录，module path 和 config key 不规则）
local toplevel_demos = {
    phase1_2d_showcase         = {module = "phase1_2d_showcase",        cfg = "phase1_2d_showcase"},
    phase1_2d_physics_showcase = {module = "phase1_2d_physics_showcase", cfg = "phase1_2d_physics_showcase"},
}

-- 约定式解析
local RuntimeEntry = nil
local runtime_config = nil

local top = toplevel_demos[game_entry]
if top then
    RuntimeEntry = require(top.module)
    runtime_config = Config[top.cfg] or {}
elseif game_entry:sub(1, 3) == "3d_" then
    local suffix = game_entry:sub(4) -- "fracture", "triangle", etc.
    if primitives_3d[suffix] then
        RuntimeEntry = require("3d." .. suffix)
        runtime_config = Config.basic_3d or {}
    else
        RuntimeEntry = require("3d." .. game_entry)
        runtime_config = Config["demo_" .. game_entry] or Config.basic_3d or {}
    end
else
    print("[main] 未知 demo: " .. game_entry .. ", fallback to phase1_2d_physics_showcase")
    RuntimeEntry = require("phase1_2d_physics_showcase")
    runtime_config = Config.phase1_2d_physics_showcase or {}
end

print("[main] 加载 demo: " .. game_entry)

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
