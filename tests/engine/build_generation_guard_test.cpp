#include "catch/catch.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path FindWorkspaceRoot() {
    auto current = std::filesystem::current_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "CMakeLists.txt") && std::filesystem::exists(current / "tests")) {
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

std::string ReadAllText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    REQUIRE(in.is_open());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("Given_GeneratedVisualStudioProjects_When_InspectingVcxproj_Then_VcpkgAutoLinkRemainsDisabled", "[engine][unit][build_generation][vcpkg]") {
    const auto root = FindWorkspaceRoot();
    const auto dse_engine_vcxproj = root / "build_vs2022" / "dse_engine.vcxproj";
    const auto cpp_host_vcxproj = root / "build_vs2022" / "apps" / "runtime" / "cpp_host" / "DSEngine_example_cpp.vcxproj";
    const auto lua_host_vcxproj = root / "build_vs2022" / "apps" / "runtime" / "lua_host" / "dse_example_lua.vcxproj";

    INFO("workspace root: " << root.string());
    REQUIRE(std::filesystem::exists(dse_engine_vcxproj));
    REQUIRE(std::filesystem::exists(cpp_host_vcxproj));
    REQUIRE(std::filesystem::exists(lua_host_vcxproj));

    const auto engine_text = ReadAllText(dse_engine_vcxproj);
    const auto cpp_host_text = ReadAllText(cpp_host_vcxproj);
    const auto lua_host_text = ReadAllText(lua_host_vcxproj);

    REQUIRE(engine_text.find("<VcpkgAutoLink>false</VcpkgAutoLink>") != std::string::npos);
    REQUIRE(cpp_host_text.find("<VcpkgAutoLink>false</VcpkgAutoLink>") != std::string::npos);
    REQUIRE(lua_host_text.find("<VcpkgAutoLink>false</VcpkgAutoLink>") != std::string::npos);

    REQUIRE(engine_text.find("installed\\x64-windows\\lib\\*.lib") == std::string::npos);
    REQUIRE(cpp_host_text.find("installed\\x64-windows\\lib\\*.lib") == std::string::npos);
    REQUIRE(lua_host_text.find("installed\\x64-windows\\lib\\*.lib") == std::string::npos);
}
