/**
 * @file debug.cpp
 * @brief 调试与日志系统，提供日志记录、断言和基础的调试工具
 */

//
// Created by captainchen on 2021/8/23.
//

#include "debug.h"
#include <iostream>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

bool Debug::bInited_=false;

bool Debug::CanLog() {
    return bInited_ && spdlog::default_logger() != nullptr;
}

void Debug::Init() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/multisink.txt", true);
    file_sink->set_level(spdlog::level::trace);

    spdlog::set_default_logger(std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list({console_sink, file_sink})));
    spdlog::set_pattern("[source %s] [function %!] [line %#] [%^%l%$] %v");
    spdlog::flush_on(spdlog::level::trace);

    bInited_=true;
    DEBUG_LOG_INFO("spdlog init success");
}

void Debug::ShutDown() {
    bInited_=false;
    if (spdlog::default_logger()) {
        spdlog::set_default_logger(nullptr);
    }
    spdlog::shutdown();
}
