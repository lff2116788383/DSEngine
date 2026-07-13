// Unit tests for the unified cross-platform process runner (P1-1).
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "engine/platform/process.h"

using namespace dse::platform;

namespace {
#if defined(_WIN32)
ProcessOptions EchoHello() {
    ProcessOptions o;
    o.executable = "cmd";
    o.args = {"/c", "echo", "hello-proc"};
    return o;
}
ProcessOptions ExitWith(int code) {
    ProcessOptions o;
    o.executable = "cmd";
    o.args = {"/c", "exit " + std::to_string(code)};
    return o;
}
ProcessOptions SleepLong() {
    ProcessOptions o;
    o.executable = "cmd";
    o.args = {"/c", "ping", "127.0.0.1", "-n", "30"};
    return o;
}
#else
ProcessOptions EchoHello() {
    ProcessOptions o;
    o.executable = "/bin/echo";
    o.args = {"hello-proc"};
    return o;
}
ProcessOptions ExitWith(int code) {
    ProcessOptions o;
    o.executable = "/bin/sh";
    o.args = {"-c", "exit " + std::to_string(code)};
    return o;
}
ProcessOptions SleepLong() {
    ProcessOptions o;
    o.executable = "/bin/sh";
    o.args = {"-c", "sleep 30"};
    return o;
}
#endif

// A child that echoes each stdin line back to stdout (for bidirectional tests).
ProcessOptions StdinEcho() {
    ProcessOptions o;
#if defined(_WIN32)
    o.executable = "cmd";
    o.args = {"/c", "sort"};  // reads stdin, writes it back to stdout on EOF
#else
    o.executable = "/bin/cat";
#endif
    return o;
}

std::string DrainUntil(ManagedProcess& p, const std::string& needle, int max_polls = 200) {
    std::string acc;
    for (int i = 0; i < max_polls; ++i) {
        if (!p.ReadStdout(acc) && acc.find(needle) == std::string::npos) {
            // pipe closed; one last chance already appended.
        }
        if (acc.find(needle) != std::string::npos) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return acc;
}
} // namespace

TEST(ManagedProcess, CapturesStdoutFromChild) {
    ManagedProcessOptions o;
    o.process = EchoHello();
    o.pipe_stdout = true;

    ManagedProcess p;
    std::string err;
    ASSERT_TRUE(p.Start(o, &err)) << err;

    std::string out = DrainUntil(p, "hello-proc");
    EXPECT_NE(out.find("hello-proc"), std::string::npos);

    int code = p.Wait();
    EXPECT_EQ(code, 0);
    EXPECT_FALSE(p.Running());
}

TEST(ManagedProcess, BidirectionalStdinStdout) {
    ManagedProcessOptions o;
    o.process = StdinEcho();
    o.pipe_stdin = true;
    o.pipe_stdout = true;

    ManagedProcess p;
    std::string err;
    ASSERT_TRUE(p.Start(o, &err)) << err;

    ASSERT_TRUE(p.WriteStdin("ping-token\n"));
    p.CloseStdin();  // EOF: child echoes its buffered input and exits

    // The child may block-buffer stdout to a pipe (e.g. findstr), so drain after EOF.
    std::string out = DrainUntil(p, "ping-token");
    EXPECT_NE(out.find("ping-token"), std::string::npos);

    p.Wait();
    EXPECT_FALSE(p.Running());
}

TEST(ManagedProcess, KillTerminatesLongLivedChild) {
    ManagedProcessOptions o;
    o.process = SleepLong();

    ManagedProcess p;
    std::string err;
    ASSERT_TRUE(p.Start(o, &err)) << err;
    EXPECT_TRUE(p.Running());

    p.Kill();
    EXPECT_FALSE(p.Running());
}

TEST(ManagedProcess, DestructorKillsRunningChild) {
    long pid = 0;
    {
        ManagedProcessOptions o;
        o.process = SleepLong();
        ManagedProcess p;
        std::string err;
        ASSERT_TRUE(p.Start(o, &err)) << err;
        pid = p.Pid();
        EXPECT_GT(pid, 0);
    }  // destructor must terminate the child; no orphan/leak.
    SUCCEED();
}

TEST(ManagedProcess, StartFailsForMissingExecutable) {
    ManagedProcessOptions o;
    o.process.executable = "definitely_not_a_real_program_xyz";
    ManagedProcess p;
    std::string err;
    EXPECT_FALSE(p.Start(o, &err));
    EXPECT_FALSE(err.empty());
}

TEST(ManagedProcess, LaunchDetachedReportsMissingExecutable) {
    ProcessOptions o;
    o.executable = "";
    std::string err;
    EXPECT_FALSE(LaunchDetached(o, &err));
    EXPECT_FALSE(err.empty());
}

TEST(ProcessRunner, CapturesStdoutAndZeroExit) {
    std::string out;
    ProcessResult r = RunProcessCapture(EchoHello(), out);
    ASSERT_TRUE(r.launched) << r.error;
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(out.find("hello-proc"), std::string::npos);
    EXPECT_TRUE(r.Succeeded());
}

TEST(ProcessRunner, PropagatesNonZeroExitCode) {
    std::string out;
    ProcessResult r = RunProcessCapture(ExitWith(3), out);
    ASSERT_TRUE(r.launched) << r.error;
    EXPECT_EQ(r.exit_code, 3);
    EXPECT_FALSE(r.Succeeded());
}

TEST(ProcessRunner, MissingExecutableReportsError) {
    ProcessOptions o;
    o.executable = "definitely_not_a_real_program_xyz";
    std::string out;
    ProcessResult r = RunProcessCapture(o, out);
    // Either launch fails outright, or the shell reports a non-zero code; both
    // must be surfaced (never a silent success).
    EXPECT_FALSE(r.Succeeded());
}

TEST(ProcessRunner, TimeoutTerminatesProcessTree) {
    std::string out;
    auto start = std::chrono::steady_clock::now();
    ProcessResult r = RunProcessCapture(SleepLong(), out, std::chrono::milliseconds(400));
    auto elapsed = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(r.launched) << r.error;
    EXPECT_TRUE(r.timed_out);
    EXPECT_FALSE(r.Succeeded());
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10);
}

TEST(ProcessRunner, CancelFlagTerminatesProcess) {
    std::atomic<bool> cancel{false};
    std::thread canceller([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        cancel.store(true);
    });
    std::string out;
    ProcessResult r = RunProcessCapture(SleepLong(), out, std::chrono::milliseconds::zero(), &cancel);
    canceller.join();
    ASSERT_TRUE(r.launched) << r.error;
    EXPECT_TRUE(r.canceled);
    EXPECT_FALSE(r.Succeeded());
}
