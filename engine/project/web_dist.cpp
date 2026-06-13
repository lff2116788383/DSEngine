/**
 * @file web_dist.cpp
 * @brief CollectWebDistribution 实现 — 见 web_dist.h。
 */

#include "engine/project/web_dist.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace dse::project {

namespace {
// emcc 把宿主 OUTPUT_NAME 设为 "index"（见 apps/web_host/CMakeLists.txt），
// 故产物固定以 index.* 命名。
const char* const kRequired[] = {"index.html", "index.js", "index.wasm"};
const char* const kOptional[] = {"index.data", "index.wasm.map"};
} // namespace

WebDistResult CollectWebDistribution(const std::string& in_dir,
                                     const std::string& out_dir) {
    WebDistResult result;
    std::error_code ec;

    const fs::path in_path(in_dir);
    if (!fs::is_directory(in_path, ec)) {
        result.error = "输入目录不存在: " + in_dir;
        return result;
    }

    // 必需产物缺失则直接失败（不创建空的输出目录）。
    std::string missing;
    for (const char* name : kRequired) {
        if (!fs::exists(in_path / name, ec)) {
            if (!missing.empty()) missing += ", ";
            missing += name;
        }
    }
    if (!missing.empty()) {
        result.error = "缺少 Web 产物 (" + missing +
                       ")；请先用 web-release/web-debug 预设构建 dse_web_host";
        return result;
    }

    const fs::path out_path(out_dir);
    fs::create_directories(out_path, ec);
    if (ec) {
        result.error = "无法创建输出目录: " + ec.message();
        return result;
    }

    auto copy_one = [&](const char* name) {
        const fs::path src = in_path / name;
        if (!fs::exists(src, ec)) return;  // 可选产物允许缺失
        fs::copy_file(src, out_path / name, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            ec.clear();
            return;
        }
        result.files.emplace_back(name);
        result.total_bytes += fs::file_size(src, ec);
        if (ec) ec.clear();
    };

    for (const char* name : kRequired) copy_one(name);
    for (const char* name : kOptional) copy_one(name);

    result.ok = true;
    return result;
}

} // namespace dse::project
