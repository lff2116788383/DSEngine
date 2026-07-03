/**
 * @file debug.h
 * @brief Production-grade logging system with categories, level filtering, file rotation,
 *        crash-safe flush, and callback mechanism for editor integration.
 */

#ifndef UNTITLED_DEBUG_H
#define UNTITLED_DEBUG_H

#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include "engine/core/dse_export.h"

namespace dse::debug {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off
};

/// Callback signature: (level, category, message, timestamp)
using LogCallback = std::function<void(LogLevel, const char*, const std::string&, const std::string&)>;

DSE_EXPORT void LogMessage(LogLevel level, const std::string& message);
DSE_EXPORT void LogMessageCat(LogLevel level, const char* category, const std::string& message);
DSE_EXPORT void SetLogLevel(LogLevel level);
DSE_EXPORT LogLevel GetLogLevel();

/// Register a callback to receive all log messages (used by editor console).
/// Only one callback is supported; setting a new one replaces the old.
DSE_EXPORT void SetLogCallback(LogCallback cb);

/// Force flush the log file (call on crash / shutdown).
DSE_EXPORT void FlushLogFile();

/// Current log file path (for export / diagnostics).
DSE_EXPORT std::string GetLogFilePath();

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

// Legacy macros (category defaults to "General")
#define DEBUG_LOG_TRACE(...) do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Trace, "General", dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DEBUG_LOG_INFO(...)  do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Info,  "General", dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DEBUG_LOG_WARN(...)  do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Warn,  "General", dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DEBUG_LOG_ERROR(...) do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Error, "General", dse::debug::Format(__VA_ARGS__)); } } while(0)

// New category-aware macros
#define DSE_LOG_TRACE(cat, ...) do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Trace, cat, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DSE_LOG_DEBUG(cat, ...) do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Debug, cat, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DSE_LOG_INFO(cat, ...)  do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Info,  cat, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DSE_LOG_WARN(cat, ...)  do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Warn,  cat, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DSE_LOG_ERROR(cat, ...) do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Error, cat, dse::debug::Format(__VA_ARGS__)); } } while(0)
#define DSE_LOG_FATAL(cat, ...) do { if(Debug::CanLog()) { dse::debug::LogMessageCat(dse::debug::LogLevel::Fatal, cat, dse::debug::Format(__VA_ARGS__)); } } while(0)

#define __CHECK_GL_ERROR__ { \
        auto gl_error_code=glGetError();\
        if(gl_error_code!=GL_NO_ERROR){\
            DSE_LOG_ERROR("Render", "gl_error_code: {}",gl_error_code);\
        }\
    }

/**
 * @class Debug
 * @brief Static debug/logging lifecycle manager.
 */
class DSE_EXPORT Debug {
public:
    static void Init();
    static bool CanLog();
    static void ShutDown();

public:
    static bool bInited_;
};

#endif //UNTITLED_DEBUG_H
