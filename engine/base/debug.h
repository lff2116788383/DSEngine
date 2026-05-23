/**
 * @file debug.h
 * @brief 调试与日志系统，提供日志记录、断言和基础的调试工具
 */

//
// Created by captainchen on 2021/8/23.
//

#ifndef UNTITLED_DEBUG_H
#define UNTITLED_DEBUG_H

#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include "engine/core/dse_export.h"

namespace dse::debug {

enum class LogLevel {
    Trace = 0,
    Info,
    Warn,
    Error,
    Off
};

DSE_EXPORT void LogMessage(LogLevel level, const std::string& message);
DSE_EXPORT void SetLogLevel(LogLevel level);
DSE_EXPORT LogLevel GetLogLevel();

inline void AppendFormatted(std::ostringstream& oss, const char* format) {
    if (format) {
        oss << format;
    }
}

template <typename T, typename... Rest>
void AppendFormatted(std::ostringstream& oss, const char* format, T&& value, Rest&&... rest) {
    if (!format) {
        return;
    }

    const char* placeholder = std::strstr(format, "{}");
    if (!placeholder) {
        oss << format;
        return;
    }

    oss.write(format, static_cast<std::streamsize>(placeholder - format));
    oss << std::forward<T>(value);
    AppendFormatted(oss, placeholder + 2, std::forward<Rest>(rest)...);
}

template <typename... Args>
std::string Format(const char* format, Args&&... args) {
    std::ostringstream oss;
    AppendFormatted(oss, format, std::forward<Args>(args)...);
    return oss.str();
}

} // namespace dse::debug

#define DEBUG_LOG_TRACE(...) do { if(Debug::CanLog()) { dse::debug::LogMessage(dse::debug::LogLevel::Trace, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DEBUG_LOG_INFO(...) do { if(Debug::CanLog()) { dse::debug::LogMessage(dse::debug::LogLevel::Info, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DEBUG_LOG_WARN(...) do { if(Debug::CanLog()) { dse::debug::LogMessage(dse::debug::LogLevel::Warn, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DEBUG_LOG_ERROR(...) do { if(Debug::CanLog()) { dse::debug::LogMessage(dse::debug::LogLevel::Error, dse::debug::Format(__VA_ARGS__)); } } while(0)

#define __CHECK_GL_ERROR__ { \
        auto gl_error_code=glGetError();\
        if(gl_error_code!=GL_NO_ERROR){\
            DEBUG_LOG_ERROR("gl_error_code: {}",gl_error_code);\
        }\
    }

/**
 * @class Debug
 * @brief 调试类，提供日志系统的初始化和资源释放管理
 */
class Debug {
public:
    /**
     * @brief 初始化日志系统，配置控制台和文件多路输出
     */
    static void Init();

    static bool CanLog();

    /**
     * @brief 关闭并释放日志系统资源
     */
    static void ShutDown();

public:
    static bool bInited_;
};

#endif //UNTITLED_DEBUG_H
