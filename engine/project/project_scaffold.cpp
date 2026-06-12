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
    return tmpl != ProjectTemplate::Empty;
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
    std::ostringstream ss;
    ss << "{\n";
    ss << "    \"format_version\": 1,\n";
    ss << "    \"name\": \"" << JsonEscape(name) << "\",\n";
    ss << "    \"version\": \"1.0.0\",\n";
    ss << "    \"engine_version\": \"" << JsonEscape(engine_version) << "\",\n";
    ss << "    \"description\": \"\",\n";
    ss << "    \"features\": [" << (lua ? "\"lua_scripting\"" : "") << "],\n";
    ss << "    \"entry_script\": \"" << (lua ? "scripts/main.lua" : "") << "\",\n";
    ss << "    \"default_scene\": \"scenes/main.json\",\n";
    ss << "    \"asset_dir\": \"assets/\",\n";
    ss << "    \"scene_dir\": \"scenes/\",\n";
    ss << "    \"script_dir\": \"scripts/\",\n";
    ss << "    \"build\": {\n";
    ss << "        \"output_dir\": \"build/\",\n";
    ss << "        \"target\": \"standalone\"\n";
    ss << "    }\n";
    ss << "}\n";
    return ss.str();
}

std::string BuildSceneJson(ProjectTemplate tmpl) {
    if (tmpl == ProjectTemplate::Game3D) {
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

std::string BuildMainLua(const std::string& name, ProjectTemplate tmpl) {
    std::ostringstream ss;
    if (tmpl == ProjectTemplate::Game2D || tmpl == ProjectTemplate::Lua) {
        ss << "-- " << name << " entry script\n"
           << "-- DSEngine Lua scripting\n\n"
           << "function on_init()\n"
           << "    print(\"" << name << " initialized\")\n"
           << "end\n\n"
           << "function on_update(dt)\n"
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
           << "function on_init()\n"
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
           << "function on_update(dt)\n"
           << "    state.time = state.time + (dt or 0)\n"
           << "    if state.cube then\n"
           << "        dse.ecs.set_transform_rotation(state.cube, 0, state.time * 45.0, 0)\n"
           << "    end\n"
           << "end\n";
    }
    return ss.str();
}

} // namespace

bool ParseTemplateToken(const std::string& token, ProjectTemplate& out) {
    if (token == "empty") { out = ProjectTemplate::Empty;  return true; }
    if (token == "2d")    { out = ProjectTemplate::Game2D; return true; }
    if (token == "3d")    { out = ProjectTemplate::Game3D; return true; }
    if (token == "lua")   { out = ProjectTemplate::Lua;    return true; }
    return false;
}

const char* TemplateDisplayName(ProjectTemplate tmpl) {
    switch (tmpl) {
        case ProjectTemplate::Empty:  return "Empty";
        case ProjectTemplate::Game2D: return "2D";
        case ProjectTemplate::Game3D: return "3D";
        case ProjectTemplate::Lua:    return "Lua";
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
    if (!WriteTextFile(root / ".gitignore", "build/\n.lock\n*.log\n", result.error)) {
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace dse::project
