/**
 * @file harmony_file_system.h
 * @brief OHOS 文件路径工具 — 应用沙箱目录
 *
 * 仅在 __OHOS__ 下编译。
 * 提供 HarmonyOS 应用沙箱标准路径（files / cache / temp）。
 */

#pragma once
#ifdef __OHOS__

#include <string>

namespace dse::platform::harmony {

/// 应用持久化文件目录 (/data/storage/el2/base/files)
std::string GetFilesDir();

/// 应用缓存目录 (/data/storage/el2/base/cache)
std::string GetCacheDir();

/// 应用临时目录 (/data/storage/el2/base/temp)
std::string GetTempDir();

} // namespace dse::platform::harmony

#endif // __OHOS__
