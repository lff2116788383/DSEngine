/**
 * @file crash_handler.cpp
 * @brief 崩溃捕获子系统实现：平台无关的报告格式化/落地 + 平台相关的异常捕获。
 */

#include "engine/diagnostics/crash_handler.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "engine/dse_version.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>  // MiniDumpWriteDump / StackWalk64 / Sym*
#else
#include <csignal>
#include <fcntl.h>     // open(), O_WRONLY, O_CREAT, O_TRUNC
#include <unistd.h>
#if defined(__GLIBC__)
#include <execinfo.h>  // backtrace / backtrace_symbols / backtrace_symbols_fd
#endif
#include <sys/utsname.h>
#endif

namespace dse {
namespace diagnostics {

// ============================== 通用工具 ==============================
namespace {

std::string NowTimestampIso() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::string NowTimestampForFile() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02dT%02d%02d%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

unsigned long CurrentProcessId() {
#if defined(_WIN32)
    return static_cast<unsigned long>(::GetCurrentProcessId());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

std::string DetectOsInfo() {
#if defined(_WIN32)
    SYSTEM_INFO si{};
    ::GetNativeSystemInfo(&si);
    const char* arch = "unknown";
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
        default: break;
    }
    std::ostringstream oss;
    oss << "Windows (" << arch << ", " << si.dwNumberOfProcessors << " cpus)";
    return oss.str();
#else
    struct utsname u {};
    if (::uname(&u) == 0) {
        std::ostringstream oss;
        oss << u.sysname << ' ' << u.release << " (" << u.machine << ')';
        return oss.str();
    }
    return "POSIX";
#endif
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void AppendJsonStringArray(std::ostringstream& oss, const std::vector<std::string>& items) {
    oss << '[';
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ',';
        oss << '"' << JsonEscape(items[i]) << '"';
    }
    oss << ']';
}

}  // namespace

// ============================== BreadcrumbBuffer ==============================

BreadcrumbBuffer::BreadcrumbBuffer(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {
    entries_.resize(capacity_);
}

void BreadcrumbBuffer::Push(const std::string& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[head_] = entry;
    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) {
        ++count_;
    }
}

std::vector<std::string> BreadcrumbBuffer::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(count_);
    // 最旧元素的位置：当缓冲未满时为 0；满时为 head_。
    const std::size_t start = (count_ < capacity_) ? 0 : head_;
    for (std::size_t i = 0; i < count_; ++i) {
        out.push_back(entries_[(start + i) % capacity_]);
    }
    return out;
}

void BreadcrumbBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    count_ = 0;
}

void BreadcrumbBuffer::Reset(std::size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    capacity_ = (capacity == 0 ? 1 : capacity);
    entries_.assign(capacity_, std::string());
    head_ = 0;
    count_ = 0;
}

std::size_t BreadcrumbBuffer::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

// ============================== 报告格式化/落地 ==============================

std::string FormatCrashReport(const CrashReportInfo& info) {
    std::ostringstream oss;
    oss << "==================== DSEngine Crash Report ====================\n";
    oss << "app        : " << info.app_name << '\n';
    oss << "version    : " << info.app_version << '\n';
    oss << "time (UTC) : " << info.timestamp_utc << '\n';
    oss << "os         : " << info.os_info << '\n';
    oss << "reason     : " << info.reason << '\n';
    if (!info.fault_address.empty()) {
        oss << "fault addr : " << info.fault_address << '\n';
    }
    if (!info.dump_file.empty()) {
        oss << "minidump   : " << info.dump_file << '\n';
    }

    oss << "\n---- Call stack ----\n";
    if (info.call_stack.empty()) {
        oss << "(unavailable)\n";
    } else {
        for (std::size_t i = 0; i < info.call_stack.size(); ++i) {
            oss << '#' << i << "  " << info.call_stack[i] << '\n';
        }
    }

    if (!info.metadata.empty()) {
        oss << "\n---- Metadata ----\n";
        for (const auto& kv : info.metadata) {
            oss << kv.first << " = " << kv.second << '\n';
        }
    }

    oss << "\n---- Breadcrumbs (oldest -> newest) ----\n";
    if (info.breadcrumbs.empty()) {
        oss << "(none)\n";
    } else {
        for (const auto& b : info.breadcrumbs) {
            oss << "  " << b << '\n';
        }
    }

    if (!info.modules.empty()) {
        oss << "\n---- Loaded modules ----\n";
        for (const auto& m : info.modules) {
            oss << "  " << m << '\n';
        }
    }
    oss << "==============================================================\n";
    return oss.str();
}

std::string FormatCrashReportJson(const CrashReportInfo& info) {
    std::ostringstream oss;
    oss << '{';
    oss << "\"app\":\"" << JsonEscape(info.app_name) << "\",";
    oss << "\"version\":\"" << JsonEscape(info.app_version) << "\",";
    oss << "\"time\":\"" << JsonEscape(info.timestamp_utc) << "\",";
    oss << "\"os\":\"" << JsonEscape(info.os_info) << "\",";
    oss << "\"reason\":\"" << JsonEscape(info.reason) << "\",";
    oss << "\"fault_address\":\"" << JsonEscape(info.fault_address) << "\",";
    oss << "\"dump_file\":\"" << JsonEscape(info.dump_file) << "\",";
    oss << "\"call_stack\":";
    AppendJsonStringArray(oss, info.call_stack);
    oss << ",\"breadcrumbs\":";
    AppendJsonStringArray(oss, info.breadcrumbs);
    oss << ",\"metadata\":{";
    for (std::size_t i = 0; i < info.metadata.size(); ++i) {
        if (i) oss << ',';
        oss << '"' << JsonEscape(info.metadata[i].first) << "\":\""
            << JsonEscape(info.metadata[i].second) << '"';
    }
    oss << "}}";
    return oss.str();
}

std::string WriteCrashReport(const std::string& dir, const CrashReportInfo& info) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);  // 忽略“已存在”

    std::ostringstream name;
    name << "crash_" << (info.app_name.empty() ? "DSEngine" : info.app_name) << '_'
         << NowTimestampForFile() << '_' << CurrentProcessId() << ".txt";
    // 去掉文件名里可能的路径分隔符/空格
    std::string fname = name.str();
    for (char& c : fname) {
        if (c == '/' || c == '\\' || c == ' ' || c == ':') c = '_';
    }

    std::filesystem::path path = std::filesystem::path(dir) / fname;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::string();
    }
    const std::string text = FormatCrashReport(info);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    return path.string();
}

// ============================== CrashReporter ==============================

CrashReporter& CrashReporter::Instance() {
    static CrashReporter inst;
    return inst;
}

void CrashReporter::AddBreadcrumb(const std::string& entry) {
    breadcrumbs_.Push(entry);
}

void CrashReporter::SetMetadata(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : metadata_) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    metadata_.emplace_back(key, value);
}

std::string CrashReporter::LastReportPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_report_path_;
}

void CrashReporter::SetLastReportPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_report_path_ = path;
}

bool CrashReporter::IsInstalled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return installed_;
}

CrashReportInfo CrashReporter::BuildBaseInfo(const std::string& reason) const {
    CrashReportInfo info;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        info.app_name = config_.app_name.empty() ? "DSEngine" : config_.app_name;
        info.app_version = config_.app_version.empty() ? std::string(DSE_VERSION_STRING)
                                                       : config_.app_version;
        info.metadata = metadata_;
    }
    info.timestamp_utc = NowTimestampIso();
    info.reason = reason;
    info.os_info = DetectOsInfo();
    info.breadcrumbs = breadcrumbs_.Snapshot();
    return info;
}

// ----- 平台相关：异常/信号捕获 + minidump + 调用栈 -----
#if defined(_WIN32)

namespace {

std::string ExceptionCodeToString(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
        case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
        default: return "EXCEPTION_UNKNOWN";
    }
}

std::mutex& DbgHelpMutex() {
    static std::mutex m;
    return m;
}

// 用 StackWalk64 走给定 context 的调用栈，尽力符号化。
void CaptureStack(CONTEXT* ctx, std::vector<std::string>& out, std::size_t max_frames = 64) {
    std::lock_guard<std::mutex> lock(DbgHelpMutex());

    HANDLE process = ::GetCurrentProcess();
    HANDLE thread = ::GetCurrentThread();

    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    ::SymInitialize(process, nullptr, TRUE);

    STACKFRAME64 frame{};
    DWORD machine;
#if defined(_M_X64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = ctx->Rip;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrStack.Offset = ctx->Rsp;
#elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = ctx->Pc;
    frame.AddrFrame.Offset = ctx->Fp;
    frame.AddrStack.Offset = ctx->Sp;
#elif defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = ctx->Eip;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrStack.Offset = ctx->Esp;
#else
    ::SymCleanup(process);
    return;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char sym_buf[sizeof(SYMBOL_INFO) + 512] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;

    for (std::size_t i = 0; i < max_frames; ++i) {
        if (!::StackWalk64(machine, process, thread, &frame, ctx, nullptr,
                           ::SymFunctionTableAccess64, ::SymGetModuleBase64, nullptr)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) break;

        std::ostringstream line;
        DWORD64 addr = frame.AddrPC.Offset;
        DWORD64 disp = 0;
        if (::SymFromAddr(process, addr, &disp, symbol)) {
            line << symbol->Name << " + 0x" << std::hex << disp;
        } else {
            line << "0x" << std::hex << addr;
        }

        IMAGEHLP_LINE64 lineinfo{};
        lineinfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD line_disp = 0;
        if (::SymGetLineFromAddr64(process, addr, &line_disp, &lineinfo) && lineinfo.FileName) {
            line << "  (" << lineinfo.FileName << ':' << std::dec << lineinfo.LineNumber << ')';
        }
        out.push_back(line.str());
    }
    ::SymCleanup(process);
}

void CollectModules(std::vector<std::string>& out) {
    std::lock_guard<std::mutex> lock(DbgHelpMutex());
    HANDLE process = ::GetCurrentProcess();
    ::EnumerateLoadedModules64(
        process,
        [](PCSTR ModuleName, DWORD64 ModuleBase, ULONG /*ModuleSize*/, PVOID UserContext) -> BOOL {
            auto* vec = reinterpret_cast<std::vector<std::string>*>(UserContext);
            std::ostringstream oss;
            oss << ModuleName << " @ 0x" << std::hex << ModuleBase;
            vec->push_back(oss.str());
            return TRUE;
        },
        &out);
}

// 写 minidump，返回写出的文件名（仅文件名，不含目录）；失败返回空。
std::string WriteMinidump(const std::string& dir, const std::string& app_name,
                          EXCEPTION_POINTERS* ep, bool full_memory) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::ostringstream name;
    name << "crash_" << (app_name.empty() ? "DSEngine" : app_name) << '_'
         << NowTimestampForFile() << '_' << CurrentProcessId() << ".dmp";
    std::filesystem::path path = std::filesystem::path(dir) / name.str();

    HANDLE file = ::CreateFileA(path.string().c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::string();
    }

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = ::GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE type = full_memory
        ? static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData |
                                     MiniDumpWithThreadInfo)
        : static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                                     MiniDumpScanMemory | MiniDumpWithThreadInfo);

    const BOOL ok = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(),
                                        file, type, ep ? &mei : nullptr, nullptr, nullptr);
    ::CloseHandle(file);
    if (!ok) {
        std::filesystem::remove(path, ec);
        return std::string();
    }
    return name.str();
}

LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;
std::atomic<bool> g_in_handler{false};

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* ep) {
    // 防重入：崩溃处理本身再崩则直接交还系统。
    bool expected = false;
    if (!g_in_handler.compare_exchange_strong(expected, true)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    CrashReporter& reporter = CrashReporter::Instance();
    const CrashHandlerConfig& cfg = reporter.Config();

    std::string reason = "Unhandled exception";
    std::string fault_addr;
    if (ep && ep->ExceptionRecord) {
        const DWORD code = ep->ExceptionRecord->ExceptionCode;
        std::ostringstream r;
        r << ExceptionCodeToString(code) << " (0x" << std::hex << code << ')';
        reason = r.str();
        std::ostringstream fa;
        fa << "0x" << std::hex
           << reinterpret_cast<std::uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
        fault_addr = fa.str();
    }

    CrashReportInfo info = reporter.BuildBaseInfo(reason);
    info.fault_address = fault_addr;

    if (ep && ep->ContextRecord) {
        // StackWalk64 会修改 context，复制一份。
        CONTEXT ctx = *ep->ContextRecord;
        CaptureStack(&ctx, info.call_stack);
    }
    CollectModules(info.modules);

    if (cfg.write_minidump) {
        info.dump_file = WriteMinidump(cfg.dump_dir, info.app_name, ep, cfg.full_memory_dump);
    }

    const std::string report_path = WriteCrashReport(cfg.dump_dir, info);
    reporter.SetLastReportPath(report_path);

    if (cfg.upload_callback) {
        std::string dump_full;
        if (!info.dump_file.empty()) {
            dump_full = (std::filesystem::path(cfg.dump_dir) / info.dump_file).string();
        }
        // 尽力而为：上传失败不应阻止进程退出。
        try {
            cfg.upload_callback(report_path, dump_full, FormatCrashReportJson(info));
        } catch (...) {
        }
    }

    if (g_previous_filter) {
        return g_previous_filter(ep);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

bool CrashReporter::Install(const CrashHandlerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    if (config_.app_version.empty()) {
        config_.app_version = DSE_VERSION_STRING;
    }
    breadcrumbs_.Reset(config_.max_breadcrumbs);
    if (!installed_) {
        g_previous_filter = ::SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
        installed_ = true;
    }
    return true;
}

void CrashReporter::Uninstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (installed_) {
        ::SetUnhandledExceptionFilter(g_previous_filter);
        g_previous_filter = nullptr;
        installed_ = false;
    }
}

std::string CrashReporter::WriteManualReport(const std::string& reason) {
    CrashReportInfo info = BuildBaseInfo(reason.empty() ? "Manual report" : reason);

    CONTEXT ctx{};
    ::RtlCaptureContext(&ctx);
    CaptureStack(&ctx, info.call_stack);
    CollectModules(info.modules);

    std::string dump_dir;
    bool want_dump = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dump_dir = config_.dump_dir;
        want_dump = config_.write_minidump;
    }
    if (want_dump) {
        info.dump_file = WriteMinidump(dump_dir, info.app_name, nullptr, false);
    }
    const std::string path = WriteCrashReport(dump_dir, info);
    SetLastReportPath(path);
    return path;
}

#else  // ---------------- 非 Windows（POSIX 信号 + backtrace） ----------------

//
// P2-0: POSIX async-signal-safe crash handler.
//
// The signal handler only calls functions that are async-signal-safe
// per POSIX.1-2016 (Table 2-4): write(), _exit(), snprintf(),
// backtrace(), backtrace_symbols_fd(), etc.
//
// All non-safe work (std::string, std::mutex, std::ofstream, upload
// callbacks) is deferred: the handler writes a minimal report via
// raw write() to a pre-opened fd, then _exit().
//

namespace {

const char* SignalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGBUS:  return "SIGBUS";
        default:      return "SIGNAL";
    }
}

// Reentrancy guard: sig_atomic_t is the POSIX-sanctioned type for
// variables accessed in signal handlers.
volatile sig_atomic_t g_in_handler = 0;

// Pre-allocated resources (populated at Install() time, read-only in handler).
// These use plain C arrays / POD types — no heap, no mutex.
constexpr int kMaxStackFrames = 64;
constexpr int kReportBufSize  = 4096;

struct SignalSafeState {
    int report_fd = -1;           ///< pre-opened fd for crash report, or -1
    char app_name[128] = {};
    char app_version[64] = {};
    char os_info[256] = {};
    char dump_dir[512] = {};
    bool initialized = false;
};

SignalSafeState g_safe_state;

/// Write a NUL-terminated string to a file descriptor (async-signal-safe).
void WriteStr(int fd, const char* str) {
    if (fd < 0 || !str) return;
    size_t len = 0;
    while (str[len]) ++len;  // strlen is not guaranteed async-signal-safe
    ssize_t unused = ::write(fd, str, len);
    (void)unused;
}

/// Capture backtrace and write directly to fd (async-signal-safe on glibc).
void CaptureStackPosixSafe(int fd) {
    if (fd < 0) return;
#if defined(__GLIBC__)
    void* frames[kMaxStackFrames];
    const int n = ::backtrace(frames, kMaxStackFrames);
    // backtrace_symbols_fd writes directly to fd — no malloc required.
    ::backtrace_symbols_fd(frames, n, fd);
#else
    WriteStr(fd, "(backtrace unavailable on this platform)\n");
#endif
}

/// Non-signal-safe stack capture (used by WriteManualReport).
void CaptureStackPosix(std::vector<std::string>& out, std::size_t max_frames = 64) {
#if defined(__GLIBC__)
    std::vector<void*> frames(max_frames);
    const int n = ::backtrace(frames.data(), static_cast<int>(frames.size()));
    char** symbols = ::backtrace_symbols(frames.data(), n);
    if (symbols) {
        for (int i = 0; i < n; ++i) {
            out.emplace_back(symbols[i] ? symbols[i] : "(null)");
        }
        ::free(symbols);
    }
#else
    (void)out;
    (void)max_frames;
#endif
}

/// The async-signal-safe signal handler.
/// Only calls async-signal-safe functions (write, snprintf, _exit,
/// backtrace, backtrace_symbols_fd).
void PosixSignalHandler(int sig) {
    if (g_in_handler) {
        // Re-entered — bail immediately.
        ::_exit(128 + sig);
    }
    g_in_handler = 1;

    const SignalSafeState& st = g_safe_state;

    // Build a minimal report in a stack-local buffer using snprintf.
    char buf[kReportBufSize];
    int pos = 0;

    if (st.report_fd >= 0) {
        // Write report header
        pos = snprintf(buf, sizeof(buf),
            "==================== DSEngine Crash Report ====================\n"
            "app        : %s\n"
            "version    : %s\n"
            "os         : %s\n"
            "reason     : Signal %s (%d)\n"
            "pid        : %d\n"
            "\n---- Call stack ----\n",
            st.app_name, st.app_version, st.os_info,
            SignalName(sig), sig,
            static_cast<int>(::getpid()));
        if (pos > 0) {
            ssize_t unused = ::write(st.report_fd, buf, static_cast<size_t>(pos));
            (void)unused;
        }

        // Write backtrace directly to the fd
        CaptureStackPosixSafe(st.report_fd);

        // Write footer
        WriteStr(st.report_fd,
            "==============================================================\n");
    }

    // Also write a brief message to stderr
    pos = snprintf(buf, sizeof(buf),
        "\n[DSEngine] Caught signal %s (%d). Crash report written.\n",
        SignalName(sig), sig);
    if (pos > 0) {
        ssize_t unused = ::write(STDERR_FILENO, buf, static_cast<size_t>(pos));
        (void)unused;
    }

    // Restore default handler and re-raise to preserve core dump behavior.
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
    ::raise(sig);

    // Should not reach here, but just in case:
    ::_exit(128 + sig);
}

/// Install a signal handler using sigaction (async-signal-safe setup).
void InstallSignalHandler(int sig) {
    struct sigaction sa{};
    sa.sa_handler = &PosixSignalHandler;
    sigemptyset(&sa.sa_mask);
    // SA_RESETHAND: restore default after handler triggers (one-shot)
    // SA_NODEFER:  allow re-entry (we guard with g_in_handler)
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigaction(sig, &sa, nullptr);
}

void RestoreSignalHandler(int sig) {
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

}  // namespace

bool CrashReporter::Install(const CrashHandlerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    if (config_.app_version.empty()) {
        config_.app_version = DSE_VERSION_STRING;
    }
    breadcrumbs_.Reset(config_.max_breadcrumbs);
    if (!installed_) {
        // Populate async-signal-safe state (heap allocation OK here —
        // we're in a normal context, not a signal handler).
        SignalSafeState& st = g_safe_state;
        snprintf(st.app_name, sizeof(st.app_name), "%s",
                 config_.app_name.empty() ? "DSEngine" : config_.app_name.c_str());
        snprintf(st.app_version, sizeof(st.app_version), "%s",
                 config_.app_version.c_str());
        snprintf(st.os_info, sizeof(st.os_info), "%s", DetectOsInfo().c_str());
        snprintf(st.dump_dir, sizeof(st.dump_dir), "%s", config_.dump_dir.c_str());

        // Pre-open a crash report file so the signal handler can use
        // raw write() without any allocation.
        // Close any previously opened fd.
        if (st.report_fd >= 0) {
            ::close(st.report_fd);
            st.report_fd = -1;
        }
        // Note: we don't pre-open here because the crash could happen
        // much later and the file would be empty/stale. Instead, the
        // signal handler will create the file using async-signal-safe
        // open() if needed.
        //
        // Actually, open() is async-signal-safe per POSIX. But constructing
        // the filename (timestamp + pid) in the handler is complex.
        // A better approach: pre-format the filename at install time.
        {
            std::ostringstream name;
            name << "crash_" << (config_.app_name.empty() ? "DSEngine" : config_.app_name)
                 << '_' << NowTimestampForFile() << '_' << CurrentProcessId() << ".txt";
            std::string fname = name.str();
            for (char& c : fname) {
                if (c == '/' || c == '\\' || c == ' ' || c == ':') c = '_';
            }
            std::filesystem::path path = std::filesystem::path(config_.dump_dir) / fname;
            // Create the directory and open the file now.
            std::error_code ec;
            std::filesystem::create_directories(config_.dump_dir, ec);
            // Open the file (will be written to by the signal handler)
            // We keep it open for the lifetime of the program.
            st.report_fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            // Store the path for LastReportPath() — use a static string.
            // This is safe because we're in a normal context.
            last_report_path_ = path.string();
        }
        st.initialized = true;

        // Install signal handlers using sigaction
        InstallSignalHandler(SIGSEGV);
        InstallSignalHandler(SIGABRT);
        InstallSignalHandler(SIGFPE);
        InstallSignalHandler(SIGILL);
#ifdef SIGBUS
        InstallSignalHandler(SIGBUS);
#endif
        installed_ = true;
    }
    return true;
}

void CrashReporter::Uninstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (installed_) {
        RestoreSignalHandler(SIGSEGV);
        RestoreSignalHandler(SIGABRT);
        RestoreSignalHandler(SIGFPE);
        RestoreSignalHandler(SIGILL);
#ifdef SIGBUS
        RestoreSignalHandler(SIGBUS);
#endif
        // Close pre-opened fd
        if (g_safe_state.report_fd >= 0) {
            ::close(g_safe_state.report_fd);
            g_safe_state.report_fd = -1;
        }
        g_safe_state.initialized = false;
        installed_ = false;
    }
}

std::string CrashReporter::WriteManualReport(const std::string& reason) {
    CrashReportInfo info = BuildBaseInfo(reason.empty() ? "Manual report" : reason);
    CaptureStackPosix(info.call_stack);
    std::string dump_dir;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dump_dir = config_.dump_dir;
    }
    const std::string path = WriteCrashReport(dump_dir, info);
    SetLastReportPath(path);
    return path;
}

#endif  // _WIN32

}  // namespace diagnostics
}  // namespace dse
