#define CATCH_CONFIG_RUNNER
#include "catch/catch.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

bool ShouldPauseOnFailureInteractive() {
#ifdef _WIN32
    if (GetEnvironmentVariableA("CTEST_INTERACTIVE_DEBUG_MODE", nullptr, 0) > 0) {
        return false;
    }
    if (GetEnvironmentVariableA("DSE_NO_TEST_PAUSE", nullptr, 0) > 0) {
        return false;
    }
    if (GetEnvironmentVariableA("CI", nullptr, 0) > 0) {
        return false;
    }
    if (GetEnvironmentVariableA("CTEST_FULL_OUTPUT", nullptr, 0) > 0) {
        return false;
    }
    if (GetEnvironmentVariableA("DSE_ENABLE_TEST_PAUSE", nullptr, 0) == 0) {
        return false;
    }
    return ::GetConsoleWindow() != nullptr;
#else
    return false;
#endif
}

void PauseOnFailureIfNeeded(int result) {
#ifdef _WIN32
    if (result == 0 || !ShouldPauseOnFailureInteractive()) {
        return;
    }
    std::fputs("\n[Test Failure] Press Enter to close...", stdout);
    std::fflush(stdout);
    (void)std::getchar();
#else
    (void)result;
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    std::fputs("[test-main] before session construct\n", stdout);
    std::fflush(stdout);
    Catch::Session session;
    std::fputs("[test-main] before session.run\n", stdout);
    std::fflush(stdout);
    const int result = session.run(argc, argv);
    std::fputs("[test-main] after session.run\n", stdout);
    std::fflush(stdout);
    PauseOnFailureIfNeeded(result);
    std::fputs("[test-main] after pause check\n", stdout);
    std::fflush(stdout);
    return result;
}
