/**
 * @file bundle_packer.cpp
 * @brief 目录 → .bun 资源包的自由函数实现。
 */

#include "engine/assets/bundle_packer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "bundle/bundle.h"
extern "C" {
#include "aes.h"
}

namespace dse::assets {

bool PackDirectoryToBundle(const std::string& input_dir,
                           const std::string& output_bundle,
                           const std::string& aes_key) {
    std::error_code ec;
    if (!std::filesystem::exists(input_dir, ec) || ec) {
        return false;
    }

    bundle::archive pak;
    int idx = 0;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(input_dir, ec)) {
        if (ec) {
            return false;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
        if (!file) {
            continue;
        }
        std::streamsize size = file.tellg();
        if (size < 0) {
            continue;
        }
        file.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (size != 0 && !file.read(content.data(), size)) {
            continue;
        }
        pak.resize(idx + 1);
        std::string rel_path = std::filesystem::relative(entry.path(), input_dir).generic_string();
        std::replace(rel_path.begin(), rel_path.end(), '\\', '/');
        pak[idx]["name"] = rel_path;
        pak[idx]["data"] = content;
        ++idx;
    }

    std::string bin = pak.zip(60); // 压缩级别 60

    if (!aes_key.empty() && aes_key.size() >= 16) {
        struct AES_ctx ctx;
        uint8_t iv[16] = {0}; // 固定 IV：与 MountBundle 解密保持一致
        AES_init_ctx_iv(&ctx, reinterpret_cast<const uint8_t*>(aes_key.c_str()), iv);
        AES_CTR_xcrypt_buffer(&ctx, reinterpret_cast<uint8_t*>(bin.data()), bin.size());
    }

    std::ofstream out(output_bundle, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(bin.data(), bin.size());
    return out.good();
}

} // namespace dse::assets
