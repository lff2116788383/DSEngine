/**
 * @file sha256.cpp
 * @brief SHA256::HashFile 实现（需要文件 IO）
 */

#include "engine/assets/sha256.h"
#include <fstream>

namespace dse {
namespace assets {

std::string SHA256::HashFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};

    SHA256 ctx;
    char buf[8192];
    while (f.read(buf, sizeof(buf))) {
        ctx.Update(buf, sizeof(buf));
    }
    if (f.gcount() > 0) {
        ctx.Update(buf, static_cast<size_t>(f.gcount()));
    }
    return DigestToHex(ctx.Finalize());
}

} // namespace assets
} // namespace dse
