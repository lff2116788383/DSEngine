/**
 * @file ios_file_system.mm
 * @brief iOS 文件路径工具实现
 *
 * 封装 NSBundle / NSSearchPathForDirectoriesInDomains 等 Foundation API，
 * 返回 std::string 供引擎 AssetManager 使用。
 */

#ifdef DSE_ENABLE_APPLE_PLATFORM

#import <Foundation/Foundation.h>
#include "engine/platform/ios/ios_file_system.h"

namespace dse::platform::ios {

std::string GetBundleResourcePath() {
    @autoreleasepool {
        NSString* path = [[NSBundle mainBundle] resourcePath];
        return path ? std::string([path UTF8String]) : std::string();
    }
}

std::string GetDocumentsPath() {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES);
        if (paths.count > 0) {
            return std::string([paths[0] UTF8String]);
        }
        return std::string();
    }
}

std::string GetCachesPath() {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(
            NSCachesDirectory, NSUserDomainMask, YES);
        if (paths.count > 0) {
            return std::string([paths[0] UTF8String]);
        }
        return std::string();
    }
}

std::string GetTempPath() {
    @autoreleasepool {
        NSString* path = NSTemporaryDirectory();
        return path ? std::string([path UTF8String]) : std::string();
    }
}

bool BundleFileExists(const std::string& relative_path) {
    @autoreleasepool {
        NSString* resource_path = [[NSBundle mainBundle] resourcePath];
        if (!resource_path) return false;

        NSString* full_path = [resource_path stringByAppendingPathComponent:
            [NSString stringWithUTF8String:relative_path.c_str()]];

        return [[NSFileManager defaultManager] fileExistsAtPath:full_path];
    }
}

} // namespace dse::platform::ios

#endif // DSE_ENABLE_APPLE_PLATFORM
