#include "catch/catch.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path FindWorkspaceRoot() {
    auto current = std::filesystem::current_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "CMakeLists.txt") && std::filesystem::exists(current / "bin")) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::filesystem::current_path();
}

std::string QuoteArg(const std::string& value) {
#ifdef _WIN32
    return "\"" + value + "\"";
#else
    return "'" + value + "'";
#endif
}

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

CommandResult RunCommandCapture(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    REQUIRE(pipe != nullptr);

    std::array<char, 512> buffer{};
    std::string output;
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#ifdef _WIN32
    const int exit_code = _pclose(pipe);
#else
    const int exit_code = pclose(pipe);
#endif
    return {exit_code, output};
}

} // namespace

TEST_CASE("Given_RuntimeHosts_When_LaunchedWithShortFrameBudget_Then_CppAndLuaHostsBootstrapSuccessfully", "[engine][smoke][integration][runtime_host]") {
    const auto root = FindWorkspaceRoot();
    const auto cpp_host = root / "bin" / "DSEngine_c++_debug.exe";
    const auto lua_host = root / "bin" / "DSEngine_lua_debug.exe";

    INFO("workspace root: " << root.string());
    REQUIRE(std::filesystem::exists(cpp_host));
    REQUIRE(std::filesystem::exists(lua_host));

#ifdef _WIN32
    _putenv_s("DSE_MAX_FRAMES", "2");
    _putenv_s("DSE_NO_TEST_PAUSE", "1");
#else
    setenv("DSE_MAX_FRAMES", "2", 1);
    setenv("DSE_NO_TEST_PAUSE", "1", 1);
#endif

    const auto cpp_result = RunCommandCapture(QuoteArg(cpp_host.string()) + " 2>&1");
    INFO("cpp host output:\n" << cpp_result.output);
    REQUIRE(cpp_result.exit_code == 0);

    const auto lua_result = RunCommandCapture(QuoteArg(lua_host.string()) + " 2>&1");
    INFO("lua host output:\n" << lua_result.output);
    REQUIRE(lua_result.exit_code == 0);
}
