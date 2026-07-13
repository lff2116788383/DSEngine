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
#  include <fcntl.h>
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

// ---------------------------------------------------------------------------
// ManagedProcess: long-lived child with optional bidirectional pipes.
// ---------------------------------------------------------------------------

struct ManagedProcess::Impl {
#if defined(_WIN32)
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    HANDLE stdin_w = nullptr;
    HANDLE stdout_r = nullptr;
    DWORD  pid = 0;
#else
    pid_t pid = 0;
    int   stdin_w = -1;
    int   stdout_r = -1;
#endif
    bool started = false;
    bool exited = false;
    int  exit_code = -1;
    bool stdout_open = false;
};

ManagedProcess::ManagedProcess() : impl_(std::make_unique<Impl>()) {}

ManagedProcess::~ManagedProcess() {
    if (impl_) Kill();
}

ManagedProcess::ManagedProcess(ManagedProcess&&) noexcept = default;
ManagedProcess& ManagedProcess::operator=(ManagedProcess&& other) noexcept {
    if (this != &other) {
        Kill();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

long ManagedProcess::Pid() const { return impl_ ? static_cast<long>(impl_->pid) : 0; }

#if defined(_WIN32)

bool ManagedProcess::Start(const ManagedProcessOptions& opts, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return false; };
    if (impl_->started) return fail("process already started");
    if (opts.process.executable.empty()) return fail("executable is empty");

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE in_r = nullptr, in_w = nullptr, out_r = nullptr, out_w = nullptr;
    auto close_local = [&]() {
        for (HANDLE* h : {&in_r, &in_w, &out_r, &out_w}) {
            if (*h) { ::CloseHandle(*h); *h = nullptr; }
        }
    };

    if (opts.pipe_stdin) {
        if (!::CreatePipe(&in_r, &in_w, &sa, 0)) return fail("CreatePipe(stdin) failed");
        ::SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    }
    if (opts.pipe_stdout) {
        if (!::CreatePipe(&out_r, &out_w, &sa, 0)) { close_local(); return fail("CreatePipe(stdout) failed"); }
        ::SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    }

    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji{};
        ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji));
    }

    std::wstring cmd;
    AppendQuotedArg(cmd, Utf8ToWide(opts.process.executable));
    for (auto& a : opts.process.args) { cmd += L' '; AppendQuotedArg(cmd, Utf8ToWide(a)); }

    std::wstring env_block = BuildEnvBlock(opts.process);
    std::wstring wdir = opts.process.working_dir.empty() ? std::wstring() : opts.process.working_dir.wstring();

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    const bool redirect = opts.pipe_stdin || opts.pipe_stdout;
    if (redirect) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput  = opts.pipe_stdin  ? in_r  : ::GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = opts.pipe_stdout ? out_w : ::GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = ::GetStdHandle(STD_ERROR_HANDLE);
    }

    DWORD flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;
    if (opts.no_window) flags |= CREATE_NO_WINDOW;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd_mutable(cmd.begin(), cmd.end());
    cmd_mutable.push_back(L'\0');

    BOOL ok = ::CreateProcessW(nullptr, cmd_mutable.data(), nullptr, nullptr,
                               redirect ? TRUE : FALSE, flags, env_block.data(),
                               wdir.empty() ? nullptr : wdir.c_str(), &si, &pi);

    if (in_r) { ::CloseHandle(in_r); in_r = nullptr; }
    if (out_w) { ::CloseHandle(out_w); out_w = nullptr; }

    if (!ok) {
        DWORD e = ::GetLastError();
        if (in_w) ::CloseHandle(in_w);
        if (out_r) ::CloseHandle(out_r);
        if (job) ::CloseHandle(job);
        return fail("CreateProcess failed (error " + std::to_string(e) + ") for '" + opts.process.executable + "'");
    }

    if (job) ::AssignProcessToJobObject(job, pi.hProcess);
    ::ResumeThread(pi.hThread);
    ::CloseHandle(pi.hThread);

    impl_->process = pi.hProcess;
    impl_->job = job;
    impl_->pid = pi.dwProcessId;
    impl_->stdin_w = in_w;
    impl_->stdout_r = out_r;
    impl_->stdout_open = (out_r != nullptr);
    impl_->started = true;
    impl_->exited = false;
    return true;
}

bool ManagedProcess::Running() const {
    if (!impl_->started || impl_->exited || !impl_->process) return false;
    DWORD code = 0;
    if (::GetExitCodeProcess(impl_->process, &code) && code == STILL_ACTIVE) return true;
    impl_->exited = true;
    impl_->exit_code = static_cast<int>(code);
    return false;
}

bool ManagedProcess::WriteStdin(std::string_view data) {
    if (!impl_->stdin_w) return false;
    const char* p = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
        DWORD written = 0;
        if (!::WriteFile(impl_->stdin_w, p, static_cast<DWORD>(remaining), &written, nullptr) || written == 0) {
            return false;
        }
        p += written;
        remaining -= written;
    }
    return true;
}

void ManagedProcess::CloseStdin() {
    if (impl_->stdin_w) { ::CloseHandle(impl_->stdin_w); impl_->stdin_w = nullptr; }
}

bool ManagedProcess::ReadStdout(std::string& out) {
    if (!impl_->stdout_r) return false;
    for (;;) {
        DWORD avail = 0;
        if (!::PeekNamedPipe(impl_->stdout_r, nullptr, 0, nullptr, &avail, nullptr)) {
            // Pipe broken (child closed its write end / exited).
            ::CloseHandle(impl_->stdout_r);
            impl_->stdout_r = nullptr;
            impl_->stdout_open = false;
            return false;
        }
        if (avail == 0) return impl_->stdout_open;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!::ReadFile(impl_->stdout_r, chunk.data(), avail, &read, nullptr) || read == 0) {
            ::CloseHandle(impl_->stdout_r);
            impl_->stdout_r = nullptr;
            impl_->stdout_open = false;
            return false;
        }
        out.append(chunk.data(), read);
    }
}

bool ManagedProcess::TryGetExitCode(int& code) const {
    if (!impl_->started) return false;
    if (impl_->exited) { code = impl_->exit_code; return true; }
    if (!impl_->process) return false;
    DWORD c = 0;
    if (::GetExitCodeProcess(impl_->process, &c) && c != STILL_ACTIVE) {
        impl_->exited = true;
        impl_->exit_code = static_cast<int>(c);
        code = impl_->exit_code;
        return true;
    }
    return false;
}

int ManagedProcess::Wait() {
    if (!impl_->started || !impl_->process) return -1;
    ::WaitForSingleObject(impl_->process, INFINITE);
    DWORD c = 0;
    ::GetExitCodeProcess(impl_->process, &c);
    impl_->exited = true;
    impl_->exit_code = static_cast<int>(c);
    return impl_->exit_code;
}

void ManagedProcess::Kill() {
    if (!impl_) return;
    if (impl_->job) { ::TerminateJobObject(impl_->job, 1); ::CloseHandle(impl_->job); impl_->job = nullptr; }
    else if (impl_->process) ::TerminateProcess(impl_->process, 1);
    if (impl_->stdin_w) { ::CloseHandle(impl_->stdin_w); impl_->stdin_w = nullptr; }
    if (impl_->stdout_r) { ::CloseHandle(impl_->stdout_r); impl_->stdout_r = nullptr; }
    if (impl_->process) { ::CloseHandle(impl_->process); impl_->process = nullptr; }
    impl_->stdout_open = false;
    impl_->started = false;
}

bool LaunchDetached(const ProcessOptions& opts, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return false; };
    if (opts.executable.empty()) return fail("executable is empty");

    std::wstring cmd;
    AppendQuotedArg(cmd, Utf8ToWide(opts.executable));
    for (auto& a : opts.args) { cmd += L' '; AppendQuotedArg(cmd, Utf8ToWide(a)); }

    std::wstring env_block = BuildEnvBlock(opts);
    std::wstring wdir = opts.working_dir.empty() ? std::wstring() : opts.working_dir.wstring();

    STARTUPINFOW si{};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd_mutable(cmd.begin(), cmd.end());
    cmd_mutable.push_back(L'\0');

    BOOL ok = ::CreateProcessW(nullptr, cmd_mutable.data(), nullptr, nullptr, FALSE,
                               CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS,
                               env_block.data(), wdir.empty() ? nullptr : wdir.c_str(), &si, &pi);
    if (!ok) {
        return fail("CreateProcess failed (error " + std::to_string(::GetLastError()) + ") for '" + opts.executable + "'");
    }
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}

#else // POSIX

bool ManagedProcess::Start(const ManagedProcessOptions& opts, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return false; };
    if (impl_->started) return fail("process already started");
    if (opts.process.executable.empty()) return fail("executable is empty");

    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (opts.pipe_stdin && ::pipe(in_pipe) != 0) return fail("pipe(stdin) failed");
    if (opts.pipe_stdout && ::pipe(out_pipe) != 0) {
        if (opts.pipe_stdin) { ::close(in_pipe[0]); ::close(in_pipe[1]); }
        return fail("pipe(stdout) failed");
    }

    std::vector<std::string> arg_store;
    arg_store.push_back(opts.process.executable);
    for (auto& a : opts.process.args) arg_store.push_back(a);
    std::vector<char*> argv;
    for (auto& s : arg_store) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    std::vector<std::string> env_store;
    if (opts.process.inherit_env) {
        for (char** e = environ; e && *e; ++e) env_store.emplace_back(*e);
    }
    for (auto& kv : opts.process.env) {
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
        if (opts.pipe_stdin) { ::close(in_pipe[0]); ::close(in_pipe[1]); }
        if (opts.pipe_stdout) { ::close(out_pipe[0]); ::close(out_pipe[1]); }
        return fail("fork failed");
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        if (opts.pipe_stdin)  { ::dup2(in_pipe[0], STDIN_FILENO); ::close(in_pipe[0]); ::close(in_pipe[1]); }
        if (opts.pipe_stdout) { ::dup2(out_pipe[1], STDOUT_FILENO); ::close(out_pipe[0]); ::close(out_pipe[1]); }
        if (!opts.process.working_dir.empty()) {
            if (::chdir(opts.process.working_dir.c_str()) != 0) _exit(127);
        }
        environ = envp.data();
        ::execvp(opts.process.executable.c_str(), argv.data());
        _exit(127);
    }

    ::setpgid(pid, pid);
    if (opts.pipe_stdin)  { ::close(in_pipe[0]); impl_->stdin_w = in_pipe[1]; }
    if (opts.pipe_stdout) {
        ::close(out_pipe[1]);
        impl_->stdout_r = out_pipe[0];
        ::fcntl(impl_->stdout_r, F_SETFL, ::fcntl(impl_->stdout_r, F_GETFL) | O_NONBLOCK);
        impl_->stdout_open = true;
    }
    impl_->pid = pid;
    impl_->started = true;
    impl_->exited = false;
    return true;
}

bool ManagedProcess::Running() const {
    if (!impl_->started || impl_->exited) return false;
    int status = 0;
    pid_t r = ::waitpid(impl_->pid, &status, WNOHANG);
    if (r == 0) return true;
    if (r == impl_->pid) {
        impl_->exited = true;
        if (WIFEXITED(status)) impl_->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) impl_->exit_code = 128 + WTERMSIG(status);
    }
    return false;
}

bool ManagedProcess::WriteStdin(std::string_view data) {
    if (impl_->stdin_w < 0) return false;
    const char* p = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t n = ::write(impl_->stdin_w, p, remaining);
        if (n <= 0) return false;
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

void ManagedProcess::CloseStdin() {
    if (impl_->stdin_w >= 0) { ::close(impl_->stdin_w); impl_->stdin_w = -1; }
}

bool ManagedProcess::ReadStdout(std::string& out) {
    if (impl_->stdout_r < 0) return false;
    char chunk[4096];
    for (;;) {
        ssize_t n = ::read(impl_->stdout_r, chunk, sizeof(chunk));
        if (n > 0) { out.append(chunk, static_cast<size_t>(n)); continue; }
        if (n == 0) { // EOF
            ::close(impl_->stdout_r); impl_->stdout_r = -1; impl_->stdout_open = false;
            return false;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return impl_->stdout_open;
        ::close(impl_->stdout_r); impl_->stdout_r = -1; impl_->stdout_open = false;
        return false;
    }
}

bool ManagedProcess::TryGetExitCode(int& code) const {
    if (!impl_->started) return false;
    if (impl_->exited) { code = impl_->exit_code; return true; }
    int status = 0;
    pid_t r = ::waitpid(impl_->pid, &status, WNOHANG);
    if (r == impl_->pid) {
        impl_->exited = true;
        if (WIFEXITED(status)) impl_->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) impl_->exit_code = 128 + WTERMSIG(status);
        code = impl_->exit_code;
        return true;
    }
    return false;
}

int ManagedProcess::Wait() {
    if (!impl_->started) return -1;
    if (impl_->exited) return impl_->exit_code;
    int status = 0;
    ::waitpid(impl_->pid, &status, 0);
    impl_->exited = true;
    if (WIFEXITED(status)) impl_->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) impl_->exit_code = 128 + WTERMSIG(status);
    return impl_->exit_code;
}

void ManagedProcess::Kill() {
    if (!impl_) return;
    if (impl_->started && !impl_->exited && impl_->pid > 0) {
        ::kill(-impl_->pid, SIGKILL);
        int status = 0;
        ::waitpid(impl_->pid, &status, 0);
    }
    if (impl_->stdin_w >= 0) { ::close(impl_->stdin_w); impl_->stdin_w = -1; }
    if (impl_->stdout_r >= 0) { ::close(impl_->stdout_r); impl_->stdout_r = -1; }
    impl_->stdout_open = false;
    impl_->started = false;
}

bool LaunchDetached(const ProcessOptions& opts, std::string* error) {
    auto fail = [&](const std::string& m) { if (error) *error = m; return false; };
    if (opts.executable.empty()) return fail("executable is empty");

    std::vector<std::string> arg_store;
    arg_store.push_back(opts.executable);
    for (auto& a : opts.args) arg_store.push_back(a);
    std::vector<char*> argv;
    for (auto& s : arg_store) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) return fail("fork failed");
    if (pid == 0) {
        // Double-fork so the launched GUI app is fully detached (reparented to init).
        ::setsid();
        pid_t inner = ::fork();
        if (inner == 0) {
            if (!opts.working_dir.empty()) {
                if (::chdir(opts.working_dir.c_str()) != 0) _exit(127);
            }
            ::execvp(opts.executable.c_str(), argv.data());
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    return true;
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
