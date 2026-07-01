/**
 * @file harmony_rawfile_fs.h
 * @brief HarmonyOS rawfile 资产文件系统 — FileSystem 接口实现
 *
 * 仅在 __OHOS__ 下编译。
 * 使用 NativeResourceManager + OH_ResourceManager_* API 读取 rawfile 资产。
 * 等价于 Android 的 AndroidAssetFileSystem（使用 AAssetManager）。
 *
 * 设计决策：与 AndroidAssetFileSystem 结构相似但底层 API 完全不同
 * （AAsset_* vs OH_ResourceManager_*），强行抽象共享基类会增加不必要的复杂度。
 * 这是有意识的代码结构重复，不视为技术债。
 */

#pragma once
#ifdef __OHOS__

#include "engine/assets/file_system.h"

#include <string>
#include <vector>

struct NativeResourceManager;

namespace dse::assets {

class HarmonyRawFileSystem final : public FileSystem {
public:
    /**
     * @param mgr          NativeResourceManager（由 NAPI 注入）
     * @param base_prefix  资产路径前缀（默认空 = rawfile 根目录）
     */
    explicit HarmonyRawFileSystem(NativeResourceManager* mgr,
                                  const std::string& base_prefix = "");

    ~HarmonyRawFileSystem() override = default;

    bool ReadFile(const std::string& path,
                  std::vector<uint8_t>& out) const override;
    bool ReadTextFile(const std::string& path,
                      std::string& out) const override;
    bool Exists(const std::string& path) const override;
    bool IsDirectory(const std::string& path) const override;
    bool ListDirectory(const std::string& path,
                       std::vector<std::string>& out) const override;
    std::string GetBasePath() const override;
    std::string ResolvePath(const std::string& relative) const override;

private:
    NativeResourceManager* mgr_;
    std::string base_prefix_;

    std::string FullPath(const std::string& relative) const;
};

} // namespace dse::assets

#endif // __OHOS__
