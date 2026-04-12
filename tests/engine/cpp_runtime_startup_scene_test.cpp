#include "catch/catch.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>

namespace {

std::string MakeTempPath(const char* name) {
    auto base = std::filesystem::temp_directory_path() / "dse_cpp_runtime_scene_tests";
    std::filesystem::create_directories(base);
    return (base / name).string();
}

struct ScopedTempFile {
    explicit ScopedTempFile(std::string p) : path(std::move(p)) {}
    ~ScopedTempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::string path;
};

void WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    REQUIRE(out.is_open());
    out << content;
}

std::string RunSampleAndCapture(const std::string& workdir, const std::string& envScene) {
    const std::string logPath = MakeTempPath("cpp_runtime_scene_test.log");
    ScopedTempFile logFile(logPath);

    std::string command = "$env:DSE_MAX_FRAMES='120'; ";
    if (!envScene.empty()) {
        command += "$env:DSE_STARTUP_SCENE='" + std::filesystem::path(envScene).generic_string() + "'; ";
    } else {
        command += "Remove-Item Env:DSE_STARTUP_SCENE -ErrorAction SilentlyContinue; ";
    }
    command += "$p = Start-Process -FilePath '" + (std::filesystem::path(workdir) / "bin" / "DSEngine_c++_debug.exe").string() + "' ";
    command += "-WorkingDirectory '" + std::filesystem::path(workdir).string() + "' ";
    command += "-PassThru -RedirectStandardOutput '" + std::filesystem::path(logPath).string() + "'; ";
    command += "Start-Sleep -Seconds 4; if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }; ";
    command += "Get-Content '" + std::filesystem::path(logPath).string() + "' -Raw";

    const std::string shellCommand = "powershell -NoProfile -Command \"" + command + "\"";

    std::array<char, 512> buffer{};
    std::string result;
    FILE* pipe = _popen(shellCommand.c_str(), "r");
    REQUIRE(pipe != nullptr);
    while (fgets(buffer.data(), 512, pipe) != nullptr) {
        result += buffer.data();
    }
    const int code = _pclose(pipe);
    REQUIRE(code == 0);
    return result;
}

} // namespace

TEST_CASE("Given_InvalidStartupScene_When_RuntimeStarts_Then_FallbackDemoPathIsUsedWithFailureLog", "[engine][smoke][cpp_runtime][startup_scene]") {
    const std::string repoRoot = "C:/Users/Administrator/Desktop/Engine/DSEngine";
    const std::string missingScene = "assets/scenes/does_not_exist.scene.json";

    const std::string output = RunSampleAndCapture(repoRoot, missingScene);
    INFO(output);
    REQUIRE(output.find("startup_scene_env=assets/scenes/does_not_exist.scene.json") != std::string::npos);
    REQUIRE(output.find("startup_scene_failed path=assets/scenes/does_not_exist.scene.json") != std::string::npos);
    REQUIRE(output.find("bootstrap_ok") != std::string::npos);
}

TEST_CASE("Given_ValidStartupScene_When_RuntimeStarts_Then_SceneModeLoadsAndLegacySpawnPathStaysDisabled", "[engine][smoke][cpp_runtime][startup_scene]") {
    const std::string repoRoot = "C:/Users/Administrator/Desktop/Engine/DSEngine";
    const std::string scenePath = "assets/scenes/3d_mvp_minimal.scene.json";

    const std::string output = RunSampleAndCapture(repoRoot, scenePath);
    INFO(output);
    REQUIRE(output.find("startup_scene_env=assets/scenes/3d_mvp_minimal.scene.json") != std::string::npos);
    REQUIRE(output.find("startup_scene_loaded path=assets/scenes/3d_mvp_minimal.scene.json") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=mesh path=assets/meshes/mvp_cube.fbx") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=skybox path=assets/skyboxes/default_sky") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=terrain_heightmap path=assets/terrain/height.png") != std::string::npos);
    REQUIRE(output.find("spawned=0/0") != std::string::npos);
}

TEST_CASE("Given_ReferenceDemoStartupScene_When_RuntimeStarts_Then_ReferenceSceneLoadsAndLegacySpawnPathStaysDisabled", "[engine][smoke][cpp_runtime][startup_scene][reference_demo]") {
    const std::string repoRoot = "C:/Users/Administrator/Desktop/Engine/DSEngine";
    const std::string scenePath = "assets/scenes/reference_demo_15_8.scene.json";

    const std::string output = RunSampleAndCapture(repoRoot, scenePath);
    INFO(output);
    REQUIRE(output.find("startup_scene_env=assets/scenes/reference_demo_15_8.scene.json") != std::string::npos);
    REQUIRE(output.find("startup_scene_loaded path=assets/scenes/reference_demo_15_8.scene.json") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=mesh path=assets/meshes/reference_demo_character_placeholder.fbx") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=skybox path=assets/skyboxes/default_sky") != std::string::npos);
    REQUIRE(output.find("spawned=0/0") != std::string::npos);
}

TEST_CASE("Given_ReferenceDemo159StartupScene_When_RuntimeStarts_Then_MaterialBootstrapPathLoadsAndLegacySpawnPathStaysDisabled", "[engine][smoke][cpp_runtime][startup_scene][reference_demo]") {
    const std::string repoRoot = "C:/Users/Administrator/Desktop/Engine/DSEngine";
    const std::string scenePath = "assets/scenes/reference_demo_15_9.scene.json";

    const std::string output = RunSampleAndCapture(repoRoot, scenePath);
    INFO(output);
    REQUIRE(output.find("startup_scene_env=assets/scenes/reference_demo_15_9.scene.json") != std::string::npos);
    REQUIRE(output.find("startup_scene_loaded path=assets/scenes/reference_demo_15_9.scene.json") != std::string::npos);
    REQUIRE(output.find("reference_demo_15_9_material_bootstrap") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=mesh path=assets/meshes/reference_demo_character_placeholder.fbx") != std::string::npos);
    REQUIRE(output.find("mvp_resource_missing type=skybox path=assets/skyboxes/default_sky") != std::string::npos);
    REQUIRE(output.find("spawned=0/0") != std::string::npos);
}
