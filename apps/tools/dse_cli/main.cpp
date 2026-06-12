/**
 * @file main.cpp
 * @brief DSEngine headless CLI（OUTPUT 名 `dse`）。脱离编辑器完成：建项目模板 / 打包加密 / 完整 build。
 *
 * 用法：
 *   dse new <empty|2d|3d|lua> <dir>        # 生成项目模板
 *   dse pack <dir> <out.bun> [--key=KEY]   # 把目录打包成（可加密）资源包
 *   dse build <project> [--out=DIR] [--key=KEY]
 *                                          # 定位运行时、拷贝 exe+dll、打包加密、生成 launch.bat
 *
 * 与运行时端到端对应：加密 build 产物可被 DSEngine_Game 挂载解密并加载 Lua（见
 * EngineInstance::Init 的 MountBundle / lua_runtime 的 VFS searcher）。
 */

#include "engine/project/project_scaffold.h"
#include "engine/assets/bundle_packer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace fs = std::filesystem;

namespace {

int PrintUsage() {
    std::cout <<
        "DSEngine CLI\n"
        "用法:\n"
        "  dse new <empty|2d|3d|lua> <dir>          生成项目模板\n"
        "  dse pack <dir> <out.bun> [--key=KEY]     把目录打包成(可加密)资源包\n"
        "  dse build <project> [--out=DIR] [--key=KEY]\n"
        "                                           定位运行时, 拷贝 exe+dll, 打包加密, 生成 launch.bat\n";
    return 1;
}

// 从形如 --key=VALUE 的参数中取值；命中返回 true。
bool MatchOption(const std::string& arg, const std::string& prefix, std::string& out) {
    if (arg.rfind(prefix, 0) == 0) {
        out = arg.substr(prefix.size());
        return true;
    }
    return false;
}

std::string ReadEntryScript(const fs::path& dseproj, const std::string& fallback) {
    std::ifstream ifs(dseproj);
    if (!ifs.is_open()) {
        return fallback;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        return fallback;
    }
    if (doc.HasMember("entry_script") && doc["entry_script"].IsString()) {
        std::string v = doc["entry_script"].GetString();
        if (!v.empty()) {
            return v;
        }
    }
    return fallback;
}

int CmdNew(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "错误: new 需要 <模板> <目录>\n";
        return PrintUsage();
    }
    dse::project::ProjectTemplate tmpl;
    if (!dse::project::ParseTemplateToken(args[0], tmpl)) {
        std::cerr << "错误: 未知模板 '" << args[0] << "' (可选: empty|2d|3d|lua)\n";
        return 1;
    }
    const fs::path dir(args[1]);
    std::error_code ec;
    if (fs::exists(dir, ec) && !fs::is_empty(dir, ec)) {
        std::cerr << "错误: 目标目录非空: " << dir.string() << "\n";
        return 1;
    }
    const std::string name = dir.filename().string();
    dse::project::ScaffoldResult res = dse::project::ScaffoldProject(dir.string(), name, tmpl);
    if (!res.ok) {
        std::cerr << "错误: 生成项目失败: " << res.error << "\n";
        return 1;
    }
    std::cout << "已创建 " << dse::project::TemplateDisplayName(tmpl)
              << " 项目: " << fs::absolute(dir).string() << "\n";
    return 0;
}

int CmdPack(const std::vector<std::string>& args) {
    std::string dir;
    std::string out;
    std::string key;
    std::vector<std::string> positional;
    for (const auto& a : args) {
        std::string v;
        if (MatchOption(a, "--key=", v)) {
            key = v;
        } else {
            positional.push_back(a);
        }
    }
    if (positional.size() < 2) {
        std::cerr << "错误: pack 需要 <目录> <out.bun>\n";
        return PrintUsage();
    }
    dir = positional[0];
    out = positional[1];
    if (!fs::exists(dir)) {
        std::cerr << "错误: 目录不存在: " << dir << "\n";
        return 1;
    }
    std::error_code ec;
    const fs::path out_parent = fs::path(out).parent_path();
    if (!out_parent.empty()) {
        fs::create_directories(out_parent, ec);
    }
    if (!dse::assets::PackDirectoryToBundle(dir, out, key)) {
        std::cerr << "错误: 打包失败: " << out << "\n";
        return 1;
    }
    const auto sz = fs::exists(out) ? fs::file_size(out, ec) : 0;
    std::cout << "已打包 " << dir << " -> " << out
              << (key.empty() ? " (明文)" : " (AES 加密)")
              << "  " << sz << " bytes\n";
    return 0;
}

// 在 CLI 自身所在目录与 bin/ 下定位 standalone 运行时（DSEngine_Game[.exe]）。
fs::path LocateRuntimeExe(const char* argv0) {
#if defined(_WIN32)
    const char* names[] = {
        "DSEngine_Game.exe", "DSEngine_Game_release.exe", "DSEngine_Game_debug.exe",
    };
#else
    const char* names[] = {
        "DSEngine_Game", "DSEngine_Game_release", "DSEngine_Game_debug",
    };
#endif
    std::error_code ec;
    fs::path self_dir;
    try {
        self_dir = fs::absolute(fs::path(argv0)).parent_path();
    } catch (...) {
        self_dir = fs::current_path(ec);
    }
    std::vector<fs::path> search_dirs = {
        self_dir,
        self_dir / "bin",
        self_dir.parent_path() / "bin",
        fs::current_path(ec),
        fs::current_path(ec) / "bin",
    };
    for (const auto& d : search_dirs) {
        for (const char* n : names) {
            fs::path candidate = d / n;
            if (fs::exists(candidate, ec)) {
                return candidate;
            }
        }
    }
    return {};
}

// 将项目中需要随包发行的内容（脚本/场景/资产 + 描述文件）拷贝到 staging 目录，
// 避免把 build/ 等产物打进资源包。
bool StageProjectForPacking(const fs::path& project_root, const fs::path& staging, std::string& error) {
    std::error_code ec;
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec) {
        error = "无法创建打包暂存目录: " + ec.message();
        return false;
    }
    const char* subdirs[] = {"scripts", "scenes", "assets"};
    for (const char* sub : subdirs) {
        fs::path src = project_root / sub;
        if (fs::exists(src, ec)) {
            fs::copy(src, staging / sub,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            if (ec) {
                error = std::string("拷贝 ") + sub + " 失败: " + ec.message();
                return false;
            }
        }
    }
    fs::path dseproj = project_root / "project.dseproj";
    if (fs::exists(dseproj, ec)) {
        fs::copy_file(dseproj, staging / "project.dseproj",
                      fs::copy_options::overwrite_existing, ec);
    }
    return true;
}

int CmdBuild(const std::vector<std::string>& args, const char* argv0) {
    std::string key;
    std::string out_opt;
    std::vector<std::string> positional;
    for (const auto& a : args) {
        std::string v;
        if (MatchOption(a, "--key=", v)) {
            key = v;
        } else if (MatchOption(a, "--out=", v)) {
            out_opt = v;
        } else {
            positional.push_back(a);
        }
    }
    if (positional.empty()) {
        std::cerr << "错误: build 需要 <project>\n";
        return PrintUsage();
    }

    std::error_code ec;
    // <project> 可以是 .dseproj 文件或包含它的目录。
    fs::path project_arg(positional[0]);
    fs::path project_root = fs::is_directory(project_arg, ec) ? project_arg : project_arg.parent_path();
    if (project_root.empty()) {
        project_root = ".";
    }
    fs::path dseproj = project_root / "project.dseproj";
    if (!fs::exists(dseproj, ec)) {
        std::cerr << "错误: 未找到 project.dseproj: " << dseproj.string() << "\n";
        return 1;
    }

    const std::string game_name = fs::absolute(project_root).filename().string();
    fs::path out_dir = out_opt.empty() ? (project_root / "build" / "dist") : fs::path(out_opt);
    fs::create_directories(out_dir, ec);
    if (ec) {
        std::cerr << "错误: 无法创建输出目录: " << ec.message() << "\n";
        return 1;
    }

    // 1. 定位运行时
    fs::path runtime_exe = LocateRuntimeExe(argv0);
    if (runtime_exe.empty()) {
        std::cerr << "错误: 未找到 DSEngine_Game 运行时。请先构建 dse_standalone 目标，"
                     "或把 dse 与 DSEngine_Game 放在同一目录/其 bin 子目录下。\n";
        return 1;
    }
    std::cout << "运行时: " << runtime_exe.string() << "\n";

    // 2. 拷贝 exe（重命名为项目名）+ 同目录全部 DLL
#if defined(_WIN32)
    fs::path dest_exe = out_dir / (game_name + ".exe");
#else
    fs::path dest_exe = out_dir / game_name;
#endif
    fs::copy_file(runtime_exe, dest_exe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "错误: 拷贝运行时 exe 失败: " << ec.message() << "\n";
        return 1;
    }
    int dll_count = 0;
    for (const auto& entry : fs::directory_iterator(runtime_exe.parent_path(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dll") {
            fs::copy_file(entry.path(), out_dir / entry.path().filename(),
                          fs::copy_options::overwrite_existing, ec);
            if (!ec) ++dll_count;
            ec.clear();
        }
    }
    std::cout << "已拷贝 exe + " << dll_count << " 个 DLL\n";

    // 3. 暂存项目内容并打包加密 game.bun
    fs::path staging = out_dir / ".dse_stage";
    std::string stage_err;
    if (!StageProjectForPacking(project_root, staging, stage_err)) {
        std::cerr << "错误: " << stage_err << "\n";
        return 1;
    }
    fs::path bundle_path = out_dir / "game.bun";
    if (!dse::assets::PackDirectoryToBundle(staging.string(), bundle_path.string(), key)) {
        std::cerr << "错误: 打包 game.bun 失败\n";
        fs::remove_all(staging, ec);
        return 1;
    }
    fs::remove_all(staging, ec);
    const auto bundle_sz = fs::file_size(bundle_path, ec);
    std::cout << "已生成 " << bundle_path.string()
              << (key.empty() ? " (明文)" : " (AES 加密)")
              << "  " << bundle_sz << " bytes\n";

    // 4. 生成 launch.bat（同目录运行；运行时会自动探测 game.bun，这里显式传参更稳）
    const std::string entry_script = ReadEntryScript(dseproj, "scripts/main.lua");
#if defined(_WIN32)
    {
        std::ofstream bat(out_dir / "launch.bat", std::ios::trunc);
        bat << "@echo off\r\n";
        bat << "cd /d \"%~dp0\"\r\n";
        bat << "\"" << game_name << ".exe\""
            << " --bundle=game.bun"
            << (key.empty() ? "" : (" --key=" + key))
            << " --script=" << entry_script << "\r\n";
    }
    std::cout << "已生成 launch.bat\n";
#else
    {
        std::ofstream sh(out_dir / "launch.sh", std::ios::trunc);
        sh << "#!/bin/sh\n";
        sh << "cd \"$(dirname \"$0\")\"\n";
        sh << "./" << game_name
           << " --bundle=game.bun"
           << (key.empty() ? "" : (" --key=" + key))
           << " --script=" << entry_script << "\n";
    }
    std::cout << "已生成 launch.sh\n";
#endif

    std::cout << "Build 完成: " << fs::absolute(out_dir).string() << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return PrintUsage();
    }
    const std::string cmd = argv[1];
    std::vector<std::string> rest;
    for (int i = 2; i < argc; ++i) {
        rest.emplace_back(argv[i]);
    }

    if (cmd == "new")   return CmdNew(rest);
    if (cmd == "pack")  return CmdPack(rest);
    if (cmd == "build") return CmdBuild(rest, argv[0]);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        PrintUsage();
        return 0;
    }

    std::cerr << "错误: 未知子命令 '" << cmd << "'\n";
    return PrintUsage();
}
