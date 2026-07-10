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
} // namespace

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
