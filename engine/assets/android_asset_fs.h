/**
 * @file android_asset_fs.h
 * @brief Android APK 资产文件系统实现（AAssetManager）
 *
 * 仅在 __ANDROID__ 下编译。
 * 读取 APK assets/ 目录内的文件，替代桌面端的 NativeFileSystem。
 */

#pragma once
#ifdef __ANDROID__

#include "engine/assets/file_system.h"

struct AAssetManager;

namespace dse::assets {

class AndroidAssetFileSystem final : public FileSystem {
public:
    explicit AndroidAssetFileSystem(AAssetManager* mgr, const std::string& base_prefix = "");
    ~AndroidAssetFileSystem() override = default;

    bool ReadFile(const std::string& path, std::vector<uint8_t>& out_data) const override;
    bool ReadTextFile(const std::string& path, std::string& out_text) const override;

    bool Exists(const std::string& path) const override;
    bool IsDirectory(const std::string& path) const override;
    bool ListDirectory(const std::string& path, std::vector<std::string>& out_entries) const override;

    std::string GetBasePath() const override;
    std::string ResolvePath(const std::string& relative) const override;

private:
    AAssetManager* mgr_         = nullptr;
    std::string    base_prefix_;  // 可选前缀，如 "data/" 对应 assets/data/

    std::string MakePath(const std::string& path) const;
};

} // namespace dse::assets

#endif // __ANDROID__
