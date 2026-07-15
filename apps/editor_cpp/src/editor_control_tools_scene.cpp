#include "editor_control_tools_common.h"

#include "editor_control_server.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/ecs/blueprint_component.h"
#include "engine/ecs/components_3d_impostor.h"
#include "engine/base/time.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/assets/asset_manager.h"
#include "editor_toolbar.h"
#include "editor_scene_io.h"
#include "editor_shared_components.h"
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "editor_entity_snapshot.h"
#include "editor_shell.h"
#include "editor_scene_tabs.h"
#include "editor_prefab.h"
#include "editor_selection.h"
#include "editor_project.h"
#include "editor_asset_db.h"

#include <vector>
#include <string>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stb/stb_image_write.h>

namespace dse::editor {

static JsonRpcResponse HandleScriptCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("path") || !params["path"].IsString() ||
        !params.HasMember("content") || !params["content"].IsString()) {
        return MakeToolError(-32602, "Missing required params: path, content");
    }

    std::filesystem::path file_path = params["path"].GetString();
    // 安全检查：不允许 .. 路径逃逸
    if (file_path.string().find("..") != std::string::npos) {
        return MakeToolError(-32602, "Path must not contain '..'");
    }

    std::filesystem::create_directories(file_path.parent_path());
    std::ofstream ofs(file_path, std::ios::trunc);
    if (!ofs.is_open()) {
        return MakeToolError(-32603, "Failed to write file: " + file_path.string());
    }
    ofs << params["content"].GetString();
    ofs.close();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(file_path.string().c_str(), alloc), alloc);
    result.AddMember("written", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleSceneSave(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string path;
    if (params.HasMember("path") && params["path"].IsString()) {
        path = params["path"].GetString();
    } else {
        path = GetCurrentScenePath();
        if (path.empty() || path == "Untitled") {
            return MakeToolError(-32602, "No path specified and no current scene file");
        }
    }

    auto& registry = engine.pipeline()->world().registry();
    SaveScene(registry, path);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("saved", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleSceneLoad(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::string path = params["path"].GetString();
    if (!std::filesystem::exists(path)) {
        return MakeToolError(-32602, "Scene file not found: " + path);
    }

    auto& registry = engine.pipeline()->world().registry();
    LoadScene(registry, path);
    SetCurrentScenePath(path);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("loaded", rapidjson::Value(true), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleAssetImport(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }
    std::string path = params["path"].GetString();

    std::string type = "auto";
    if (params.HasMember("type") && params["type"].IsString()) {
        type = params["type"].GetString();
    }

    auto* am = engine.asset_manager();
    if (!am) {
        return MakeToolError(-32603, "AssetManager not available");
    }

    // auto-detect type from extension
    if (type == "auto") {
        auto ext_pos = path.rfind('.');
        if (ext_pos != std::string::npos) {
            std::string ext = path.substr(ext_pos);
            for (auto& c : ext) c = static_cast<char>(std::tolower(c));
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                ext == ".tga" || ext == ".dds" || ext == ".hdr" || ext == ".ppm")
                type = "texture";
            else if (ext == ".dmesh")
                type = "mesh";
            else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac")
                type = "audio";
            else if (ext == ".dmat")
                type = "material";
        }
        if (type == "auto") {
            return MakeToolError(-32602, "Cannot auto-detect asset type for: " + path);
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (type == "texture") {
        auto tex = am->LoadTexture(path);
        if (!tex) return MakeToolError(-32603, "Failed to load texture: " + path);
        result.AddMember("type", "texture", alloc);
        result.AddMember("handle", tex->GetHandle().raw(), alloc);
        result.AddMember("width", tex->GetWidth(), alloc);
        result.AddMember("height", tex->GetHeight(), alloc);
        result.AddMember("channels", tex->GetChannels(), alloc);
    } else if (type == "mesh") {
        auto mesh = am->LoadDmesh(path);
        if (!mesh) return MakeToolError(-32603, "Failed to load mesh: " + path);
        result.AddMember("type", "mesh", alloc);
        result.AddMember("path", rapidjson::Value(mesh->GetPath().c_str(), alloc), alloc);
        result.AddMember("size_bytes", static_cast<uint64_t>(mesh->GetData().size()), alloc);
    } else if (type == "audio") {
        auto clip = am->LoadAudioClip(path);
        if (!clip) return MakeToolError(-32603, "Failed to load audio: " + path);
        result.AddMember("type", "audio", alloc);
        result.AddMember("path", rapidjson::Value(clip->GetPath().c_str(), alloc), alloc);
        result.AddMember("size_bytes", static_cast<uint64_t>(clip->GetData().size()), alloc);
    } else if (type == "material") {
        std::size_t idx = 0;
        if (params.HasMember("material_index") && params["material_index"].IsUint())
            idx = params["material_index"].GetUint();
        auto mat = am->LoadMaterialInstanceFromDmat(path, idx);
        if (!mat) return MakeToolError(-32603, "Failed to load material: " + path);
        result.AddMember("type", "material", alloc);
        result.AddMember("material_id", mat->GetId(), alloc);
        result.AddMember("name", rapidjson::Value(mat->GetName().c_str(), alloc), alloc);
    } else {
        return MakeToolError(-32602, "Unknown asset type: " + type);
    }

    result.AddMember("success", true, alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleMaterialCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string name = "untitled_material";
    if (params.HasMember("name") && params["name"].IsString()) {
        name = params["name"].GetString();
    }

    auto* am = engine.asset_manager();
    if (!am) {
        return MakeToolError(-32603, "AssetManager not available");
    }

    // Build .dmat JSON
    rapidjson::Document dmat;
    dmat.SetObject();
    auto& da = dmat.GetAllocator();

    rapidjson::Value mat_obj(rapidjson::kObjectType);
    mat_obj.AddMember("name", rapidjson::Value(name.c_str(), da), da);

    // shader_variant
    std::string shader_variant = "MESH_PBR";
    if (params.HasMember("shader_variant") && params["shader_variant"].IsString())
        shader_variant = params["shader_variant"].GetString();
    mat_obj.AddMember("shader_variant", rapidjson::Value(shader_variant.c_str(), da), da);

    // base_color [r,g,b,a]
    if (params.HasMember("base_color") && params["base_color"].IsArray()) {
        rapidjson::Value bc(rapidjson::kArrayType);
        for (auto& v : params["base_color"].GetArray()) bc.PushBack(v.GetFloat(), da);
        mat_obj.AddMember("base_color", bc, da);
    } else {
        rapidjson::Value bc(rapidjson::kArrayType);
        bc.PushBack(1.0f, da).PushBack(1.0f, da).PushBack(1.0f, da).PushBack(1.0f, da);
        mat_obj.AddMember("base_color", bc, da);
    }

    // emissive [r,g,b]
    if (params.HasMember("emissive") && params["emissive"].IsArray()) {
        rapidjson::Value em(rapidjson::kArrayType);
        for (auto& v : params["emissive"].GetArray()) em.PushBack(v.GetFloat(), da);
        mat_obj.AddMember("emissive", em, da);
    }

    // scalars
    auto addFloat = [&](const char* key) {
        if (params.HasMember(key) && params[key].IsNumber())
            mat_obj.AddMember(rapidjson::Value(key, da),
                              rapidjson::Value(params[key].GetFloat()), da);
    };
    addFloat("metallic");
    addFloat("roughness");
    addFloat("occlusion_strength");
    addFloat("normal_scale");
    addFloat("alpha_cutoff");

    if (params.HasMember("alpha_test") && params["alpha_test"].IsBool())
        mat_obj.AddMember("alpha_test", params["alpha_test"].GetBool(), da);
    if (params.HasMember("double_sided") && params["double_sided"].IsBool())
        mat_obj.AddMember("double_sided", params["double_sided"].GetBool(), da);

    // texture paths
    auto addTex = [&](const char* key) {
        if (params.HasMember(key) && params[key].IsString())
            mat_obj.AddMember(rapidjson::Value(key, da),
                              rapidjson::Value(params[key].GetString(), da), da);
    };
    addTex("base_color_texture");
    addTex("normal_texture");
    addTex("metallic_roughness_texture");
    addTex("emissive_texture");
    addTex("occlusion_texture");

    rapidjson::Value materials_arr(rapidjson::kArrayType);
    materials_arr.PushBack(mat_obj, da);
    dmat.AddMember("materials", materials_arr, da);

    // Serialize to JSON string
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    dmat.Accept(writer);
    std::string json_str = sb.GetString();

    // Save to file
    std::string save_path;
    if (params.HasMember("save_path") && params["save_path"].IsString()) {
        save_path = params["save_path"].GetString();
    } else {
        std::string data_root = am->GetDataRoot();
        save_path = data_root + "/materials/" + name + ".dmat";
    }

    // Ensure directory exists
    std::filesystem::path file_path(save_path);
    if (file_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
    }

    {
        std::ofstream f(save_path, std::ios::binary);
        if (!f.is_open()) {
            return MakeToolError(-32603, "Failed to write material file: " + save_path);
        }
        f.write(json_str.data(), static_cast<std::streamsize>(json_str.size()));
    }

    // Load into engine
    auto loaded = am->LoadMaterialInstanceFromDmat(save_path, 0);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", true, alloc);
    result.AddMember("file_path", rapidjson::Value(save_path.c_str(), alloc), alloc);
    if (loaded) {
        result.AddMember("material_id", loaded->GetId(), alloc);
        result.AddMember("material_name", rapidjson::Value(loaded->GetName().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandlePrefabSave(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string path = params["path"].GetString();
    if (path.find("..") != std::string::npos) {
        return MakeToolError(-32602, "Path must not contain '..'");
    }

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (!SaveEntityAsPrefab(registry, entity, path)) {
        return MakeToolError(-32603, "Failed to save prefab: " + path);
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("saved", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandlePrefabInstantiate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::string path = params["path"].GetString();
    if (!std::filesystem::exists(path)) {
        return MakeToolError(-32602, "Prefab file not found: " + path);
    }

    auto& world = engine.pipeline()->world();
    auto& registry = world.registry();
    auto entity = InstantiatePrefab(world, registry, path);

    if (entity == entt::null) {
        return MakeToolError(-32603, "Failed to instantiate prefab: " + path);
    }

    // Undo: destroy the instantiated entity
    {
        entt::entity inst = entity;
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Instantiate Prefab (RPC)",
            []() {},
            [inst, &reg]() { if (reg.valid(inst)) reg.destroy(inst); }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    std::string inst_name = "Prefab Instance";
    if (registry.all_of<EditorNameComponent>(entity))
        inst_name = registry.get<EditorNameComponent>(entity).name;
    result.AddMember("name", rapidjson::Value(inst_name.c_str(), alloc), alloc);
    result.AddMember("source_path", rapidjson::Value(path.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleSceneNew(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    registry.clear();
    GetUndoRedoManager().Clear();
    SetCurrentScenePath("Untitled");

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("cleared", rapidjson::Value(true), alloc);
    result.AddMember("path", rapidjson::Value("Untitled", alloc), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleProjectOpen(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::filesystem::path dseproj = params["path"].GetString();
    if (!std::filesystem::exists(dseproj)) {
        return MakeToolError(-32602, "Project file not found: " + dseproj.string());
    }

    auto& pm = dse::editor::ProjectManager::Get();
    if (!pm.OpenProject(dseproj)) {
        return MakeToolError(-32603, "Failed to open project: " + dseproj.string());
    }
    pm.ApplyDataRoot();
    if (auto* am = engine.asset_manager()) {
        am->ConfigureDataRoot(pm.GetAssetDir().string());
    }
    dse::editor::AssetDatabase::Get().Refresh();

    auto& registry = engine.pipeline()->world().registry();
    bool scene_loaded = false;
    std::string scene_path;
    {
        std::filesystem::path candidate = pm.GetProjectRoot() / pm.GetDescriptor().default_scene;
        if (std::filesystem::exists(candidate)) {
            LoadScene(registry, candidate.string());
            SetCurrentScenePath(candidate.string());
            scene_path = candidate.string();
            scene_loaded = true;
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("opened", rapidjson::Value(true), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    result.AddMember("scene_loaded", rapidjson::Value(scene_loaded), alloc);
    if (scene_loaded) {
        result.AddMember("scene_path", rapidjson::Value(scene_path.c_str(), alloc), alloc);
    }
    rapidjson::Value project(rapidjson::kObjectType);
    project.AddMember("name", rapidjson::Value(pm.GetDescriptor().name.c_str(), alloc), alloc);
    project.AddMember("root", rapidjson::Value(pm.GetProjectRoot().string().c_str(), alloc), alloc);
    project.AddMember("default_scene", rapidjson::Value(pm.GetDescriptor().default_scene.c_str(), alloc), alloc);
    result.AddMember("project", project, alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleScriptAttach(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint())
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    if (!params.HasMember("script_path") || !params["script_path"].IsString())
        return MakeToolError(-32602, "Missing required param: script_path (string)");

    auto& registry = engine.pipeline()->world().registry();
    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    if (!registry.valid(entity))
        return MakeToolError(-32602, "Invalid entity_id");

    std::string path = params["script_path"].GetString();
    std::string lang = "lua";
    if (params.HasMember("language") && params["language"].IsString())
        lang = params["language"].GetString();

    if (lang == "lua") {
        auto& sc = registry.emplace_or_replace<LuaScriptComponent>(entity);
        sc.script_path = path;
    } else if (lang == "csharp" || lang == "cs") {
        auto& sc = registry.emplace_or_replace<CSharpScriptComponent>(entity);
        sc.class_name = path;
        sc.enabled = true;
    } else if (lang == "blueprint") {
        auto& sc = registry.emplace_or_replace<BlueprintComponent>(entity);
        sc.blueprint_asset_path = path;
        sc.enabled = true;
    } else {
        auto& sc = registry.emplace_or_replace<ScriptComponent>(entity);
        sc.script_path = path;
        sc.enabled = true;
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", params["entity_id"].GetUint(), alloc);
    result.AddMember("script_path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("language", rapidjson::Value(lang.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleSceneList(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& pm = dse::editor::ProjectManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    rapidjson::Value scenes(rapidjson::kArrayType);
    if (pm.HasOpenProject()) {
        auto scene_dir = pm.GetSceneDir();
        if (std::filesystem::exists(scene_dir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(scene_dir)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                if (ext == ".json" || ext == ".dscene") {
                    auto rel = std::filesystem::relative(entry.path(), pm.GetProjectRoot());
                    scenes.PushBack(rapidjson::Value(rel.string().c_str(), alloc), alloc);
                }
            }
        }
    }
    result.AddMember("scenes", scenes, alloc);
    result.AddMember("count", static_cast<int>(scenes.Size()), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleProjectGetInfo(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& pm = dse::editor::ProjectManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (!pm.HasOpenProject()) {
        result.AddMember("has_project", false, alloc);
        return MakeOk(std::move(result));
    }

    const auto& desc = pm.GetDescriptor();
    result.AddMember("has_project", true, alloc);
    result.AddMember("name", rapidjson::Value(desc.name.c_str(), alloc), alloc);
    result.AddMember("version", rapidjson::Value(desc.version.c_str(), alloc), alloc);
    result.AddMember("engine_version", rapidjson::Value(desc.engine_version.c_str(), alloc), alloc);
    result.AddMember("description", rapidjson::Value(desc.description.c_str(), alloc), alloc);
    result.AddMember("root", rapidjson::Value(pm.GetProjectRoot().string().c_str(), alloc), alloc);
    result.AddMember("default_scene", rapidjson::Value(desc.default_scene.c_str(), alloc), alloc);
    result.AddMember("asset_dir", rapidjson::Value(desc.asset_dir.c_str(), alloc), alloc);
    result.AddMember("scene_dir", rapidjson::Value(desc.scene_dir.c_str(), alloc), alloc);
    result.AddMember("script_dir", rapidjson::Value(desc.script_dir.c_str(), alloc), alloc);
    result.AddMember("entry_script", rapidjson::Value(desc.entry_script.c_str(), alloc), alloc);

    rapidjson::Value features(rapidjson::kArrayType);
    for (const auto& f : desc.features)
        features.PushBack(rapidjson::Value(f.c_str(), alloc), alloc);
    result.AddMember("features", features, alloc);

    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleAssetList(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& pm = dse::editor::ProjectManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (!pm.HasOpenProject()) {
        result.AddMember("error", "No project open", alloc);
        return MakeOk(std::move(result));
    }

    std::string filter;
    if (params.HasMember("filter") && params["filter"].IsString())
        filter = params["filter"].GetString();

    std::string subdir;
    if (params.HasMember("directory") && params["directory"].IsString())
        subdir = params["directory"].GetString();

    auto asset_dir = pm.GetAssetDir();
    if (!subdir.empty())
        asset_dir = asset_dir / subdir;

    rapidjson::Value assets(rapidjson::kArrayType);
    if (std::filesystem::exists(asset_dir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(asset_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (!filter.empty() && ext != filter) continue;
            auto rel = std::filesystem::relative(entry.path(), pm.GetProjectRoot());
            rapidjson::Value item(rapidjson::kObjectType);
            item.AddMember("path", rapidjson::Value(rel.string().c_str(), alloc), alloc);
            item.AddMember("extension", rapidjson::Value(ext.c_str(), alloc), alloc);
            item.AddMember("size", static_cast<int64_t>(entry.file_size()), alloc);
            assets.PushBack(item, alloc);
        }
    }
    result.AddMember("assets", assets, alloc);
    result.AddMember("count", static_cast<int>(assets.Size()), alloc);
    return MakeOk(std::move(result));
}

static JsonRpcResponse HandleAssetDelete(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("path") || !params["path"].IsString())
        return MakeToolError(-32602, "Missing required param: path (string)");

    auto& pm = dse::editor::ProjectManager::Get();
    if (!pm.HasOpenProject())
        return MakeToolError(-32600, "No project open");

    std::string rel_path = params["path"].GetString();
    auto full_path = pm.GetProjectRoot() / rel_path;

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (!std::filesystem::exists(full_path)) {
        result.AddMember("deleted", false, alloc);
        result.AddMember("error", "File not found", alloc);
    } else {
        std::error_code ec;
        bool ok = std::filesystem::remove(full_path, ec);
        result.AddMember("deleted", ok, alloc);
        if (!ok)
            result.AddMember("error", rapidjson::Value(ec.message().c_str(), alloc), alloc);
    }
    result.AddMember("path", rapidjson::Value(rel_path.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

void RegisterSceneTools(ControlServer& server, std::vector<std::string>& names) {
    server.RegisterTool("dsengine_scene_save", HandleSceneSave);
    names.push_back("dsengine_scene_save");
    server.RegisterTool("dsengine_scene_load", HandleSceneLoad);
    names.push_back("dsengine_scene_load");
    server.RegisterTool("dsengine_scene_new", HandleSceneNew);
    names.push_back("dsengine_scene_new");
    server.RegisterTool("dsengine_scene_list", HandleSceneList);
    names.push_back("dsengine_scene_list");
    server.RegisterTool("dsengine_prefab_save", HandlePrefabSave);
    names.push_back("dsengine_prefab_save");
    server.RegisterTool("dsengine_prefab_instantiate", HandlePrefabInstantiate);
    names.push_back("dsengine_prefab_instantiate");
    server.RegisterTool("dsengine_project_open", HandleProjectOpen);
    names.push_back("dsengine_project_open");
    server.RegisterTool("dsengine_project_get_info", HandleProjectGetInfo);
    names.push_back("dsengine_project_get_info");
    server.RegisterTool("dsengine_asset_import", HandleAssetImport);
    names.push_back("dsengine_asset_import");
    server.RegisterTool("dsengine_material_create", HandleMaterialCreate);
    names.push_back("dsengine_material_create");
    server.RegisterTool("dsengine_asset_list", HandleAssetList);
    names.push_back("dsengine_asset_list");
    server.RegisterTool("dsengine_asset_delete", HandleAssetDelete);
    names.push_back("dsengine_asset_delete");
    server.RegisterTool("dsengine_script_create", HandleScriptCreate);
    names.push_back("dsengine_script_create");
    server.RegisterTool("dsengine_script_attach", HandleScriptAttach);
    names.push_back("dsengine_script_attach");
}

} // namespace dse::editor
