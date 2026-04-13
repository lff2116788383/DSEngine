#include "catch/catch.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
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

TEST_CASE("Given_AssetBuilderSource_When_Reviewed_Then_CliHelpOutDirAndErrorContractsAreExplicit", "[engine][unit][asset_builder][static]") {
    const auto root = FindWorkspaceRoot();
    const auto source_path = root / "apps" / "tools" / "asset_builder" / "main.cpp";
    const auto text = ReadAllText(source_path);

    REQUIRE(text.find("AssetBuilder <input.gltf/glb> --out-dir <output_dir>") != std::string::npos);
    REQUIRE(text.find("if (first_arg == \"--help\" || first_arg == \"-h\")") != std::string::npos);
    REQUIRE(text.find("Unsupported input extension") != std::string::npos);
    REQUIRE(text.find("FBX is not supported as direct input") != std::string::npos);
    REQUIRE(text.find("if (option != \"--out-dir\")") != std::string::npos);

    REQUIRE(text.find("output_dmesh_path = output_dir / (base_name + \".dmesh\")") != std::string::npos);
    REQUIRE(text.find("Failed to cook dmat") != std::string::npos);
    REQUIRE(text.find("Failed to cook danim") != std::string::npos);
    REQUIRE(text.find("Failed to cook dskel") != std::string::npos);
}
