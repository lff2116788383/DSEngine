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

-- ============================================================
-- Demo 注册表：约定式自动发现
-- 每个 demo 模块通过 _meta 表自描述：
--   Module._meta = { name = "...", category = "...", config = {...} }
-- 若 _meta 不存在，按旧约定从 Config 表查找配置。
-- ============================================================

-- 3D 基础图元（module 路径不带 3d_ 前缀）
local primitives_3d = {triangle = true, square = true, cube = true}

-- 顶层 demo（非 3d/ 子目录）
local toplevel_demos = {
    phase1_2d_showcase         = {module = "phase1_2d_showcase",        cfg = "phase1_2d_showcase"},
    phase1_2d_physics_showcase = {module = "phase1_2d_physics_showcase", cfg = "phase1_2d_physics_showcase"},
}

-- resolve_demo: 根据 game_entry 字符串加载模块并返回 (module, config)
local function resolve_demo(entry)
    -- 1) 顶层 demo
    local top = toplevel_demos[entry]
    if top then
        local mod = require(top.module)
        local cfg = (mod._meta and mod._meta.config) or Config[top.cfg] or {}
        return mod, cfg
    end

    -- 2) 3d_ 前缀
    if entry:sub(1, 3) == "3d_" then
        local suffix = entry:sub(4)
        local module_path
        if primitives_3d[suffix] then
            module_path = "3d." .. suffix
        else
            module_path = "3d." .. entry
        end
        local ok, mod = pcall(require, module_path)
        if ok and type(mod) == "table" then
            -- 优先使用模块内嵌 _meta.config，其次 Config 表旧约定
            local cfg
            if mod._meta and mod._meta.config then
                cfg = mod._meta.config
            else
                cfg = Config["demo_" .. entry] or Config.basic_3d or {}
            end
            return mod, cfg
        end
        print("[main] require('" .. module_path .. "') failed: " .. tostring(mod))
    end

    -- 3) fallback
    print("[main] 未知 demo: " .. entry .. ", fallback to phase1_2d_physics_showcase")
    local mod = require("phase1_2d_physics_showcase")
    local cfg = (mod._meta and mod._meta.config) or Config.phase1_2d_physics_showcase or {}
    return mod, cfg
end

local RuntimeEntry, runtime_config = resolve_demo(game_entry)

print("[main] 加载 demo: " .. game_entry)
if RuntimeEntry._meta and RuntimeEntry._meta.name then
    print("[main] name: " .. RuntimeEntry._meta.name)
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
