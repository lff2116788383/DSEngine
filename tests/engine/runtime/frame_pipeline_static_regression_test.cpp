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
    const std::string update_graph_source = ReadTextFile("engine/runtime/runtime_update_graph.cpp");
    REQUIRE(update_graph_source.find("pipeline.particle_system_.Update(world, delta_time, &pipeline.physics2d_system_);") != std::string::npos);
}

TEST_CASE("Given_FramePipelineSource_When_RuntimeGraphsAreExtracted_Then_UpdateAndFixedUpdateDelegateToRuntimeUpdateGraph", "[engine][unit][runtime][static]") {
    const std::string frame_pipeline_source = ReadTextFile("engine/runtime/frame_pipeline.cpp");
    REQUIRE(frame_pipeline_source.find("dse::runtime::RunRuntimeUpdateGraph(*this, delta_time);") != std::string::npos);
    REQUIRE(frame_pipeline_source.find("dse::runtime::RunRuntimeFixedUpdateGraph(*this, fixed_delta_time);") != std::string::npos);
}

TEST_CASE("Given_FramePipelineSource_When_RenderShellIsExtracted_Then_RenderLifecycleDelegatesToRuntimeRenderShell", "[engine][unit][runtime][static]") {
    const std::string frame_pipeline_source = ReadTextFile("engine/runtime/frame_pipeline.cpp");
    REQUIRE(frame_pipeline_source.find("dse::runtime::BeginRuntimeRenderFrame(*this);") != std::string::npos);
    REQUIRE(frame_pipeline_source.find("dse::runtime::CreateRuntimeRenderCommandBuffer(*this);") != std::string::npos);
    REQUIRE(frame_pipeline_source.find("dse::runtime::BindRuntimeShadowMaps(*this);") != std::string::npos);
    REQUIRE(frame_pipeline_source.find("dse::runtime::SubmitAndEndRuntimeRenderFrame(*this, std::move(cmd_buffer));") != std::string::npos);
    REQUIRE(frame_pipeline_source.find("dse::runtime::FinalizeRuntimeRenderFrame(*this);") != std::string::npos);
}
