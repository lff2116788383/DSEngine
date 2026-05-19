/**
 * @file file_system.h
 * @brief 虚拟文件系统接口 — 抽象磁盘/APK/归档等资产读取路径
 *
 * AssetManager 通过此接口访问底层存储，桌面端使用 NativeFileSystem（std::filesystem），
 * Android 端将来替换为 AndroidAssetFileSystem（AAssetManager）。
 */

#ifndef DSE_ASSETS_FILE_SYSTEM_H
#define DSE_ASSETS_FILE_SYSTEM_H

#include <cstdint>
#include <string>
#include <vector>

namespace dse::assets {

class FileSystem {
public:
    virtual ~FileSystem() = default;

    // --- 读取 ---
    virtual bool ReadFile(const std::string& path, std::vector<uint8_t>& out_data) const = 0;
    virtual bool ReadTextFile(const std::string& path, std::string& out_text) const = 0;

    // --- 查询 ---
    virtual bool Exists(const std::string& path) const = 0;
    virtual bool IsDirectory(const std::string& path) const = 0;
    virtual bool ListDirectory(const std::string& path, std::vector<std::string>& out_entries) const = 0;

    // --- 路径工具 ---
    virtual std::string GetBasePath() const = 0;
    virtual std::string ResolvePath(const std::string& relative) const = 0;
};

} // namespace dse::assets

#endif // DSE_ASSETS_FILE_SYSTEM_H
