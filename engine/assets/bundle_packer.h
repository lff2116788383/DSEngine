/**
 * @file bundle_packer.h
 * @brief 目录 → .bun 资源包的自由函数。编辑器、CLI 与 AssetManager 共用同一份打包逻辑。
 */

#ifndef DSE_ASSETS_BUNDLE_PACKER_H
#define DSE_ASSETS_BUNDLE_PACKER_H

#include <string>
#include "engine/core/dse_export.h"

namespace dse::assets {

/**
 * @brief 将一个目录递归打包成 .bun 资源包（zip 压缩，可选 AES-CTR 加密）。
 *
 * 与 AssetManager::MountBundle 端到端对应：相同 key 打包/挂载即可在运行时解密并读取。
 *
 * @param input_dir      要打包的目录（递归收集其下全部常规文件，路径相对 input_dir 存储）
 * @param output_bundle  输出 .bun 文件路径
 * @param aes_key        AES 密钥；空字符串或长度 < 16 表示不加密。仅使用前 16 字节（AES-128-CTR）。
 * @return 成功返回 true
 */
DSE_EXPORT bool PackDirectoryToBundle(const std::string& input_dir,
                                      const std::string& output_bundle,
                                      const std::string& aes_key);

} // namespace dse::assets

#endif // DSE_ASSETS_BUNDLE_PACKER_H
