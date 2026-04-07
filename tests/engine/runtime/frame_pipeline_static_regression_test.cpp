#include "catch/catch.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string ReadTextFile(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path current = fs::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        const fs::path candidate = current / path;
        std::ifstream input(candidate, std::ios::in | std::ios::binary);
        if (input.is_open()) {
            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    FAIL(std::string("Failed to open file: ") + path + " from current path " + fs::current_path().string());
}

} // namespace

TEST_CASE("Given_FramePipelineSource_When_CheckParticleUpdateCall_Then_Physics2DPointerIsExplicitlyPassed", "[engine][unit][runtime][static]") {
    const std::string source = ReadTextFile("engine/runtime/frame_pipeline.cpp");
    REQUIRE(source.find("particle_system_.Update(*world_, delta_time, &physics2d_system_);") != std::string::npos);
}
