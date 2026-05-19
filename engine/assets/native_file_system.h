/**
 * @file native_file_system.h
 * @brief 桌面端原生文件系统实现（Windows/Linux/macOS）
 */

#ifndef DSE_ASSETS_NATIVE_FILE_SYSTEM_H
#define DSE_ASSETS_NATIVE_FILE_SYSTEM_H

#include "engine/assets/file_system.h"

namespace dse::assets {

class NativeFileSystem final : public FileSystem {
public:
    explicit NativeFileSystem(const std::string& base_path = "");
    ~NativeFileSystem() override = default;

    bool ReadFile(const std::string& path, std::vector<uint8_t>& out_data) const override;
    bool ReadTextFile(const std::string& path, std::string& out_text) const override;

    bool Exists(const std::string& path) const override;
    bool IsDirectory(const std::string& path) const override;
    bool ListDirectory(const std::string& path, std::vector<std::string>& out_entries) const override;

    std::string GetBasePath() const override;
    std::string ResolvePath(const std::string& relative) const override;

    void SetBasePath(const std::string& base_path);

private:
    std::string base_path_;
};

} // namespace dse::assets

#endif // DSE_ASSETS_NATIVE_FILE_SYSTEM_H
