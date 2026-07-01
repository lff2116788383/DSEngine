/**
 * @file harmony_file_system.cpp
 * @brief OHOS 文件路径工具实现
 *
 * 仅在 __OHOS__ 下编译。
 */

#ifdef __OHOS__

#include "engine/platform/harmony/harmony_file_system.h"

namespace dse::platform::harmony {

std::string GetFilesDir() {
    return "/data/storage/el2/base/files";
}

std::string GetCacheDir() {
    return "/data/storage/el2/base/cache";
}

std::string GetTempDir() {
    return "/data/storage/el2/base/temp";
}

} // namespace dse::platform::harmony

#endif // __OHOS__
