/**
 * @file project_scaffold_test.cpp
 * @brief 项目模板生成测试：单轴模板 + 两轴（GameType × ScriptingLanguage）产物校验
 *
 * 测试策略：
 * - 两轴式：Lua / C# 脚本语言各自生成完整且可用的目录结构
 * - 单轴品类模板：Platformer2D / TopDownRPG / ThirdPerson3D 生成品类脚本
 * - C# 模板产物齐全（sln / csproj / DseScript 基类 / 示例脚本）
 * 临时目录置于仓库根下（沙箱对 %TEMP% 的 fs::remove_all 静默失败，避免污染）。
 */

#include <gtest/gtest.h>

#include "engine/project/project_scaffold.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

const std::string kTmpRoot = "test_project_scaffold_tmp";

std::string ReadFile(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool FileExists(const fs::path& p) { return fs::exists(p); }

// 生成唯一子目录名避免并发/残留冲突
std::string UniqueDir(const std::string& prefix) {
    return kTmpRoot + "/" + prefix + "_" + std::to_string(
        static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void Cleanup(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
}

} // namespace

// 两轴式：GameType::Game2D × Lua 生成完整目录树与入口脚本
TEST(ProjectScaffoldTest, TwoAxisGame2DLua) {
    const fs::path dir = UniqueDir("game2d_lua");
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto res = dse::project::ScaffoldProject(
        dir.string(), "TestGame", dse::project::GameType::Game2D,
        dse::project::ScriptingLanguage::Lua, "0.1.0");
    ASSERT_TRUE(res.ok) << res.error;

    EXPECT_TRUE(FileExists(dir / "project.dseproj"));
    EXPECT_TRUE(FileExists(dir / "scenes" / "main.json"));
    EXPECT_TRUE(FileExists(dir / "scripts" / "main.lua"));
    const std::string lua = ReadFile(dir / "scripts" / "main.lua");
    EXPECT_NE(lua.find("Awake"), std::string::npos);
    EXPECT_NE(lua.find("Update"), std::string::npos);

    const std::string proj = ReadFile(dir / "project.dseproj");
    EXPECT_NE(proj.find("\"lua_scripting\""), std::string::npos);
    EXPECT_NE(proj.find("\"entry_script\": \"scripts/main.lua\""), std::string::npos);

    Cleanup(dir);
}

// 两轴式：GameType::Game3D × C# 生成完整 C# 工程（sln/csproj/基类/示例）
TEST(ProjectScaffoldTest, TwoAxisGame3DCSharp) {
    const fs::path dir = UniqueDir("game3d_csharp");
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto res = dse::project::ScaffoldProject(
        dir.string(), "CsGame", dse::project::GameType::Game3D,
        dse::project::ScriptingLanguage::CSharp, "0.1.0");
    ASSERT_TRUE(res.ok) << res.error;

    // 场景预置：3D 相机 + 平行光
    const std::string scene = ReadFile(dir / "scenes" / "main.json");
    EXPECT_NE(scene.find("camera3d"), std::string::npos);

    // C# 工程文件齐全
    const fs::path cs = dir / "GameScripts";
    EXPECT_TRUE(FileExists(cs / "DSEngine.sln"));
    EXPECT_TRUE(FileExists(cs / "DSEngine.Runtime" / "DSEngine.Runtime.csproj"));
    EXPECT_TRUE(FileExists(cs / "DSEngine.Runtime" / "Core" / "Entity.cs"));
    EXPECT_TRUE(FileExists(cs / "DSEngine.Runtime" / "Core" / "DseScript.cs"));
    EXPECT_TRUE(FileExists(cs / "DSEngine.Game" / "DSEngine.Game.csproj"));
    EXPECT_TRUE(FileExists(cs / "DSEngine.Game" / "SampleScript.cs"));

    const std::string dse_script = ReadFile(cs / "DSEngine.Runtime" / "Core" / "DseScript.cs");
    EXPECT_NE(dse_script.find("OnUpdate"), std::string::npos);
    const std::string sample = ReadFile(cs / "DSEngine.Game" / "SampleScript.cs");
    EXPECT_NE(sample.find("DseScript"), std::string::npos);

    // 描述文件脚本目录指向 GameScripts
    const std::string proj = ReadFile(dir / "project.dseproj");
    EXPECT_NE(proj.find("\"csharp_scripting\""), std::string::npos);
    EXPECT_NE(proj.find("GameScripts/DSEngine.Game"), std::string::npos);

    Cleanup(dir);
}

// 两轴式：Empty × C++ 生成 src/main.cpp + CMakeLists.txt
TEST(ProjectScaffoldTest, TwoAxisEmptyCpp) {
    const fs::path dir = UniqueDir("empty_cpp");
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto res = dse::project::ScaffoldProject(
        dir.string(), "CppGame", dse::project::GameType::Empty,
        dse::project::ScriptingLanguage::Cpp, "0.1.0");
    ASSERT_TRUE(res.ok) << res.error;

    EXPECT_TRUE(FileExists(dir / "src" / "main.cpp"));
    EXPECT_TRUE(FileExists(dir / "CMakeLists.txt"));
    const std::string main = ReadFile(dir / "src" / "main.cpp");
    EXPECT_NE(main.find("ConfigureCppBusinessHooks"), std::string::npos);

    Cleanup(dir);
}

// 单轴品类模板：Platformer2D 生成品类脚本（含重力/跳跃逻辑特征）
TEST(ProjectScaffoldTest, PlatformerCategoryTemplate) {
    const fs::path dir = UniqueDir("platformer");
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto res = dse::project::ScaffoldProject(
        dir.string(), "Plat", dse::project::ProjectTemplate::Platformer2D, "0.1.0");
    ASSERT_TRUE(res.ok) << res.error;

    EXPECT_TRUE(FileExists(dir / "scripts" / "main.lua"));
    const std::string lua = ReadFile(dir / "scripts" / "main.lua");
    EXPECT_NE(lua.find("platformer"), std::string::npos);
    EXPECT_NE(lua.find("GRAVITY"), std::string::npos);

    Cleanup(dir);
}

// 单轴品类模板：TopDownRPG 生成品类脚本（从模板目录拷贝素材 + 完整游戏脚本）
TEST(ProjectScaffoldTest, TopDownCategoryTemplate) {
    const fs::path dir = UniqueDir("topdown");
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto res = dse::project::ScaffoldProject(
        dir.string(), "Top", dse::project::ProjectTemplate::TopDownRPG, "0.1.0");
    ASSERT_TRUE(res.ok) << res.error;

    // 素材目录被模板素材填充
    EXPECT_TRUE(FileExists(dir / "assets"));
    bool assets_filled = false;
    for (auto it = fs::directory_iterator(dir / "assets", ec); it != fs::directory_iterator(); ++it) {
        assets_filled = true;
        break;
    }
    EXPECT_TRUE(assets_filled);

    const std::string lua = ReadFile(dir / "scripts" / "main.lua");
    EXPECT_NE(lua.find("Awake"), std::string::npos);
    EXPECT_NE(lua.find("topdown_3d"), std::string::npos);

    Cleanup(dir);
}

// 模板 token 解析覆盖全部枚举
TEST(ProjectScaffoldTest, ParseTemplateTokens) {
    struct Case { const char* token; dse::project::ProjectTemplate expected; };
    const Case cases[] = {
        {"empty", dse::project::ProjectTemplate::Empty},
        {"2d", dse::project::ProjectTemplate::Game2D},
        {"3d", dse::project::ProjectTemplate::Game3D},
        {"lua", dse::project::ProjectTemplate::Lua},
        {"cpp", dse::project::ProjectTemplate::Cpp},
        {"csharp", dse::project::ProjectTemplate::CSharp},
        {"platformer", dse::project::ProjectTemplate::Platformer2D},
        {"topdown", dse::project::ProjectTemplate::TopDownRPG},
        {"thirdperson", dse::project::ProjectTemplate::ThirdPerson3D},
    };
    for (const auto& c : cases) {
        dse::project::ProjectTemplate out = dse::project::ProjectTemplate::Empty;
        EXPECT_TRUE(dse::project::ParseTemplateToken(c.token, out)) << c.token;
        EXPECT_EQ(out, c.expected) << c.token;
    }
    dse::project::ProjectTemplate out = dse::project::ProjectTemplate::Empty;
    EXPECT_FALSE(dse::project::ParseTemplateToken("bogus", out));
}
