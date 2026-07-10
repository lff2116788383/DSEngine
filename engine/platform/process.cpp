#include "engine/platform/process.h"

#include "engine/base/debug.h"

#include <array>
#include <cstring>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <poll.h>
#  include <spawn.h>
#  include <sys/wait.h>
#  include <unistd.h>
extern char** environ;
#endif

namespace dse::platform {

namespace {
constexpr const char* kCat = "Process";

// Splits an accumulating buffer into complete lines (delimited by '\n', with a
// trailing '\r' stripped) and forwards them to the sink. Any incomplete tail is
// retained in `buffer` for the next chunk.
void PumpLines(std::string& buffer, bool is_stderr, const ProcessOutputFn& sink) {
    if (!sink) { buffer.clear(); return; }
    size_t start = 0;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '\n') {
            size_t len = i - start;
            if (len > 0 && buffer[start + len - 1] == '\r') --len;
            sink(std::string_view(buffer).substr(start, len), is_stderr);
            start = i + 1;
        }
    }
    buffer.erase(0, start);
}

void FlushTail(std::string& buffer, bool is_stderr, const ProcessOutputFn& sink) {
    if (!buffer.empty() && sink) {
        size_t len = buffer.size();
        if (buffer[len - 1] == '\r') --len;
        sink(std::string_view(buffer).substr(0, len), is_stderr);
    }
    buffer.clear();
}
} // namespace

#if defined(_WIN32)

namespace {
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Quote a single argument per the Windows CommandLineToArgvW convention.
void AppendQuotedArg(std::wstring& cmd, const std::wstring& arg) {
    if (!arg.empty() &&
        arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        cmd += arg;
        return;
    }
    cmd += L'"';
    for (auto it = arg.begin();; ++it) {
        unsigned backslashes = 0;
        while (it != arg.end() && *it == L'\\') { ++it; ++backslashes; }
        if (it == arg.end()) {
            cmd.append(backslashes * 2, L'\\');
            break;
        } else if (*it == L'"') {
            cmd.append(backslashes * 2 + 1, L'\\');
            cmd += *it;
        } else {
            cmd.append(backslashes, L'\\');
            cmd += *it;
        }
    }
    cmd += L'"';
}

// Build a UTF-16 environment block (double-null terminated).
std::wstring BuildEnvBlock(const ProcessOptions& opts) {
    std::vector<std::pair<std::wstring, std::wstring>> vars;
    auto upsert = [&vars](const std::wstring& key, const std::wstring& val) {
        for (auto& kv : vars) {
            if (_wcsicmp(kv.first.c_str(), key.c_str()) == 0) { kv.second = val; return; }
        }
        vars.emplace_back(key, val);
    };
    if (opts.inherit_env) {
        if (LPWCH env = ::GetEnvironmentStringsW()) {
            for (LPWCH p = env; *p;) {
                std::wstring entry = p;
                p += entry.size() + 1;
                // Skip drive-letter cwd entries like "=C:=C:\..."
                size_t eq = entry.find(L'=', entry.front() == L'=' ? 1 : 0);
                if (eq == std::wstring::npos) continue;
                upsert(entry.substr(0, eq), entry.substr(eq + 1));
            }
            ::FreeEnvironmentStringsW(env);
        }
    }
    for (auto& kv : opts.env) upsert(Utf8ToWide(kv.first), Utf8ToWide(kv.second));

    std::wstring block;
    for (auto& kv : vars) {
        block += kv.first;
        block += L'=';
        block += kv.second;
        block += L'\0';
    }
    block += L'\0';
    return block;
}
} // namespace

ProcessResult RunProcess(const ProcessOptions& opts, const ProcessOutputFn& on_output,
                         std::chrono::milliseconds timeout, const std::atomic<bool>* cancel) {
    ProcessResult result;
    if (opts.executable.empty()) { result.error = "executable is empty"; return result; }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE out_r = nullptr, out_w = nullptr, err_r = nullptr, err_w = nullptr;
    auto close_all = [&]() {
        for (HANDLE* h : {&out_r, &out_w, &err_r, &err_w}) {
            if (*h) { ::CloseHandle(*h); *h = nullptr; }
        }
    };
    if (!::CreatePipe(&out_r, &out_w, &sa, 0)) { result.error = "CreatePipe(stdout) failed"; return result; }
    ::SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    if (!opts.merge_stderr) {
        if (!::CreatePipe(&err_r, &err_w, &sa, 0)) { close_all(); result.error = "CreatePipe(stderr) failed"; return result; }
        ::SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    }

    // Job object so cancel/timeout terminates the whole child process tree.
    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji{};
        ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji));
    }

    std::wstring cmd = L"";
    AppendQuotedArg(cmd, Utf8ToWide(opts.executable));
    for (auto& a : opts.args) { cmd += L' '; AppendQuotedArg(cmd, Utf8ToWide(a)); }

    std::wstring env_block = BuildEnvBlock(opts);
    std::wstring wdir = opts.working_dir.empty() ? std::wstring() : opts.working_dir.wstring();

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = out_w;
    si.hStdError  = opts.merge_stderr ? out_w : err_w;
    si.hStdInput  = ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd_mutable(cmd.begin(), cmd.end());
    cmd_mutable.push_back(L'\0');

    BOOL ok = ::CreateProcessW(
        nullptr, cmd_mutable.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
        env_block.data(),
        wdir.empty() ? nullptr : wdir.c_str(),
        &si, &pi);

    // Parent no longer needs the write ends.
    if (out_w) { ::CloseHandle(out_w); out_w = nullptr; }
    if (err_w) { ::CloseHandle(err_w); err_w = nullptr; }

    if (!ok) {
        DWORD e = ::GetLastError();
        result.error = "CreateProcess failed (error " + std::to_string(e) + ") for '" + opts.executable + "'";
        close_all();
        if (job) ::CloseHandle(job);
        DSE_LOG_ERROR(kCat, "{}", result.error);
        return result;
    }

    if (job) ::AssignProcessToJobObject(job, pi.hProcess);
    ::ResumeThread(pi.hThread);

    std::mutex sink_mtx;
    auto reader = [&](HANDLE h, bool is_stderr) {
        std::string buf;
        std::array<char, 4096> chunk;
        DWORD read = 0;
        while (h && ::ReadFile(h, chunk.data(), (DWORD)chunk.size(), &read, nullptr) && read > 0) {
            buf.append(chunk.data(), read);
            std::lock_guard<std::mutex> lk(sink_mtx);
            PumpLines(buf, is_stderr, on_output);
        }
        std::lock_guard<std::mutex> lk(sink_mtx);
        FlushTail(buf, is_stderr, on_output);
    };

    std::thread out_thread(reader, out_r, false);
    std::thread err_thread;
    if (!opts.merge_stderr) err_thread = std::thread(reader, err_r, true);

    const auto start = std::chrono::steady_clock::now();
    bool killed = false;
    for (;;) {
        DWORD wr = ::WaitForSingleObject(pi.hProcess, 50);
        if (wr == WAIT_OBJECT_0) break;
        if (cancel && cancel->load()) { result.canceled = true; killed = true; }
        if (timeout.count() > 0 &&
            std::chrono::steady_clock::now() - start >= timeout) { result.timed_out = true; killed = true; }
        if (killed) {
            if (job) ::TerminateJobObject(job, 1);
            else ::TerminateProcess(pi.hProcess, 1);
            ::WaitForSingleObject(pi.hProcess, INFINITE);
            break;
        }
    }

    if (out_thread.joinable()) out_thread.join();
    if (err_thread.joinable()) err_thread.join();

    DWORD code = 0;
    if (::GetExitCodeProcess(pi.hProcess, &code)) result.exit_code = (int)code;
    result.launched = true;

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    close_all();
    if (job) ::CloseHandle(job);
    return result;
}

#else // POSIX

ProcessResult RunProcess(const ProcessOptions& opts, const ProcessOutputFn& on_output,
                         std::chrono::milliseconds timeout, const std::atomic<bool>* cancel) {
    ProcessResult result;
    if (opts.executable.empty()) { result.error = "executable is empty"; return result; }

    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    if (::pipe(out_pipe) != 0) { result.error = "pipe(stdout) failed"; return result; }
    if (!opts.merge_stderr && ::pipe(err_pipe) != 0) {
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        result.error = "pipe(stderr) failed"; return result;
    }

    std::vector<std::string> arg_store;
    arg_store.reserve(opts.args.size() + 1);
    arg_store.push_back(opts.executable);
    for (auto& a : opts.args) arg_store.push_back(a);
    std::vector<char*> argv;
    for (auto& s : arg_store) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    // Build environment (inherit + overrides).
    std::vector<std::string> env_store;
    if (opts.inherit_env) {
        for (char** e = environ; e && *e; ++e) env_store.emplace_back(*e);
    }
    for (auto& kv : opts.env) {
        std::string prefix = kv.first + "=";
        bool replaced = false;
        for (auto& e : env_store) {
            if (e.rfind(prefix, 0) == 0) { e = prefix + kv.second; replaced = true; break; }
        }
        if (!replaced) env_store.push_back(prefix + kv.second);
    }
    std::vector<char*> envp;
    for (auto& s : env_store) envp.push_back(const_cast<char*>(s.c_str()));
    envp.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        result.error = "fork failed";
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        if (!opts.merge_stderr) { ::close(err_pipe[0]); ::close(err_pipe[1]); }
        return result;
    }
    if (pid == 0) {
        // Child: own process group so we can kill the whole tree.
        ::setpgid(0, 0);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(opts.merge_stderr ? out_pipe[1] : err_pipe[1], STDERR_FILENO);
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        if (!opts.merge_stderr) { ::close(err_pipe[0]); ::close(err_pipe[1]); }
        if (!opts.working_dir.empty()) {
            if (::chdir(opts.working_dir.c_str()) != 0) _exit(127);
        }
        environ = envp.data(); // portable alternative to execvpe
        ::execvp(opts.executable.c_str(), argv.data());
        _exit(127); // exec failed
    }

    ::setpgid(pid, pid);
    ::close(out_pipe[1]);
    if (!opts.merge_stderr) ::close(err_pipe[1]);

    std::string out_buf, err_buf;
    struct pollfd fds[2];
    int nfds = 0;
    int out_idx = -1, err_idx = -1;
    fds[nfds].fd = out_pipe[0]; fds[nfds].events = POLLIN; out_idx = nfds++;
    if (!opts.merge_stderr) { fds[nfds].fd = err_pipe[0]; fds[nfds].events = POLLIN; err_idx = nfds++; }

    const auto start = std::chrono::steady_clock::now();
    bool killed = false;
    int open_fds = nfds;
    while (open_fds > 0) {
        int pr = ::poll(fds, nfds, 50);
        if (pr > 0) {
            char chunk[4096];
            for (int i = 0; i < nfds; ++i) {
                if (fds[i].fd < 0) continue;
                if (fds[i].revents & (POLLIN | POLLHUP)) {
                    ssize_t n = ::read(fds[i].fd, chunk, sizeof(chunk));
                    if (n > 0) {
                        bool is_err = (i == err_idx);
                        auto& buf = is_err ? err_buf : out_buf;
                        buf.append(chunk, n);
                        PumpLines(buf, is_err, on_output);
                    } else {
                        ::close(fds[i].fd); fds[i].fd = -1; --open_fds;
                    }
                }
            }
        }
        if (cancel && cancel->load()) { result.canceled = true; killed = true; }
        if (timeout.count() > 0 && std::chrono::steady_clock::now() - start >= timeout) {
            result.timed_out = true; killed = true;
        }
        if (killed) { ::kill(-pid, SIGKILL); break; }
    }
    (void)out_idx;
    FlushTail(out_buf, false, on_output);
    if (!opts.merge_stderr) FlushTail(err_buf, true, on_output);

    if (out_pipe[0] >= 0) ::close(out_pipe[0]);
    if (!opts.merge_stderr && err_pipe[0] >= 0) ::close(err_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    result.launched = true;
    if (!killed) {
        if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
    }
    return result;
}

#endif

ProcessResult RunProcessCapture(const ProcessOptions& opts, std::string& out_combined,
                                std::chrono::milliseconds timeout, const std::atomic<bool>* cancel) {
    ProcessOptions o = opts;
    o.merge_stderr = true;
    out_combined.clear();
    std::mutex m;
    ProcessResult r = RunProcess(o, [&](std::string_view line, bool) {
        std::lock_guard<std::mutex> lk(m);
        out_combined.append(line.data(), line.size());
        out_combined.push_back('\n');
    }, timeout, cancel);
    return r;
}

} // namespace dse::platform
