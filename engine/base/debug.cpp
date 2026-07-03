/**
 * @file debug.cpp
 * @brief Production-grade logging: categories, timestamps, file rotation, crash handler, callback.
 */

#include "debug.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {

constexpr std::size_t kMaxLogFileSize = 10 * 1024 * 1024;  // 10 MB
constexpr int kMaxRotatedFiles = 5;

std::mutex& LogMutex() {
    static std::mutex mutex;
    return mutex;
}

std::ofstream& LogFile() {
    static std::ofstream file;
    return file;
}

std::string& LogFilePath() {
    static std::string path;
    return path;
}

std::size_t& LogFileSize() {
    static std::size_t size = 0;
    return size;
}

dse::debug::LogLevel& CurrentLogLevel() {
    static dse::debug::LogLevel level = dse::debug::LogLevel::Info;
    return level;
}

dse::debug::LogCallback& GetCallback() {
    static dse::debug::LogCallback cb;
    return cb;
}

const char* ToLabel(dse::debug::LogLevel level) {
    switch (level) {
        case dse::debug::LogLevel::Trace: return "TRACE";
        case dse::debug::LogLevel::Debug: return "DEBUG";
        case dse::debug::LogLevel::Info:  return "INFO";
        case dse::debug::LogLevel::Warn:  return "WARN";
        case dse::debug::LogLevel::Error: return "ERROR";
        case dse::debug::LogLevel::Fatal: return "FATAL";
        case dse::debug::LogLevel::Off:   return "OFF";
        default: return "UNKNOWN";
    }
}

bool ShouldLog(dse::debug::LogLevel level) {
    return level >= CurrentLogLevel() && CurrentLogLevel() != dse::debug::LogLevel::Off;
}

std::string CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &time_t_now);
#else
    localtime_r(&time_t_now, &local_tm);
#endif

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
        local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
        static_cast<int>(ms.count()));
    return buf;
}

void RotateLogFile() {
    auto& file = LogFile();
    const auto& path = LogFilePath();
    if (path.empty()) return;

    file.flush();
    file.close();

    namespace fs = std::filesystem;
    std::error_code ec;

    std::string oldest = path + "." + std::to_string(kMaxRotatedFiles);
    fs::remove(oldest, ec);

    for (int i = kMaxRotatedFiles - 1; i >= 1; --i) {
        std::string src = path + "." + std::to_string(i);
        std::string dst = path + "." + std::to_string(i + 1);
        if (fs::exists(src, ec)) {
            fs::rename(src, dst, ec);
        }
    }

    if (fs::exists(path, ec)) {
        fs::rename(path, path + ".1", ec);
    }

    file.open(path, std::ios::out | std::ios::trunc);
    LogFileSize() = 0;
}

void CrashSignalHandler(int sig) {
    auto& file = LogFile();
    if (file.is_open()) {
        file << "[FATAL] Process received signal " << sig << ", flushing logs." << std::endl;
        file.flush();
        file.close();
    }
    std::_Exit(128 + sig);
}

} // anonymous namespace

bool Debug::bInited_ = false;

namespace dse::debug {

void LogMessage(LogLevel level, const std::string& message) {
    LogMessageCat(level, "General", message);
}

void LogMessageCat(LogLevel level, const char* category, const std::string& message) {
    if (!Debug::CanLog() || !ShouldLog(level)) {
        return;
    }

    std::string ts = CurrentTimestamp();

    std::lock_guard<std::mutex> lock(LogMutex());
    std::ostringstream oss;
    oss << "[" << ts << "] [" << ToLabel(level) << "] [" << category << "] " << message;
    const std::string line = oss.str();

    if (level == LogLevel::Error || level == LogLevel::Warn || level == LogLevel::Fatal) {
        std::cerr << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }

    auto& file = LogFile();
    if (file.is_open()) {
        file << line << std::endl;
        LogFileSize() += line.size() + 1;

        if (LogFileSize() >= kMaxLogFileSize) {
            RotateLogFile();
        }
    }

    auto& cb = GetCallback();
    if (cb) {
        cb(level, category, message, ts);
    }
}

void SetLogLevel(LogLevel level) {
    CurrentLogLevel() = level;
}

LogLevel GetLogLevel() {
    return CurrentLogLevel();
}

void SetLogCallback(LogCallback cb) {
    std::lock_guard<std::mutex> lock(LogMutex());
    GetCallback() = std::move(cb);
}

void FlushLogFile() {
    std::lock_guard<std::mutex> lock(LogMutex());
    auto& file = LogFile();
    if (file.is_open()) {
        file.flush();
    }
}

std::string GetLogFilePath() {
    return LogFilePath();
}

} // namespace dse::debug

bool Debug::CanLog() {
    return bInited_;
}

void Debug::Init() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("logs", ec);

    LogFilePath() = "logs/dse_engine.log";

    auto& file = LogFile();
    if (!file.is_open()) {
        if (fs::exists(LogFilePath(), ec)) {
            auto sz = fs::file_size(LogFilePath(), ec);
            if (!ec && sz >= kMaxLogFileSize) {
                RotateLogFile();
            }
        }
        if (!file.is_open()) {
            file.open(LogFilePath(), std::ios::out | std::ios::app);
            if (file.is_open() && fs::exists(LogFilePath(), ec)) {
                LogFileSize() = static_cast<std::size_t>(fs::file_size(LogFilePath(), ec));
            }
        }
    }

    std::signal(SIGSEGV, CrashSignalHandler);
    std::signal(SIGABRT, CrashSignalHandler);
#if !defined(_WIN32)
    std::signal(SIGBUS, CrashSignalHandler);
#endif

    dse::debug::SetLogLevel(dse::debug::LogLevel::Info);
    bInited_ = true;
    DSE_LOG_INFO("Core", "Logging system initialized (file: {}, rotation: {}MB x {})",
                 LogFilePath(), kMaxLogFileSize / (1024*1024), kMaxRotatedFiles);
}

void Debug::ShutDown() {
    DSE_LOG_INFO("Core", "Logging system shutting down");
    bInited_ = false;

    std::lock_guard<std::mutex> lock(LogMutex());
    auto& file = LogFile();
    if (file.is_open()) {
        file.flush();
        file.close();
    }
}
