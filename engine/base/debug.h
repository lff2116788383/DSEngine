/**
 * @file debug.h
 * @brief Production-grade logging system with categories, level filtering, file rotation,
 *        crash-safe flush, and callback mechanism for editor integration.
 */

#ifndef UNTITLED_DEBUG_H
#define UNTITLED_DEBUG_H

#include <cstring>
#include <functional>
#include <iomanip>
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

/// 将单个值按 std::format 风格的子集 spec 写出。支持形如 `[0][width][.prec][type]`，
/// type ∈ {x,X 十六进制；f/F 定点小数；其余按默认}。spec 为空时等价于 `{}` 直接输出。
template <typename T>
void AppendValueWithSpec(std::ostringstream& oss, const char* spec, std::size_t spec_len, T&& value) {
    if (spec_len == 0) {
        oss << std::forward<T>(value);
        return;
    }
    std::ostringstream vs;
    std::size_t i = 0;
    bool zero_fill = false;
    if (i < spec_len && spec[i] == '0') { zero_fill = true; ++i; }
    int width = 0; bool has_width = false;
    while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') { has_width = true; width = width * 10 + (spec[i] - '0'); ++i; }
    int prec = -1;
    if (i < spec_len && spec[i] == '.') {
        ++i; prec = 0;
        while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') { prec = prec * 10 + (spec[i] - '0'); ++i; }
    }
    char type = (i < spec_len) ? spec[i] : '\0';
    if (type == 'x') { vs << std::hex << std::nouppercase; }
    else if (type == 'X') { vs << std::hex << std::uppercase; }
    else if (type == 'f' || type == 'F') { vs << std::fixed; if (prec >= 0) vs << std::setprecision(prec); }
    if (zero_fill) { vs << std::setfill('0'); }
    if (has_width) { vs << std::setw(width); }
    vs << std::forward<T>(value);
    oss << vs.str();
}

template <typename T, typename... Rest>
void AppendFormatted(std::ostringstream& oss, const char* format, T&& value, Rest&&... rest) {
    if (!format) {
        return;
    }

    // 仅将 `{}` 与 `{:spec}` 识别为占位符；其它 `{` 视为字面量，避免误吞日志中形如
    // `settings{gpu_driven=...}` 的字面花括号。
    for (const char* p = format; *p; ++p) {
        if (p[0] != '{') {
            continue;
        }
        if (p[1] == '}') {
            oss.write(format, static_cast<std::streamsize>(p - format));
            AppendValueWithSpec(oss, nullptr, 0, std::forward<T>(value));
            AppendFormatted(oss, p + 2, std::forward<Rest>(rest)...);
            return;
        }
        if (p[1] == ':') {
            const char* close = std::strchr(p + 2, '}');
            if (close) {
                oss.write(format, static_cast<std::streamsize>(p - format));
                AppendValueWithSpec(oss, p + 2, static_cast<std::size_t>(close - (p + 2)), std::forward<T>(value));
                AppendFormatted(oss, close + 1, std::forward<Rest>(rest)...);
                return;
            }
        }
    }

    // 没有占位符可消费：原样输出（多余实参被忽略）。
    oss << format;
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
