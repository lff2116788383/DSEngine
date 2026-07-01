/**
 * @file ios_file_system.h
 * @brief iOS 文件路径工具 — NSBundle 资源路径 + Documents/Caches 可写路径
 *
 * iOS 应用沙箱约束：
 * - 只读资产从 app bundle 加载（随安装包分发）
 * - 用户存档/配置写入 Documents 目录（iCloud 备份）
 * - 流式缓存写入 Caches 目录（系统可能自动清理）
 */

#ifndef DSE_PLATFORM_IOS_FILE_SYSTEM_H
#define DSE_PLATFORM_IOS_FILE_SYSTEM_H

#ifdef DSE_ENABLE_APPLE_PLATFORM

#include <string>

namespace dse::platform::ios {

/// 只读资源路径（app bundle 内 .app/）
/// 引擎的 data/ 目录在打包时拷入 bundle，运行时从此路径加载
std::string GetBundleResourcePath();

/// 可写路径 — Documents 目录
/// iCloud 备份包含此目录，适合用户存档和配置
std::string GetDocumentsPath();

/// 可写路径 — Caches 目录
/// 系统可能在磁盘不足时清理，适合可重建的缓存（shader cache、纹理缓存等）
std::string GetCachesPath();

/// 可写路径 — tmp 目录
/// 系统可能在应用不运行时清理，适合临时文件
std::string GetTempPath();

/// 检查文件是否存在于 bundle 资源中
bool BundleFileExists(const std::string& relative_path);

} // namespace dse::platform::ios

#endif // DSE_ENABLE_APPLE_PLATFORM
#endif // DSE_PLATFORM_IOS_FILE_SYSTEM_H
