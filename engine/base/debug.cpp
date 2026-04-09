/**
 * @file debug.cpp
 * @brief 调试与日志系统，提供日志记录、断言和基础的调试工具
 */

//
// Created by captainchen on 2021/8/23.
//

#include "debug.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

#include "spdlog/fmt/bundled/format.h"

namespace {
std::mutex& LogMutex() {
    static std::mutex mutex;
    return mutex;
}

std::ofstream& LogFile() {
    static std::ofstream file;
    return file;
}

dse::debug::LogLevel& CurrentLogLevel() {
    static dse::debug::LogLevel level = dse::debug::LogLevel::Info;
    return level;
}

const char* ToLabel(dse::debug::LogLevel level) {
    switch (level) {
        case dse::debug::LogLevel::Trace: return "TRACE";
        case dse::debug::LogLevel::Info: return "INFO";
        case dse::debug::LogLevel::Warn: return "WARN";
        case dse::debug::LogLevel::Error: return "ERROR";
        case dse::debug::LogLevel::Off: return "OFF";
        default: return "UNKNOWN";
    }
}

bool ShouldLog(dse::debug::LogLevel level) {
    return level >= CurrentLogLevel() && CurrentLogLevel() != dse::debug::LogLevel::Off;
}
}

bool Debug::bInited_ = false;

namespace dse::debug {

void LogMessage(LogLevel level, const std::string& message) {
    if (!Debug::CanLog() || !ShouldLog(level)) {
        return;
    }

    std::lock_guard<std::mutex> lock(LogMutex());
    std::ostringstream oss;
    oss << "[" << ToLabel(level) << "] " << message;
    const std::string line = oss.str();

    if (level == LogLevel::Error || level == LogLevel::Warn) {
        std::cerr << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }

    auto& file = LogFile();
    if (file.is_open()) {
        file << line << std::endl;
        file.flush();
    }
}

void SetLogLevel(LogLevel level) {
    CurrentLogLevel() = level;
}

LogLevel GetLogLevel() {
    return CurrentLogLevel();
}

} // namespace dse::debug

bool Debug::CanLog() {
    return bInited_;
}

void Debug::Init() {
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);

    auto& file = LogFile();
    if (!file.is_open()) {
        file.open("logs/multisink.txt", std::ios::out | std::ios::app);
    }

    dse::debug::SetLogLevel(dse::debug::LogLevel::Info);
    bInited_ = true;
    DEBUG_LOG_INFO("debug logger init success");
}

void Debug::ShutDown() {
    DEBUG_LOG_INFO("debug logger shutdown");
    bInited_ = false;

    auto& file = LogFile();
    if (file.is_open()) {
        file.flush();
        file.close();
    }
}
