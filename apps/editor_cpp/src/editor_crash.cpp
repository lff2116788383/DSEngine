#include "editor_crash.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "engine/diagnostics/crash_handler.h"

namespace dse::editor {

namespace {

constexpr const char* kEditorAppName = "DSEngine-Editor";
constexpr const char* kDefaultCrashSubdir = "crashes/editor";
constexpr const char* kReportPrefix = "crash_DSEngine-Editor_";

bool g_installed = false;
bool g_snapshot_taken = false;
std::string g_previous_crash_report;

bool CrashHandlerDisabledByEnv() {
    const char* v = std::getenv("DSE_CRASH_HANDLER");
    return v != nullptr && std::string(v) == "0";
}

bool IsEditorCrashReportName(const std::string& filename) {
    if (filename.rfind(kReportPrefix, 0) != 0) return false;  // 前缀
    return filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".txt") == 0;
}

}  // namespace

std::string GetEditorCrashDir() {
    if (const char* dir = std::getenv("DSE_CRASH_DIR")) {
        if (dir[0] != '\0') return std::string(dir);
    }
    return std::string(kDefaultCrashSubdir);
}

std::string FindLatestCrashReportInDir(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (dir.empty() || !fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return {};

    std::string best;
    fs::file_time_type best_time{};
    for (fs::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        std::error_code fec;
        if (!it->is_regular_file(fec) || fec) continue;
        const std::string name = it->path().filename().string();
        if (!IsEditorCrashReportName(name)) continue;
        std::error_code tec;
        const auto t = fs::last_write_time(it->path(), tec);
        if (tec) continue;
        if (best.empty() || t > best_time) {
            best_time = t;
            best = it->path().string();
        }
    }
    return best;
}

std::string FindLatestEditorCrashReport() {
    return FindLatestCrashReportInDir(GetEditorCrashDir());
}

std::string GetPreviousSessionCrashReport() {
    return g_previous_crash_report;
}

bool IsEditorCrashHandlerInstalled() {
    return g_installed;
}

void InstallEditorCrashHandler() {
    using dse::diagnostics::CrashHandlerConfig;
    using dse::diagnostics::CrashReporter;

    // 仅在“本会话尚未写入任何报告”之前快照一次上次遗留的崩溃报告。
    if (!g_snapshot_taken) {
        g_previous_crash_report = FindLatestEditorCrashReport();
        g_snapshot_taken = true;
    }

    if (CrashHandlerDisabledByEnv()) {
        g_installed = false;
        return;
    }

    CrashHandlerConfig cfg;
    cfg.app_name = kEditorAppName;
    cfg.dump_dir = GetEditorCrashDir();
    cfg.write_minidump = true;

    // Install 为“后者覆盖”语义且会重置面包屑：此处的二次调用用于在引擎 Init 覆盖后
    // 重新夺回编辑器身份。重置后面包屑由调用方在其后重新补充。
    g_installed = CrashReporter::Instance().Install(cfg);
    if (g_installed) {
        CrashReporter::Instance().SetMetadata("process", "editor");
    }
}

void ResetEditorCrashHandlerForTesting() {
    dse::diagnostics::CrashReporter::Instance().Uninstall();
    g_installed = false;
    g_snapshot_taken = false;
    g_previous_crash_report.clear();
}

void AddEditorBreadcrumb(const std::string& entry) {
    if (!g_installed) return;
    dse::diagnostics::CrashReporter::Instance().AddBreadcrumb(entry);
}

void SetEditorCrashMetadata(const std::string& key, const std::string& value) {
    if (!g_installed) return;
    dse::diagnostics::CrashReporter::Instance().SetMetadata(key, value);
}

}  // namespace dse::editor
