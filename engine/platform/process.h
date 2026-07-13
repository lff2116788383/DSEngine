/**
 * @file process.h
 * @brief Cross-platform external process runner (P1-1).
 *
 * Single, shared implementation used by Git, Web build, C# build and any other
 * subsystem that needs to spawn an external tool. Deliberately avoids unsafe
 * shell-string concatenation (`std::system`): the program and its arguments are
 * passed as an explicit array, output is streamed line-by-line, exit codes are
 * reported, and cancel/timeout terminate the whole process tree.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse::platform {

/// How to launch an external process.
struct ProcessOptions {
    std::string executable;                                  ///< Path/name of the program (NOT a shell line).
    std::vector<std::string> args;                           ///< Argument array; each element passed verbatim.
    std::filesystem::path working_dir;                       ///< Empty = inherit current directory.
    std::vector<std::pair<std::string, std::string>> env;    ///< Extra/override environment variables.
    bool inherit_env = true;                                 ///< Start from the parent environment.
    bool merge_stderr = false;                               ///< Route stderr through the stdout callback (is_stderr=false).
};

/// Outcome of a process run.
struct ProcessResult {
    bool launched = false;   ///< False if the executable could not be started.
    int  exit_code = -1;     ///< Process exit code (valid only when launched && !timed_out && !canceled).
    bool timed_out = false;  ///< True if killed because the timeout elapsed.
    bool canceled = false;   ///< True if killed because the cancel flag was set.
    std::string error;       ///< Human-readable diagnostic when launched == false or on failure.

    bool Succeeded() const { return launched && !timed_out && !canceled && exit_code == 0; }
};

/// Line-oriented output sink. `line` excludes the trailing newline.
/// `is_stderr` distinguishes stderr from stdout (unless merge_stderr is set).
using ProcessOutputFn = std::function<void(std::string_view line, bool is_stderr)>;

/// Run an external process synchronously on the calling thread.
///
/// - stdout/stderr are read concurrently and delivered to @p on_output as whole
///   UTF-8 lines (partial trailing content is flushed at exit).
/// - When @p timeout > 0 and elapses, the process tree is terminated and
///   ProcessResult::timed_out is set.
/// - When @p cancel is non-null and becomes true, the process tree is
///   terminated and ProcessResult::canceled is set.
/// - No console window is created on Windows.
DSE_EXPORT ProcessResult RunProcess(const ProcessOptions& opts,
                                    const ProcessOutputFn& on_output = {},
                                    std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
                                    const std::atomic<bool>* cancel = nullptr);

/// Convenience: capture all stdout (and, if merge_stderr, stderr) into a string.
DSE_EXPORT ProcessResult RunProcessCapture(const ProcessOptions& opts,
                                           std::string& out_combined,
                                           std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
                                           const std::atomic<bool>* cancel = nullptr);

/// How to launch a long-lived, asynchronously-managed process (see ManagedProcess).
struct ManagedProcessOptions {
    ProcessOptions process;     ///< Executable + args + working dir + env (same rules as RunProcess).
    bool pipe_stdin = false;    ///< Create a stdin pipe so the parent can WriteStdin().
    bool pipe_stdout = false;   ///< Create a stdout pipe so the parent can ReadStdout().
    bool no_window = true;      ///< Windows: do not create a console window.
};

/// A long-lived child process the caller keeps a handle to: start once, optionally
/// stream stdin/stdout, poll liveness/exit, and Kill() the whole tree on demand.
///
/// This models the patterns RunProcess() (a synchronous run-to-completion helper)
/// deliberately does not: a persistent agent child with bidirectional pipes, a
/// managed plugin child kept alive for a later Kill(), etc. Move-only; the
/// destructor kills the tree if still running so no child is orphaned.
class DSE_EXPORT ManagedProcess {
public:
    ManagedProcess();
    ~ManagedProcess();
    ManagedProcess(ManagedProcess&&) noexcept;
    ManagedProcess& operator=(ManagedProcess&&) noexcept;
    ManagedProcess(const ManagedProcess&) = delete;
    ManagedProcess& operator=(const ManagedProcess&) = delete;

    /// Launch the child. Returns false (and fills @p error, if non-null) on failure.
    bool Start(const ManagedProcessOptions& opts, std::string* error = nullptr);

    /// True while the child has been started and has not yet exited.
    bool Running() const;

    /// Write bytes to the child's stdin. Returns false if there is no stdin pipe,
    /// the pipe was closed, or the write failed.
    bool WriteStdin(std::string_view data);

    /// Close the child's stdin (signals EOF to the child). Idempotent.
    void CloseStdin();

    /// Append any currently-available stdout bytes to @p out (non-blocking).
    /// Returns false once the pipe is closed AND fully drained (child gone).
    bool ReadStdout(std::string& out);

    /// If the child has exited, set @p code to its exit code and return true.
    bool TryGetExitCode(int& code) const;

    /// Block until the child exits; returns its exit code (-1 if never started).
    int Wait();

    /// Terminate the child (and its tree) if running. Idempotent.
    void Kill();

    /// Native process id (0 if not started).
    long Pid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Fire-and-forget launch of a detached process (e.g. a GUI external editor):
/// no pipes, no wait, the child outlives this call and is not tracked. Returns
/// false (and fills @p error, if non-null) if the process could not be started.
DSE_EXPORT bool LaunchDetached(const ProcessOptions& opts, std::string* error = nullptr);

} // namespace dse::platform
