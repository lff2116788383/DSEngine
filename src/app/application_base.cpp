//
// Created by captainchen on 2021/5/14.
//

#include "application_base.h"
#include <memory>
#include <iostream>
#include <vector>
#include <filesystem>
#include "rttr/registration"
#include "easy/profiler.h"
#include "utils/debug.h"
#include "control/input.h"
#include "utils/screen.h"
#include "render_device/render_task_consumer.h"
#include "utils/time.h"
#include "render_device/render_task_producer.h"
#include "lua_binding/lua_binding.h"
#include "phase1/runtime/frame_pipeline.h"

namespace fs = std::filesystem;

void ApplicationBase::Init() {
    EASY_MAIN_THREAD;
    profiler::startListen();// 启动profiler服务器，等待gui连接。

    Debug::Init();
    DEBUG_LOG_INFO("game start");

    InitLuaBinding();
    LoadConfig();

    Time::Init();

    //初始化图形库，例如glfw
    InitGraphicsLibraryFramework();

    UpdateScreenSize();

    Phase1FramePipeline::Instance().Init();
}

/// 初始化图形库，例如glfw
void ApplicationBase::InitGraphicsLibraryFramework() {

}

void ApplicationBase::InitLuaBinding() {
    //自动探测脚本目录
    std::string script_path = "";
    std::vector<std::string> possible_paths = {"script/", "../script/", "../../script/"};
    for(const auto& path : possible_paths){
        if(fs::exists(path)){
            script_path = path;
            break;
        }
    }
    
    std::string examples_path = "";
    std::vector<std::string> possible_example_paths = {"examples/", "../examples/", "../../examples/"};
    for(const auto& path : possible_example_paths){
        if(fs::exists(path)){
            examples_path = path;
            break;
        }
    }
    
    // 如果找不到，使用默认值
    if(script_path.empty()) script_path = "../script/";
    if(examples_path.empty()) examples_path = "../examples/";
    
    std::string package_path = ";" + examples_path + "?.lua;" + script_path + "?.lua;" + script_path + "utils/?.lua;" + script_path + "component/?.lua";
    LuaBinding::Init(package_path);

    //绑定引擎所有类到Lua
    LuaBinding::BindLua();
    
    //执行lua config
    std::string config_file = examples_path + "config.lua";
    if(fs::exists(config_file)){
        LuaBinding::RunLuaFile(config_file);
    }else{
        DEBUG_LOG_ERROR("config.lua not found in {}", examples_path);
    }
}

void ApplicationBase::LoadConfig() {
    sol::state& sol_state=LuaBinding::sol_state();
    title_=sol_state["Config"]["title"].get_or<std::string>("DSEngine");
    std::string config_data_path = sol_state["Config"]["data_path"].get_or<std::string>("data/");
    
    // Check if configured path exists relative to CWD
    if (fs::exists(config_data_path)) {
        data_path_ = config_data_path;
    } else {
        // Try to find data folder
        std::vector<std::string> potential_paths = {
            "data/",
            "../data/",
            "../../data/"
        };
        for(const auto& p : potential_paths){
            if(fs::exists(p)){
                data_path_ = p;
                break;
            }
        }
        if(data_path_.empty()){
            data_path_ = config_data_path; // Fallback
            DEBUG_LOG_ERROR("Data path not found, using config value: {}", data_path_);
        }
    }
    DEBUG_LOG_INFO("Data path set to: {}", data_path_);
}

void ApplicationBase::Run() {
    // 查找 main.lua
    std::string main_lua_path = "";
    // 优先加载 Mirror Game 的入口
    std::vector<std::string> possible_paths = {
        "script/mirror_game/main.lua", 
        "../script/mirror_game/main.lua", 
        "../../script/mirror_game/main.lua",
        "examples/main.lua", 
        "../examples/main.lua", 
        "../../examples/main.lua"
    };
    for(const auto& path : possible_paths){
        if(fs::exists(path)){
            main_lua_path = path;
            break;
        }
    }
    
    if(!main_lua_path.empty()){
        LuaBinding::RunLuaFile(main_lua_path);
        
        // 尝试调用 MirrorGame.Awake()，如果失败则回退到 main()
        // 注意：LuaBinding::CallLuaFunction 目前只支持全局函数
        // 如果我们想调用 MirrorGame.Awake，可能需要先在 Lua 端暴露一个全局函数
        // 或者修改 main.lua 让它返回一个全局变量
        
        // 这里简单处理：让 LuaBinding 尝试执行一段代码来启动
        // 或者我们约定 main.lua 必须包含一个全局的 main 函数或者 Awake 函数
        
        // 为了兼容旧代码，我们在 mirror_game/main.lua 中定义一个全局 Awake
        LuaBinding::CallLuaFunction("Awake");
    }else{
        DEBUG_LOG_ERROR("main.lua not found!");
    }
}

void ApplicationBase::Update(){
    EASY_FUNCTION(profiler::colors::Magenta) // 标记函数
    Time::Update();
    UpdateScreenSize();
    Phase1FramePipeline::Instance().Update(Time::delta_time());

    Input::Update();
}


void ApplicationBase::Render(){
    EASY_FUNCTION(profiler::colors::Magenta); // 标记函数
    Phase1FramePipeline::Instance().Render();
}

void ApplicationBase::FixedUpdate(){
    EASY_FUNCTION(profiler::colors::Magenta) // 标记函数
    Phase1FramePipeline::Instance().FixedUpdate(Time::fixed_update_time());
}

void ApplicationBase::OneFrame() {
    Update();
    // 如果一帧卡了很久，就多执行几次FixedUpdate
    float cost_time=Time::delta_time();
    while(cost_time>=Time::fixed_update_time()){
        FixedUpdate();
        cost_time-=Time::fixed_update_time();
    }

    Render();

}

void ApplicationBase::UpdateScreenSize() {
    RenderTaskProducer::ProduceRenderTaskUpdateScreenSize();
}

void ApplicationBase::Exit() {
    RenderTaskProducer::Exit();
    RenderTaskConsumer::Exit();

    //调用lua exit()
    LuaBinding::CallLuaFunction("exit");

    Debug::ShutDown();
}
