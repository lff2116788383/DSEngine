/**
 * @file web_dist.h
 * @brief Web(Emscripten) 出包：收集 emscripten 产物（index.html/.js/.wasm[/.data]）
 *        到一个可直接上传（itch.io 等）的目录。纯 std::filesystem，无引擎依赖。
 *        dse CLI `dist --target web` 与单元测试共用同一份逻辑。
 */

#ifndef DSE_PROJECT_WEB_DIST_H
#define DSE_PROJECT_WEB_DIST_H

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse::project {

struct WebDistResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> files;   ///< 已收集文件的文件名（不含目录）
    std::uint64_t total_bytes = 0;    ///< 收集文件的总字节数
};

/**
 * @brief 把一次 emscripten 构建的 Web 产物收集到 out_dir。
 *
 * 必需产物：index.html、index.js、index.wasm（缺任一则失败）。
 * 可选产物：index.data（--preload-file 资源包）、index.wasm.map（调试符号）。
 * out_dir 不存在会被创建；同名文件覆盖。收集完成的目录可整目录压缩后上传 itch.io。
 *
 * @param in_dir   emscripten 产物所在目录（通常是仓库 bin/）
 * @param out_dir  输出目录（通常是 dist/web/）
 * @return ok=false 时 error 含原因；ok=true 时 files/total_bytes 记录已收集内容。
 */
DSE_EXPORT WebDistResult CollectWebDistribution(const std::string& in_dir,
                                                const std::string& out_dir);

} // namespace dse::project

#endif // DSE_PROJECT_WEB_DIST_H
