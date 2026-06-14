/**
 * @file main.cpp
 * @brief DSEngine headless CLI（OUTPUT 名 `dse`）。脱离编辑器完成：建项目模板 / 打包加密 / 完整 build。
 *
 * 用法：
 *   dse new <empty|2d|3d|lua|cpp> <dir>    # 生成项目模板
 *   dse pack <dir> <out.bun> [--key=KEY]   # 把目录打包成（可加密）资源包
 *   dse build <project> [--out=DIR] [--key=KEY]
 *                                          # 定位运行时、拷贝 exe+dll、打包加密、生成 launch.bat
 *   dse build --target web [--3d] [--debug] [--out=DIR]
 *                                          # 用 emscripten 预设编译 Web 产物并收集出包（需 EMSDK）
 *   dse dist --target web [--in=DIR] [--out=DIR]
 *                                          # 收集 emscripten 产物为可上传(itch.io)的 Web 包
 *   dse help | -h | --help                 # 显示帮助
 *
 * 与运行时端到端对应：加密 build 产物可被 DSEngine_Game 挂载解密并加载 Lua（见
 * EngineInstance::Init 的 MountBundle / lua_runtime 的 VFS searcher）。
 */

#include "engine/project/project_scaffold.h"
#include "engine/project/web_dist.h"
#include "engine/project/web_build.h"
#include "engine/assets/bundle_packer.h"
#include "engine/runtime/app_manifest.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace fs = std::filesystem;

namespace {

// 打印帮助。rc 作为返回码（无参/出错时为 1，显式 help 时为 0）。
int PrintUsage(int rc = 1) {
    std::cout <<
        "DSEngine CLI (dse) — 脱离编辑器建项目 / 打包加密 / 一键 build\n"
        "\n"
        "用法:\n"
        "  dse new <template> <dir>                 生成项目模板\n"
        "  dse pack <dir> <out.bun> [--key=KEY]     把目录打包成(可加密)资源包\n"
        "  dse build <project> [--out=DIR] [--key=KEY] [--with-swgl]\n"
        "                                           定位运行时, 拷贝 exe+dll, 打包加密, 生成 launch 脚本\n"
        "  dse build --target web [--3d] [--debug] [--preset=NAME] [--source=DIR] [--out=DIR] [--no-dist]\n"
        "                                           用 emscripten 预设配置+编译 Web 产物(默认 web-release), 并收集到 --out\n"
        "  dse dist --target web [--in=DIR] [--out=DIR]\n"
        "                                           收集 emscripten 产物(index.html/.js/.wasm[/.data])为可上传的 Web 包\n"
        "  dse dist --target win|linux [--in=DIR] [--out=ARCHIVE] [--name=NAME] [--installer] [--appimage]\n"
        "                                           把已构建游戏目录打成 .zip / .tar.gz (Export Template; 可选安装器)\n"
        "  dse help | -h | --help                   显示本帮助\n"
        "\n"
        "模板 (template):\n"
        "  empty   仅项目骨架(无脚本)\n"
        "  2d      2D 玩法 + Lua 入口脚本(相机+可移动精灵, Awake/Update)\n"
        "  3d      3D 演示场景(相机+平行光) + Lua 入口脚本\n"
        "  lua     Lua 玩法 + 入口脚本\n"
        "  cpp     C++ 宿主工程(src/main.cpp + CMakeLists.txt, 链接 dse_engine, 需自行 cmake 编译)\n"
        "\n"
        "选项:\n"
        "  --key=KEY   AES-128-CTR 密钥(>=16 字节); 省略=明文打包\n"
        "  --out=DIR   build 输出目录(默认 <project>/build)\n"
        "  --with-swgl     桌面 build: 随包发行软件 OpenGL(llvmpipe) 并在 launch 脚本设 GALLIUM_DRIVER\n"
        "  --target web|win|linux  用于 build/dist 时选择目标\n"
        "  --3d / --debug  Web 构建选 *-3d / web-debug* 预设(默认 web-release)\n"
        "  --preset=NAME   直接指定 CMake 预设(覆盖 --3d/--debug)\n"
        "  --source=DIR    仓库根(含 CMakePresets.json); 省略则自动向上探测\n"
        "  --no-dist       Web 构建后不收集出包\n"
        "  --in=DIR        dist: 已构建游戏目录(默认 build/dist)\n"
        "  --name=NAME     dist win|linux: 归档/安装器名(默认按工程名)\n"
        "  --installer     dist win: 若装有 Inno Setup(iscc) 则附带生成安装器\n"
        "  --appimage      dist linux: 若装有 appimagetool 则尝试 AppImage\n"
        "\n"
        "示例:\n"
        "  dse new lua MyGame\n"
        "  dse build MyGame --out dist --key 0123456789abcdef\n"
        "  dse pack MyGame/assets assets.bun --key=0123456789abcdef\n"
        "  dse build --target web --3d            # 编译 web-release-3d 产物并收集到 dist/web\n"
        "  dse dist --target web --out dist/web   # 之后压缩 dist/web 即可上传 itch.io\n"
        "\n"
        "注: build 不编译引擎, 而是定位同目录/bin 下已构建的 DSEngine_Game; cpp 模板需用 cmake 单独编译。\n"
        "    build --target web 例外: 它驱动 emscripten 预设真正编译 Web 产物, 需先设置 EMSDK。\n";
    return rc;
}

// 从形如 --key=VALUE 的参数中取值；命中返回 true。
bool MatchOption(const std::string& arg, const std::string& prefix, std::string& out) {
    if (arg.rfind(prefix, 0) == 0) {
        out = arg.substr(prefix.size());
        return true;
    }
    return false;
}

// 在 PATH 中是否能找到某个可执行程序（用于探测可选打包工具）。
bool ToolInPath(const std::string& tool) {
#if defined(_WIN32)
    const std::string cmd = "where " + tool + " >NUL 2>NUL";
#else
    const std::string cmd = "command -v " + tool + " >/dev/null 2>&1";
#endif
    return std::system(cmd.c_str()) == 0;
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
        std::cerr << "错误: 未知模板 '" << args[0] << "' (可选: empty|2d|3d|lua|cpp)\n";
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
    if (tmpl == dse::project::ProjectTemplate::Cpp) {
        std::cout << "提示: C++ 模板需自行编译 -> cmake -B build -DCMAKE_PREFIX_PATH=<dsengine_install_dir> && cmake --build build\n";
    }
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

// 在 cwd / exe 目录及其各级父目录中定位含 CMakePresets.json 的仓库根。
fs::path LocateSourceDir(const char* argv0) {
    std::error_code ec;
    std::vector<fs::path> starts;
    starts.push_back(fs::current_path(ec));
    try {
        starts.push_back(fs::absolute(fs::path(argv0)).parent_path());
    } catch (...) {
    }
    for (const auto& start : starts) {
        for (fs::path d = start; !d.empty(); d = d.parent_path()) {
            if (fs::exists(d / "CMakePresets.json", ec)) {
                return d;
            }
            if (d == d.root_path()) break;
        }
    }
    return {};
}

// dse build --target web：用 emscripten 预设真正配置+编译 Web 产物，
// 成功后默认复用 dist 逻辑收集为可上传包。
int CmdBuildWeb(const std::vector<std::string>& args, const char* argv0) {
    dse::project::WebBuildOptions opts;
    std::string out_opt;
    std::string source_opt;
    bool no_dist = false;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        std::string v;
        if (MatchOption(a, "--target=", v)) {
            // 已由 CmdBuild 路由确认为 web，忽略。
        } else if (a == "--target") {
            if (i + 1 < args.size()) ++i;  // 跳过其值(web)
        } else if (MatchOption(a, "--preset=", v)) {
            opts.preset = v;
        } else if (a == "--preset" && i + 1 < args.size()) {
            opts.preset = args[++i];
        } else if (MatchOption(a, "--source=", v)) {
            source_opt = v;
        } else if (a == "--source" && i + 1 < args.size()) {
            source_opt = args[++i];
        } else if (MatchOption(a, "--out=", v)) {
            out_opt = v;
        } else if (a == "--out" && i + 1 < args.size()) {
            out_opt = args[++i];
        } else if (a == "--3d") {
            opts.enable_3d = true;
        } else if (a == "--debug") {
            opts.debug = true;
        } else if (a == "--no-dist") {
            no_dist = true;
        } else {
            std::cerr << "错误: build --target web 未知选项: " << a << "\n";
            return PrintUsage();
        }
    }

    fs::path source = source_opt.empty() ? LocateSourceDir(argv0) : fs::path(source_opt);
    if (source.empty()) {
        std::cerr << "错误: 未能定位 CMakePresets.json；请用 --source=<仓库根> 指定\n";
        return 1;
    }
    opts.source_dir = source.string();

    std::cout << "Web 构建: 仓库根 " << fs::absolute(source).string() << "\n";
    dse::project::WebBuildResult res = dse::project::RunWebBuild(opts);
    std::cout << "预设: " << res.preset << "\n"
              << "  配置: " << res.configure_command << "\n"
              << "  编译: " << res.build_command << "\n";
    if (!res.ok) {
        std::cerr << "错误: " << res.error << "\n";
        return 1;
    }
    std::cout << "Web 产物已生成于 " << (source / "bin").string()
              << " (index.html/.js/.wasm[/.data])\n";

    if (no_dist) {
        std::cout << "提示: 已跳过收集；如需出包运行 `dse dist --target web`\n";
        return 0;
    }

    const fs::path out_dir = out_opt.empty() ? (source / "dist" / "web") : fs::path(out_opt);
    dse::project::WebDistResult dist =
        dse::project::CollectWebDistribution((source / "bin").string(), out_dir.string());
    if (!dist.ok) {
        std::cerr << "错误: 收集 Web 产物失败: " << dist.error << "\n";
        return 1;
    }
    std::cout << "已收集 " << dist.files.size() << " 个 Web 产物 -> "
              << fs::absolute(out_dir).string() << "  (" << dist.total_bytes << " bytes)\n";
    for (const auto& f : dist.files) {
        std::cout << "  + " << f << "\n";
    }
    std::cout << "提示: 压缩该目录(zip)即可上传 itch.io。\n";
    return 0;
}

int CmdBuild(const std::vector<std::string>& args, const char* argv0) {
    // --target web 走 emscripten 预设构建（与桌面 build 完全不同的流程）。
    for (size_t i = 0; i < args.size(); ++i) {
        std::string v;
        if (MatchOption(args[i], "--target=", v)) {
            if (v == "web") return CmdBuildWeb(args, argv0);
            std::cerr << "错误: build 暂不支持 --target " << v << " (仅 web)\n";
            return PrintUsage();
        }
        if (args[i] == "--target") {
            const std::string t = (i + 1 < args.size()) ? args[i + 1] : "";
            if (t == "web") return CmdBuildWeb(args, argv0);
            std::cerr << "错误: build 暂不支持 --target " << t << " (仅 web)\n";
            return PrintUsage();
        }
    }

    std::string key;
    std::string out_opt;
    bool with_swgl = false;
    std::vector<std::string> positional;
    for (const auto& a : args) {
        std::string v;
        if (MatchOption(a, "--key=", v)) {
            key = v;
        } else if (MatchOption(a, "--out=", v)) {
            out_opt = v;
        } else if (a == "--with-swgl") {
            with_swgl = true;
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

    // 2a. --with-swgl：确保软件 OpenGL（Mesa llvmpipe）随包发行，便于在无独显的机器
    //     （远程桌面 / VM / 无 GPU 服务器）上双击即跑。这些 DLL 若已由 setup_swgl.ps1
    //     落到运行时目录，上一步已随所有 DLL 一并拷贝；这里仅做校验与提示。
    if (with_swgl) {
        const char* swgl_dlls[] = {"opengl32.dll", "libgallium_wgl.dll", "dxil.dll"};
        bool have_core = fs::exists(out_dir / "opengl32.dll", ec) &&
                         fs::exists(out_dir / "libgallium_wgl.dll", ec);
        if (have_core) {
            int n = 0;
            for (const char* d : swgl_dlls) {
                if (fs::exists(out_dir / d, ec)) ++n;
            }
            std::cout << "已随包发行软件 OpenGL (llvmpipe): " << n << " 个 Mesa DLL\n";
        } else {
            std::cerr << "警告: --with-swgl 已指定, 但运行时目录缺少 Mesa 软件 GL DLL"
                         " (opengl32.dll/libgallium_wgl.dll)。\n"
                         "      请先运行 scripts/setup_swgl.ps1 部署 llvmpipe 到运行时 bin/ 后重试。\n";
        }
    }

    // 2b. 从 project.dseproj 的 window/splash 段生成松散 game.dsmanifest（窗口+品牌化 splash）。
    //     splash 图必须松散放置（不能进 game.bun），否则挂载资源前无法读取。
    {
        dse::runtime::AppManifest manifest;
        if (dse::runtime::LoadAppManifest(dseproj.string(), manifest)) {
            if (!manifest.has_window_title) {
                manifest.has_window_title = true;
                manifest.window_title = game_name;
            }
            // 入口脚本写入 manifest，使双击 exe（不带 --script）也能加载脚本。
            manifest.entry_script = ReadEntryScript(dseproj, "scripts/main.lua");
            manifest.has_entry_script = !manifest.entry_script.empty();
            if (manifest.has_splash && !manifest.splash.image_path.empty()) {
                fs::path src_img(manifest.splash.image_path);  // 已按 dseproj 目录解析为绝对路径
                std::error_code img_ec;
                if (fs::exists(src_img, img_ec)) {
                    fs::path dest_img = out_dir / ("splash" + src_img.extension().string());
                    fs::copy_file(src_img, dest_img, fs::copy_options::overwrite_existing, img_ec);
                    manifest.splash.image_path = img_ec ? std::string() : dest_img.filename().string();
                } else {
                    manifest.splash.image_path.clear();  // dist 中无此图，回退引擎默认
                }
            }
            if (dse::runtime::WriteAppManifest((out_dir / "game.dsmanifest").string(), manifest)) {
                std::cout << "已生成 game.dsmanifest\n";
            }
        }
    }

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
        if (with_swgl) bat << "set GALLIUM_DRIVER=llvmpipe\r\n";
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
        if (with_swgl) sh << "export GALLIUM_DRIVER=llvmpipe\n";
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

// dse dist --target win|linux：把 `dse build` / 编辑器 Build Game 产出的可运行游戏目录
// 工程化为可分发归档（Export Template）。Win 出 .zip（+ 可选 Inno/NSIS 安装器），
// Linux 出 .tar.gz（+ 可选 AppImage）。归档/安装器借助系统自带工具完成，缺失时降级并提示。
int CmdDistDesktop(const std::string& target, const std::vector<std::string>& args) {
    std::error_code ec;
    std::string in_opt, out_opt, name_opt;
    bool make_installer = false;
    bool make_appimage = false;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        std::string v;
        if (MatchOption(a, "--in=", v)) in_opt = v;
        else if (MatchOption(a, "--out=", v)) out_opt = v;
        else if (MatchOption(a, "--name=", v)) name_opt = v;
        else if (a == "--installer") make_installer = true;
        else if (a == "--appimage") make_appimage = true;
    }

    // 输入目录：默认沿用 `dse build` 的默认输出 build/dist。
    fs::path in_dir = in_opt.empty() ? (fs::path("build") / "dist") : fs::path(in_opt);
    if (!fs::exists(in_dir, ec) || !fs::is_directory(in_dir, ec)) {
        std::cerr << "错误: 可分发目录不存在: " << in_dir.string() << "\n"
                     "      请先 `dse build <project> --out " << in_dir.string()
                  << "` 产出可运行目录, 或用 --in 指定。\n";
        return 1;
    }
    const fs::path abs_in = fs::absolute(in_dir);

    // 包名：优先 --name；否则从 build/dist 结构回推工程名，再否则取目录名。
    std::string game = name_opt;
    if (game.empty()) {
        if (abs_in.filename() == "dist" && abs_in.parent_path().filename() == "build") {
            game = abs_in.parent_path().parent_path().filename().string();
        } else {
            game = abs_in.filename().string();
        }
        if (game.empty()) game = "game";
    }

    const std::string arch_suffix = (target == "win") ? "-win-x64.zip" : "-linux-x64.tar.gz";
    fs::path out_archive = out_opt.empty() ? fs::path(game + arch_suffix) : fs::path(out_opt);
    const fs::path out_parent = fs::absolute(out_archive).parent_path();
    if (!out_parent.empty()) fs::create_directories(out_parent, ec);
    const fs::path abs_out = fs::absolute(out_archive);
    if (fs::exists(abs_out, ec)) fs::remove(abs_out, ec);

    // 1. 生成归档。
    int rc = 1;
    if (target == "win") {
        if (ToolInPath("powershell")) {
            const std::string cmd =
                "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Compress-Archive -Path '" +
                (abs_in / "*").string() + "' -DestinationPath '" + abs_out.string() + "' -Force\"";
            rc = std::system(cmd.c_str());
        } else if (ToolInPath("zip")) {
            const std::string cmd = "cd \"" + abs_in.string() + "\" && zip -r -q \"" + abs_out.string() + "\" .";
            rc = std::system(cmd.c_str());
        } else {
            std::cerr << "错误: 未找到 powershell 或 zip, 无法生成 .zip。\n";
            return 1;
        }
    } else {  // linux
        if (ToolInPath("tar")) {
            const std::string cmd = "tar -czf \"" + abs_out.string() + "\" -C \"" + abs_in.string() + "\" .";
            rc = std::system(cmd.c_str());
        } else {
            std::cerr << "错误: 未找到 tar, 无法生成 .tar.gz。\n";
            return 1;
        }
    }
    if (rc != 0) {
        std::cerr << "错误: 打包失败 (rc=" << rc << ")\n";
        return 1;
    }
    std::cout << "已打包 " << abs_in.string() << " -> " << abs_out.string()
              << "  (" << (fs::exists(abs_out, ec) ? fs::file_size(abs_out, ec) : 0) << " bytes)\n";

    // 2. 可选安装器 / AppImage（缺工具则降级提示，不视为失败）。
    if (make_installer && target == "win") {
        const fs::path iss = out_parent / (game + ".iss");
        if (ToolInPath("iscc")) {
#if defined(_WIN32)
            const std::string exe_name = game + ".exe";
#else
            const std::string exe_name = game;
#endif
            std::ofstream s(iss, std::ios::trunc);
            s << "; 由 dse dist 自动生成的 Inno Setup 脚本\n"
              << "[Setup]\n"
              << "AppName=" << game << "\n"
              << "AppVersion=1.0\n"
              << "DefaultDirName={autopf}\\" << game << "\n"
              << "DefaultGroupName=" << game << "\n"
              << "OutputDir=" << out_parent.string() << "\n"
              << "OutputBaseFilename=" << game << "-setup\n"
              << "Compression=lzma2\nSolidCompression=yes\n\n"
              << "[Files]\n"
              << "Source: \"" << abs_in.string() << "\\*\"; DestDir: \"{app}\"; Flags: recursesubdirs createallsubdirs\n\n"
              << "[Icons]\n"
              << "Name: \"{group}\\" << game << "\"; Filename: \"{app}\\" << exe_name << "\"\n";
            s.close();
            const std::string cmd = "iscc \"" + iss.string() + "\"";
            if (std::system(cmd.c_str()) == 0) {
                std::cout << "已生成 Inno Setup 安装器 -> " << (out_parent / (game + "-setup.exe")).string() << "\n";
            } else {
                std::cerr << "警告: iscc 执行失败, 跳过安装器 (zip 已生成)。\n";
            }
        } else {
            std::cerr << "提示: 未找到 Inno Setup (iscc), 跳过安装器。安装 Inno Setup 后重试 --installer 即可。\n";
        }
    }
    if (make_appimage && target == "linux") {
        if (ToolInPath("appimagetool")) {
            std::cout << "提示: 检测到 appimagetool；AppImage 需要 AppDir 结构, 当前版本仅生成 tar.gz。\n";
        } else {
            std::cerr << "提示: 未找到 appimagetool, 跳过 AppImage (tar.gz 已生成)。\n";
        }
    }

    std::cout << "Dist 完成: " << abs_out.string() << "\n";
    return 0;
}

int CmdDist(const std::vector<std::string>& args, const char* argv0) {
    std::string target;
    std::string in_opt;
    std::string out_opt;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        std::string v;
        if (MatchOption(a, "--target=", v)) {
            target = v;
        } else if (a == "--target" && i + 1 < args.size()) {
            target = args[++i];
        } else if (MatchOption(a, "--in=", v)) {
            in_opt = v;
        } else if (MatchOption(a, "--out=", v)) {
            out_opt = v;
        }
    }
    if (target == "win" || target == "linux") {
        return CmdDistDesktop(target, args);
    }
    if (target != "web") {
        std::cerr << "错误: dist 仅支持 --target web|win|linux\n";
        return PrintUsage();
    }

    // 默认输入目录 = dse 自身所在目录（即 bin/，emscripten 产物与桌面产物同处 bin/）。
    std::error_code ec;
    fs::path in_dir;
    if (!in_opt.empty()) {
        in_dir = in_opt;
    } else {
        try {
            in_dir = fs::absolute(fs::path(argv0)).parent_path();
        } catch (...) {
            in_dir = fs::current_path(ec) / "bin";
        }
    }
    const fs::path out_dir = out_opt.empty() ? fs::path("dist") / "web" : fs::path(out_opt);

    dse::project::WebDistResult res =
        dse::project::CollectWebDistribution(in_dir.string(), out_dir.string());
    if (!res.ok) {
        std::cerr << "错误: " << res.error << "\n";
        return 1;
    }
    std::cout << "已收集 " << res.files.size() << " 个 Web 产物 -> "
              << fs::absolute(out_dir).string() << "  (" << res.total_bytes << " bytes)\n";
    for (const auto& f : res.files) {
        std::cout << "  + " << f << "\n";
    }
    std::cout << "提示: 压缩该目录(zip)即可上传 itch.io；本机可用 scripts/package_web.ps1 直接打 zip。\n";
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
    if (cmd == "dist")  return CmdDist(rest, argv[0]);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        return PrintUsage(0);
    }

    std::cerr << "错误: 未知子命令 '" << cmd << "'\n";
    return PrintUsage();
}
