/**
 * @file project_scaffold.cpp
 * @brief 项目模板生成实现（纯 std，不依赖编辑器/引擎运行时）。
 */

#include "engine/project/project_scaffold.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace dse::project {
namespace {

namespace fs = std::filesystem;

bool HasLuaScripting(ProjectTemplate tmpl) {
    return tmpl == ProjectTemplate::Game2D
        || tmpl == ProjectTemplate::Game3D
        || tmpl == ProjectTemplate::Lua
        || tmpl == ProjectTemplate::Platformer2D
        || tmpl == ProjectTemplate::TopDownRPG
        || tmpl == ProjectTemplate::ThirdPerson3D;
}

// 是否使用 3D 场景预置（相机 + 平行光）。
bool Uses3DScene(ProjectTemplate tmpl) {
    return tmpl == ProjectTemplate::Game3D
        || tmpl == ProjectTemplate::ThirdPerson3D;
}

bool HasCppHost(ProjectTemplate tmpl) {
    return tmpl == ProjectTemplate::Cpp;
}

bool HasCSharpScripting(ProjectTemplate tmpl) {
    return tmpl == ProjectTemplate::CSharp;
}

std::string JsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;     break;
        }
    }
    return out;
}

bool WriteTextFile(const fs::path& path, const std::string& content, std::string& error) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        error = "无法写入文件: " + path.string();
        return false;
    }
    ofs << content;
    if (!ofs.good()) {
        error = "写入文件失败: " + path.string();
        return false;
    }
    return true;
}

std::string BuildProjectDescriptor(const std::string& name,
                                   ProjectTemplate tmpl,
                                   const std::string& engine_version) {
    const bool lua = HasLuaScripting(tmpl);
    const bool cpp = HasCppHost(tmpl);
    const bool csharp = HasCSharpScripting(tmpl);
    const char* features = lua ? "\"lua_scripting\"" : (cpp ? "\"cpp_business\"" : (csharp ? "\"csharp_scripting\"" : ""));
    std::ostringstream ss;
    ss << "{\n";
    ss << "    \"format_version\": 1,\n";
    ss << "    \"name\": \"" << JsonEscape(name) << "\",\n";
    ss << "    \"version\": \"1.0.0\",\n";
    ss << "    \"engine_version\": \"" << JsonEscape(engine_version) << "\",\n";
    ss << "    \"description\": \"\",\n";
    ss << "    \"features\": [" << features << "],\n";
    ss << "    \"entry_script\": \"" << (lua ? "scripts/main.lua" : "") << "\",\n";
    ss << "    \"default_scene\": \"scenes/main.json\",\n";
    ss << "    \"asset_dir\": \"assets/\",\n";
    ss << "    \"scene_dir\": \"scenes/\",\n";
    ss << "    \"script_dir\": \"scripts/\",\n";
    ss << "    \"build\": {\n";
    ss << "        \"output_dir\": \"build/\",\n";
    ss << "        \"target\": \"standalone\"\n";
    ss << "    },\n";
    ss << "    \"window\": {\n";
    ss << "        \"title\": \"" << JsonEscape(name) << "\",\n";
    ss << "        \"width\": 1280,\n";
    ss << "        \"height\": 720\n";
    ss << "    },\n";
    ss << "    \"splash\": {\n";
    ss << "        \"enabled\": true,\n";
    ss << "        \"image\": \"\",\n";
    ss << "        \"app_name\": \"" << JsonEscape(name) << "\",\n";
    ss << "        \"initial_status\": \"\",\n";
    ss << "        \"background_argb\": \"0xFF1E1E28\",\n";
    ss << "        \"accent_argb\": \"0xFF4A9EFF\",\n";
    ss << "        \"fade_in_ms\": 600,\n";
    ss << "        \"min_display_ms\": 900,\n";
    ss << "        \"fade_out_ms\": 500\n";
    ss << "    }\n";
    ss << "}\n";
    return ss.str();
}

std::string BuildSceneJson(ProjectTemplate tmpl) {
    if (Uses3DScene(tmpl)) {
        std::ostringstream ss;
        ss << "[\n"
           << "    {\n"
           << "        \"id\": 1,\n"
           << "        \"name\": \"Main Camera\",\n"
           << "        \"transform\": {\n"
           << "            \"position\": [0.0, 4.0, 10.0],\n"
           << "            \"rotation\": [0.0, 0.0, 0.0, 1.0],\n"
           << "            \"scale\": [1.0, 1.0, 1.0],\n"
           << "            \"dirty\": false\n"
           << "        },\n"
           << "        \"camera3d\": {\n"
           << "            \"enabled\": true,\n"
           << "            \"priority\": 0,\n"
           << "            \"fov\": 60.0,\n"
           << "            \"near_clip\": 0.1,\n"
           << "            \"far_clip\": 500.0\n"
           << "        }\n"
           << "    },\n"
           << "    {\n"
           << "        \"id\": 2,\n"
           << "        \"name\": \"Directional Light\",\n"
           << "        \"transform\": {\n"
           << "            \"position\": [0.0, 5.0, 0.0],\n"
           << "            \"rotation\": [0.0, 0.0, 0.0, 1.0],\n"
           << "            \"scale\": [1.0, 1.0, 1.0],\n"
           << "            \"dirty\": false\n"
           << "        },\n"
           << "        \"directional_light3d\": {\n"
           << "            \"enabled\": true,\n"
           << "            \"color\": [1.0, 0.95, 0.85],\n"
           << "            \"intensity\": 1.5,\n"
           << "            \"cast_shadow\": false,\n"
           << "            \"shadow_strength\": 1.0\n"
           << "        }\n"
           << "    }\n"
           << "]\n";
        return ss.str();
    }
    // Empty / Game2D / Lua: 空场景（正确的数组格式）
    return "[]\n";
}

// 把模板中的 __NAME__ 占位符替换为工程名（仅出现在 print 文案里，便于用 raw 字符串编写）。
std::string SubstName(std::string body, const std::string& name) {
    const std::string token = "__NAME__";
    for (size_t p = body.find(token); p != std::string::npos; p = body.find(token, p)) {
        body.replace(p, token.size(), name);
        p += name.size();
    }
    return body;
}

// 品类模板：2D 平台跳跃（重力 + 跳跃 + 平台 AABB 碰撞 + 相机跟随）。
std::string BuildPlatformerLua(const std::string& name) {
    static const char* kBody = R"LUA(-- __NAME__ — DSEngine 2D platformer template (gravity + jump + AABB platforms)
-- Lifecycle: Awake() once at startup; Update(dt) every frame. Physics is hand-rolled in Lua.
local app = dse.app
local KEY_A, KEY_D, KEY_LEFT, KEY_RIGHT, KEY_SPACE = 65, 68, 263, 262, 32

-- Player state (center position + velocity; PW/PH are half-extents)
local player
local px, py = 0.0, 2.0
local vx, vy = 0.0, 0.0
local on_ground = false
local PW, PH = 0.5, 0.5
local MOVE, JUMP, GRAVITY = 6.0, 13.0, -30.0

-- Static platforms: center x,y + half-extents hw,hh
local platforms = {
  { x = 0.0,  y = -0.5, hw = 9.0, hh = 0.5 },   -- ground
  { x = -5.0, y = 1.5,  hw = 1.6, hh = 0.25 },
  { x = 0.0,  y = 2.8,  hw = 1.6, hh = 0.25 },
  { x = 5.0,  y = 4.1,  hw = 1.4, hh = 0.25 },
}

local function spawn_quad(x, y, sx, sy, r, g, b, order)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
  dse.ecs.add_sprite(e, r, g, b, 1.0, order or 0)
  return e
end

function Awake()
  player = spawn_quad(px, py, PW * 2.0, PH * 2.0, 0.30, 0.80, 1.0, 1)

  -- 2D orthographic camera that follows the player
  local camera = dse.ecs.create_entity()
  dse.ecs.add_transform(camera, 0.0, 2.0, 0.0, 1, 1, 1)
  dse.ecs.add_camera(camera, 6.0)
  dse.ecs.set_camera_follow(camera, player, 0.12)

  for _, p in ipairs(platforms) do
    spawn_quad(p.x, p.y, p.hw * 2.0, p.hh * 2.0, 0.45, 0.50, 0.60, 0)
  end
  print("[__NAME__] platformer ready -- A/D move, Space to jump")
end

local function overlaps(ax, ay, ahw, ahh, b)
  return math.abs(ax - b.x) < (ahw + b.hw) and math.abs(ay - b.y) < (ahh + b.hh)
end

function Update(dt)
  dt = dt or 0.0
  if dt > 0.05 then dt = 0.05 end   -- clamp for stable collision

  vx = 0.0
  if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then vx = vx - MOVE end
  if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then vx = vx + MOVE end
  if on_ground and app.get_key_down(KEY_SPACE) then vy, on_ground = JUMP, false end
  vy = vy + GRAVITY * dt

  -- Integrate + resolve X
  px = px + vx * dt
  for _, p in ipairs(platforms) do
    if overlaps(px, py, PW, PH, p) then
      if px < p.x then px = p.x - p.hw - PW else px = p.x + p.hw + PW end
    end
  end

  -- Integrate + resolve Y (landing detection)
  py = py + vy * dt
  on_ground = false
  for _, p in ipairs(platforms) do
    if overlaps(px, py, PW, PH, p) then
      if vy <= 0.0 then py, on_ground = p.y + p.hh + PH, true
      else py = p.y - p.hh - PH end
      vy = 0.0
    end
  end

  if py < -20.0 then px, py, vx, vy = 0.0, 2.0, 0.0, 0.0 end  -- fell off -> respawn
  dse.ecs.set_transform_position(player, px, py, 0.0)
end
)LUA";
    return SubstName(kBody, name);
}

// 品类模板：俯视 RPG（8 向移动 + 障碍 AABB 碰撞 + 可拾取金币 + 相机跟随）。
std::string BuildTopDownLua(const std::string& name) {
    static const char* kBody = R"LUA(-- __NAME__ — DSEngine top-down RPG template (8-way move, obstacles, pickups)
local app = dse.app
local KEY_A, KEY_D, KEY_W, KEY_S = 65, 68, 87, 83
local KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN = 263, 262, 265, 264

local player
local px, py = 0.0, 0.0
local PW, PH = 0.4, 0.4
local SPEED = 5.0

-- Obstacles block movement; coins are collectible pickups.
local obstacles = {
  { x = -3.0, y = 2.0,  hw = 0.6, hh = 0.6 },
  { x =  3.0, y = -1.0, hw = 0.6, hh = 0.6 },
  { x =  1.5, y = 3.0,  hw = 0.6, hh = 0.6 },
}
local coins = {
  { x = -4.0, y = -3.0 }, { x = 4.0, y = 3.5 }, { x = 0.0, y = 4.0 },
  { x = -2.0, y = -4.0 }, { x = 5.0, y = 0.0 },
}
local score = 0

local function spawn_quad(x, y, sx, sy, r, g, b, order)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
  dse.ecs.add_sprite(e, r, g, b, 1.0, order or 0)
  return e
end

function Awake()
  player = spawn_quad(px, py, PW * 2.0, PH * 2.0, 0.30, 0.80, 1.0, 2)

  local camera = dse.ecs.create_entity()
  dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1, 1, 1)
  dse.ecs.add_camera(camera, 6.0)
  dse.ecs.set_camera_follow(camera, player, 0.15)

  for _, o in ipairs(obstacles) do
    spawn_quad(o.x, o.y, o.hw * 2.0, o.hh * 2.0, 0.55, 0.40, 0.35, 0)
  end
  for _, c in ipairs(coins) do
    c.e = spawn_quad(c.x, c.y, 0.4, 0.4, 1.0, 0.85, 0.20, 1)
    c.taken = false
  end
  print("[__NAME__] top-down ready -- WASD/arrows to move, collect the gold coins")
end

local function overlaps(ax, ay, ahw, ahh, bx, by, bhw, bhh)
  return math.abs(ax - bx) < (ahw + bhw) and math.abs(ay - by) < (ahh + bhh)
end

function Update(dt)
  dt = dt or 0.0
  local dx, dy = 0.0, 0.0
  if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1.0 end
  if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1.0 end
  if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dy = dy + 1.0 end
  if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dy = dy - 1.0 end
  if dx ~= 0.0 and dy ~= 0.0 then dx, dy = dx * 0.70710678, dy * 0.70710678 end

  -- Move X then resolve against obstacles
  px = px + dx * SPEED * dt
  for _, o in ipairs(obstacles) do
    if overlaps(px, py, PW, PH, o.x, o.y, o.hw, o.hh) then
      if px < o.x then px = o.x - o.hw - PW else px = o.x + o.hw + PW end
    end
  end
  -- Move Y then resolve
  py = py + dy * SPEED * dt
  for _, o in ipairs(obstacles) do
    if overlaps(px, py, PW, PH, o.x, o.y, o.hw, o.hh) then
      if py < o.y then py = o.y - o.hh - PH else py = o.y + o.hh + PH end
    end
  end
  dse.ecs.set_transform_position(player, px, py, 0.0)

  -- Pickups
  for _, c in ipairs(coins) do
    if not c.taken and overlaps(px, py, PW, PH, c.x, c.y, 0.2, 0.2) then
      c.taken = true
      score = score + 1
      dse.ecs.set_transform_position(c.e, 9999.0, 9999.0, 0.0)  -- hide off-screen
      print(string.format("[__NAME__] coin %d/%d", score, #coins))
      if score == #coins then print("[__NAME__] all coins collected!") end
    end
  end
end
)LUA";
    return SubstName(kBody, name);
}

// 品类模板：3D 第三人称（地面 + 角色方块 + 固定偏移跟随相机）。
std::string BuildThirdPersonLua(const std::string& name) {
    static const char* kBody = R"LUA(-- __NAME__ — DSEngine 3D third-person template (follow camera + WASD movement)
-- Scene main.json pre-populates a Camera + Directional Light. We drive the camera
-- ourselves (no free controller): keep its scene orientation, translate to follow.
local app = dse.app
local KEY_A, KEY_D, KEY_W, KEY_S = 65, 68, 87, 83
local KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN = 263, 262, 265, 264
local atan2 = math.atan2 or math.atan   -- Lua 5.1 has math.atan2; 5.3+ folds it into math.atan

local state = { camera = nil, player = nil, px = 0.0, pz = 0.0, yaw = 0.0 }
local SPEED = 6.0
local CAM_BACK, CAM_UP = 9.0, 5.0   -- camera offset behind/above the player

local function cube_verts()
  return { -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
           -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5 }
end
local function cube_idx()
  return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

function Awake()
  -- Grab the scene camera (no free controller attached: we position it each frame).
  local cams = dse.ecs.find_entities_with_camera3d and dse.ecs.find_entities_with_camera3d() or {}
  if #cams > 0 then state.camera = cams[1] end

  local ground = dse.ecs.create_entity()
  dse.ecs.add_transform(ground, 0, -0.06, 0, 20, 0.12, 20)
  dse.ecs.add_mesh_renderer(ground, 0.32, 0.36, 0.33, 1.0, cube_verts(), cube_idx())
  dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
  dse.ecs.set_mesh_material(ground, 0.0, 0.85, 1.0, 0, 0, 0, 1.0, true, true)

  state.player = dse.ecs.create_entity()
  dse.ecs.add_transform(state.player, 0, 0.5, 0, 1, 1, 1)
  dse.ecs.add_mesh_renderer(state.player, 0.20, 0.70, 1.0, 1.0, cube_verts(), cube_idx())
  dse.ecs.set_mesh_shader_variant(state.player, "MESH_LIT")
  dse.ecs.set_mesh_material(state.player, 0.1, 0.5, 1.0, 0, 0, 0, 1.0, true, false)

  print("[__NAME__] third-person ready -- WASD to move; camera follows from behind")
end

function Update(dt)
  dt = dt or 0.0
  local dx, dz = 0.0, 0.0
  if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1.0 end
  if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1.0 end
  if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dz = dz - 1.0 end   -- forward = -Z
  if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dz = dz + 1.0 end
  if dx ~= 0.0 and dz ~= 0.0 then dx, dz = dx * 0.70710678, dz * 0.70710678 end

  state.px = state.px + dx * SPEED * dt
  state.pz = state.pz + dz * SPEED * dt

  if state.player then
    dse.ecs.set_transform_position(state.player, state.px, 0.5, state.pz)
    if dx ~= 0.0 or dz ~= 0.0 then
      state.yaw = math.deg(atan2(dx, -dz))   -- face movement direction
      dse.ecs.set_transform_rotation(state.player, 0.0, state.yaw, 0.0)
    end
  end

  -- Fixed-offset third-person follow (keeps the scene camera's -Z orientation).
  if state.camera then
    dse.ecs.set_transform_position(state.camera, state.px, 0.5 + CAM_UP, state.pz + CAM_BACK)
  end
end
)LUA";
    return SubstName(kBody, name);
}

std::string BuildMainLua(const std::string& name, ProjectTemplate tmpl) {
    if (tmpl == ProjectTemplate::Platformer2D)  return BuildPlatformerLua(name);
    if (tmpl == ProjectTemplate::TopDownRPG)    return BuildTopDownLua(name);
    if (tmpl == ProjectTemplate::ThirdPerson3D) return BuildThirdPersonLua(name);
    std::ostringstream ss;
    if (tmpl == ProjectTemplate::Game2D) {
        ss << "-- " << name << " \xE2\x80\x94 DSEngine 2D entry script\n"
           << "-- Lifecycle: Awake() runs once at startup; Update(dt) runs every frame.\n"
           << "-- Input: dse.app.get_key(code) uses GLFW key codes (A=65 D=68 W=87 S=83; arrows 262-265).\n\n"
           << "local app = dse.app\n"
           << "local KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP = 263, 262, 264, 265\n"
           << "local KEY_A, KEY_D, KEY_S, KEY_W = 65, 68, 83, 87\n\n"
           << "local player\n"
           << "local pos = { x = 0.0, y = 0.0 }\n"
           << "local speed = 5.0\n\n"
           << "function Awake()\n"
           << "    -- Orthographic 2D camera (vertical half-size = 5 world units)\n"
           << "    local camera = dse.ecs.create_entity()\n"
           << "    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)\n"
           << "    dse.ecs.add_camera(camera, 5.0)\n\n"
           << "    -- Player: a solid-color quad (texture handle 0 => white, tinted by color)\n"
           << "    player = dse.ecs.create_entity()\n"
           << "    dse.ecs.add_transform(player, pos.x, pos.y, 0.0, 1.0, 1.0, 1.0)\n"
           << "    dse.ecs.add_sprite(player, 0.30, 0.80, 1.0, 1.0, 0)\n\n"
           << "    print(\"[" << name << "] ready -- move the square with WASD or arrow keys\")\n"
           << "end\n\n"
           << "function Update(dt)\n"
           << "    dt = dt or 0.0\n"
           << "    local dx, dy = 0.0, 0.0\n"
           << "    if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1.0 end\n"
           << "    if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1.0 end\n"
           << "    if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dy = dy + 1.0 end\n"
           << "    if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dy = dy - 1.0 end\n"
           << "    pos.x = pos.x + dx * speed * dt\n"
           << "    pos.y = pos.y + dy * speed * dt\n"
           << "    dse.ecs.set_transform_position(player, pos.x, pos.y, 0.0)\n"
           << "end\n";
    } else if (tmpl == ProjectTemplate::Lua) {
        ss << "-- " << name << " entry script (DSEngine Lua)\n"
           << "-- Lifecycle hooks called by the runtime:\n"
           << "--   Awake()    -> once at startup\n"
           << "--   Update(dt) -> every frame (dt = seconds since last frame)\n\n"
           << "function Awake()\n"
           << "    print(\"" << name << " initialized\")\n"
           << "end\n\n"
           << "function Update(dt)\n"
           << "end\n";
    } else if (tmpl == ProjectTemplate::Game3D) {
        ss << "-- " << name << " (3D) entry script\n"
           << "-- DSEngine 3D project template\n"
           << "-- Scene pre-populated with Camera + Directional Light via main.json\n\n"
           << "local state = { camera = nil, ground = nil, cube = nil, time = 0.0 }\n\n"
           << "local function cube_verts()\n"
           << "    return { -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,\n"
           << "             -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5 }\n"
           << "end\n\n"
           << "local function cube_idx()\n"
           << "    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }\n"
           << "end\n\n"
           << "function Awake()\n"
           << "    -- Add free-camera controller to the camera entity loaded from scene\n"
           << "    local cam_entities = dse.ecs.find_entities_with_camera3d and dse.ecs.find_entities_with_camera3d() or {}\n"
           << "    if #cam_entities > 0 then\n"
           << "        state.camera = cam_entities[1]\n"
           << "        dse.ecs.add_free_camera_controller(state.camera, 5.0, 0.12)\n"
           << "    end\n\n"
           << "    -- Ground plane\n"
           << "    state.ground = dse.ecs.create_entity()\n"
           << "    dse.ecs.add_transform(state.ground, 0, -0.06, 0, 10, 0.12, 10)\n"
           << "    dse.ecs.add_mesh_renderer(state.ground, 0.35, 0.38, 0.35, 1.0, cube_verts(), cube_idx())\n"
           << "    dse.ecs.set_mesh_shader_variant(state.ground, \"MESH_LIT\")\n"
           << "    dse.ecs.set_mesh_material(state.ground, 0.0, 0.8, 1.0, 0, 0, 0, 1.0, true, true)\n\n"
           << "    -- Demo cube (orange, slightly metallic)\n"
           << "    state.cube = dse.ecs.create_entity()\n"
           << "    dse.ecs.add_transform(state.cube, 0, 0.5, 0, 1, 1, 1)\n"
           << "    dse.ecs.add_mesh_renderer(state.cube, 0.9, 0.45, 0.18, 1.0, cube_verts(), cube_idx())\n"
           << "    dse.ecs.set_mesh_shader_variant(state.cube, \"MESH_LIT\")\n"
           << "    dse.ecs.set_mesh_material(state.cube, 0.1, 0.4, 1.0, 0, 0, 0, 1.0, true, false)\n\n"
           << "    print(\"[" << name << "] 3D scene ready. Right-click + W/A/S/D/Q/E to navigate.\")\n"
           << "end\n\n"
           << "function Update(dt)\n"
           << "    state.time = state.time + (dt or 0)\n"
           << "    if state.cube then\n"
           << "        dse.ecs.set_transform_rotation(state.cube, 0, state.time * 45.0, 0)\n"
           << "    end\n"
           << "end\n";
    }
    return ss.str();
}

std::string BuildCppMain(const std::string& name) {
    std::ostringstream ss;
    ss << "// " << name << " — DSEngine C++ host\n"
       << "// 生命周期钩子与 Lua 的 Awake/Update 对应：bootstrap / tick / shutdown。\n"
       << "#include \"engine/runtime/engine_app.h\"\n"
       << "#include \"engine/scripting/cpp/cpp_business_runtime.h\"\n"
       << "#include \"engine/ecs/world.h\"\n"
       << "#include \"engine/assets/asset_manager.h\"\n"
       << "#include <iostream>\n\n"
       << "namespace {\n\n"
       << "void Bootstrap(World& world, AssetManager& assets) {\n"
       << "    (void)world; (void)assets;\n"
       << "    std::cout << \"[" << name << "] bootstrap\" << std::endl;\n"
       << "}\n\n"
       << "void Tick(World& world, float dt) {\n"
       << "    (void)world; (void)dt;\n"
       << "}\n\n"
       << "void Shutdown() {\n"
       << "    std::cout << \"[" << name << "] shutdown\" << std::endl;\n"
       << "}\n\n"
       << "} // namespace\n\n"
       << "int main() {\n"
       << "    dse::runtime::ConfigureCppBusinessHooks({ Bootstrap, Tick, Shutdown });\n\n"
       << "    dse::runtime::EngineRunConfig config;\n"
       << "    config.window_width = 1280;\n"
       << "    config.window_height = 720;\n"
       << "    config.window_title = \"" << name << "\";\n"
       << "    config.business_mode = BusinessMode::Cpp;\n"
       << "    config.enable_editor = false;\n"
       << "    return dse::runtime::RunEngine(config);\n"
       << "}\n";
    return ss.str();
}

std::string BuildCppCMake(const std::string& name) {
    std::ostringstream ss;
    ss << "cmake_minimum_required(VERSION 3.17)\n"
       << "project(" << name << " CXX)\n\n"
       << "set(CMAKE_CXX_STANDARD 20)\n"
       << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
       << "if(MSVC)\n"
       << "    add_compile_options(/utf-8 /wd4819)\n"
       << "endif()\n\n"
       << "# 查找已安装的 DSEngine SDK：\n"
       << "#   cmake -B build -DCMAKE_PREFIX_PATH=<dsengine_install_dir>\n"
       << "find_package(DSEngine REQUIRED)\n\n"
       << "add_executable(" << name << " src/main.cpp)\n"
       << "target_link_libraries(" << name << " PRIVATE DSEngine::dse_engine)\n\n"
       << "# 把运行时 DLL 拷到可执行目录\n"
       << "add_custom_command(TARGET " << name << " POST_BUILD\n"
       << "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n"
       << "    $<TARGET_RUNTIME_DLLS:" << name << ">\n"
       << "    $<TARGET_FILE_DIR:" << name << ">\n"
       << "    COMMAND_EXPAND_LISTS\n"
       << ")\n";
    return ss.str();
}

std::string BuildCSharpSln(const std::string& name) {
    // Deterministic GUIDs for the two projects
    const char* runtime_guid = "{A1B2C3D4-1234-5678-9ABC-DEF012345678}";
    const char* game_guid    = "{B2C3D4E5-2345-6789-ABCD-EF0123456789}";
    const char* sln_guid     = "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}";
    std::ostringstream ss;
    ss << "\xEF\xBB\xBF\n"  // UTF-8 BOM
       << "Microsoft Visual Studio Solution File, Format Version 12.00\n"
       << "# Visual Studio Version 17\n"
       << "VisualStudioVersion = 17.0.31903.59\n"
       << "MinimumVisualStudioVersion = 10.0.40219.1\n"
       << "Project(\"" << sln_guid << "\") = \"DSEngine.Runtime\", "
       << "\"DSEngine.Runtime\\DSEngine.Runtime.csproj\", \"" << runtime_guid << "\"\n"
       << "EndProject\n"
       << "Project(\"" << sln_guid << "\") = \"DSEngine.Game\", "
       << "\"DSEngine.Game\\DSEngine.Game.csproj\", \"" << game_guid << "\"\n"
       << "EndProject\n"
       << "Global\n"
       << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
       << "\t\tDebug|Any CPU = Debug|Any CPU\n"
       << "\t\tRelease|Any CPU = Release|Any CPU\n"
       << "\tEndGlobalSection\n"
       << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
       << "\t\t" << runtime_guid << ".Debug|Any CPU.ActiveCfg = Debug|Any CPU\n"
       << "\t\t" << runtime_guid << ".Debug|Any CPU.Build.0 = Debug|Any CPU\n"
       << "\t\t" << runtime_guid << ".Release|Any CPU.ActiveCfg = Release|Any CPU\n"
       << "\t\t" << runtime_guid << ".Release|Any CPU.Build.0 = Release|Any CPU\n"
       << "\t\t" << game_guid << ".Debug|Any CPU.ActiveCfg = Debug|Any CPU\n"
       << "\t\t" << game_guid << ".Debug|Any CPU.Build.0 = Debug|Any CPU\n"
       << "\t\t" << game_guid << ".Release|Any CPU.ActiveCfg = Release|Any CPU\n"
       << "\t\t" << game_guid << ".Release|Any CPU.Build.0 = Release|Any CPU\n"
       << "\tEndGlobalSection\n"
       << "EndGlobal\n";
    return ss.str();
}

std::string BuildCSharpRuntimeCsproj() {
    return R"(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
  </PropertyGroup>
</Project>
)";
}

std::string BuildCSharpGameCsproj() {
    return R"(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="..\DSEngine.Runtime\DSEngine.Runtime.csproj" />
  </ItemGroup>
</Project>
)";
}

std::string BuildCSharpEntity() {
    return R"(using System.Runtime.InteropServices;

namespace DSEngine;

public readonly struct Entity : IEquatable<Entity> {
    public readonly uint Id;
    public Entity(uint id) => Id = id;
    public bool IsValid => Id != 0;
    public bool Equals(Entity other) => Id == other.Id;
    public override bool Equals(object? obj) => obj is Entity e && Equals(e);
    public override int GetHashCode() => (int)Id;
    public static bool operator ==(Entity a, Entity b) => a.Id == b.Id;
    public static bool operator !=(Entity a, Entity b) => a.Id != b.Id;
}
)";
}

std::string BuildCSharpDseScript() {
    return R"(namespace DSEngine;

/// <summary>
/// Base class for all C# game scripts. Attach to entities via CSharpScriptComponent.
/// </summary>
public abstract class DseScript {
    /// <summary>The entity this script is attached to.</summary>
    public Entity Entity { get; internal set; }

    /// <summary>Called once when the script is first activated.</summary>
    public virtual void OnStart() {}

    /// <summary>Called every frame.</summary>
    public virtual void OnUpdate(float dt) {}

    /// <summary>Called at fixed physics timestep.</summary>
    public virtual void OnFixedUpdate(float dt) {}

    /// <summary>Called when the script or entity is destroyed.</summary>
    public virtual void OnDestroy() {}
}
)";
}

std::string BuildCSharpSampleScript(const std::string& name) {
    std::ostringstream ss;
    ss << "using DSEngine;\n\n"
       << "/// <summary>\n"
       << "/// Sample script for " << name << " project.\n"
       << "/// </summary>\n"
       << "public class SampleScript : DseScript {\n"
       << "    public override void OnStart() {\n"
       << "        // Initialize your script here\n"
       << "    }\n\n"
       << "    public override void OnUpdate(float dt) {\n"
       << "        // Update logic runs every frame\n"
       << "    }\n"
       << "}\n";
    return ss.str();
}

} // namespace

bool ParseTemplateToken(const std::string& token, ProjectTemplate& out) {
    if (token == "empty")        { out = ProjectTemplate::Empty;         return true; }
    if (token == "2d")           { out = ProjectTemplate::Game2D;        return true; }
    if (token == "3d")           { out = ProjectTemplate::Game3D;        return true; }
    if (token == "lua")          { out = ProjectTemplate::Lua;           return true; }
    if (token == "cpp")          { out = ProjectTemplate::Cpp;           return true; }
    if (token == "csharp")       { out = ProjectTemplate::CSharp;        return true; }
    if (token == "platformer")   { out = ProjectTemplate::Platformer2D;  return true; }
    if (token == "topdown")      { out = ProjectTemplate::TopDownRPG;    return true; }
    if (token == "thirdperson")  { out = ProjectTemplate::ThirdPerson3D; return true; }
    return false;
}

const char* TemplateDisplayName(ProjectTemplate tmpl) {
    switch (tmpl) {
        case ProjectTemplate::Empty:  return "Empty";
        case ProjectTemplate::Game2D: return "2D";
        case ProjectTemplate::Game3D: return "3D";
        case ProjectTemplate::Lua:    return "Lua";
        case ProjectTemplate::Cpp:    return "C++";
        case ProjectTemplate::CSharp: return "C#";
        case ProjectTemplate::Platformer2D:  return "2D Platformer";
        case ProjectTemplate::TopDownRPG:    return "Top-Down RPG";
        case ProjectTemplate::ThirdPerson3D: return "3D Third-Person";
    }
    return "Unknown";
}

ScaffoldResult ScaffoldProject(const std::string& project_root,
                               const std::string& name,
                               ProjectTemplate tmpl,
                               const std::string& engine_version) {
    ScaffoldResult result;
    const fs::path root(project_root);

    std::error_code ec;
    fs::create_directories(root / "scenes", ec);
    fs::create_directories(root / "scripts", ec);
    fs::create_directories(root / "assets" / "textures", ec);
    fs::create_directories(root / "assets" / "models", ec);
    fs::create_directories(root / "assets" / "audio", ec);
    fs::create_directories(root / "assets" / "font", ec);
    if (ec) {
        result.error = "创建项目目录失败: " + ec.message();
        return result;
    }

    if (!WriteTextFile(root / "project.dseproj",
                       BuildProjectDescriptor(name, tmpl, engine_version), result.error)) {
        return result;
    }
    if (!WriteTextFile(root / "scenes" / "main.json", BuildSceneJson(tmpl), result.error)) {
        return result;
    }
    if (HasLuaScripting(tmpl)) {
        if (!WriteTextFile(root / "scripts" / "main.lua", BuildMainLua(name, tmpl), result.error)) {
            return result;
        }
    }
    if (HasCppHost(tmpl)) {
        fs::create_directories(root / "src", ec);
        if (ec) {
            result.error = "创建 src 目录失败: " + ec.message();
            return result;
        }
        if (!WriteTextFile(root / "src" / "main.cpp", BuildCppMain(name), result.error)) {
            return result;
        }
        if (!WriteTextFile(root / "CMakeLists.txt", BuildCppCMake(name), result.error)) {
            return result;
        }
    }
    if (HasCSharpScripting(tmpl)) {
        fs::path cs_root = root / "GameScripts";
        fs::create_directories(cs_root / "DSEngine.Runtime" / "Core", ec);
        fs::create_directories(cs_root / "DSEngine.Game", ec);
        if (ec) {
            result.error = "创建 GameScripts 目录失败: " + ec.message();
            return result;
        }
        if (!WriteTextFile(cs_root / "DSEngine.sln", BuildCSharpSln(name), result.error)) {
            return result;
        }
        if (!WriteTextFile(cs_root / "DSEngine.Runtime" / "DSEngine.Runtime.csproj",
                           BuildCSharpRuntimeCsproj(), result.error)) {
            return result;
        }
        if (!WriteTextFile(cs_root / "DSEngine.Runtime" / "Core" / "Entity.cs",
                           BuildCSharpEntity(), result.error)) {
            return result;
        }
        if (!WriteTextFile(cs_root / "DSEngine.Runtime" / "Core" / "DseScript.cs",
                           BuildCSharpDseScript(), result.error)) {
            return result;
        }
        if (!WriteTextFile(cs_root / "DSEngine.Game" / "DSEngine.Game.csproj",
                           BuildCSharpGameCsproj(), result.error)) {
            return result;
        }
        if (!WriteTextFile(cs_root / "DSEngine.Game" / "SampleScript.cs",
                           BuildCSharpSampleScript(name), result.error)) {
            return result;
        }
    }
    if (!WriteTextFile(root / ".gitignore", "build/\n.lock\n*.log\n", result.error)) {
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace dse::project
