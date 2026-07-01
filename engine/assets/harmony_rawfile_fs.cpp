/**
 * @file harmony_rawfile_fs.cpp
 * @brief HarmonyRawFileSystem 实现 — rawfile 资产读取
 *
 * 仅在 __OHOS__ 下编译。
 * API 对照：
 *   AAssetManager_open()         → OH_ResourceManager_OpenRawFile()
 *   AAsset_getLength()           → OH_ResourceManager_GetRawFileSize()
 *   AAsset_read()                → OH_ResourceManager_ReadRawFile()
 *   AAsset_close()               → OH_ResourceManager_CloseRawFile()
 *   AAssetManager_openDir()      → OH_ResourceManager_OpenRawDir()
 *   AAssetDir_getNextFileName()  → OH_ResourceManager_GetRawFileName()
 *   AAssetDir_close()            → OH_ResourceManager_CloseRawDir()
 */

#ifdef __OHOS__

#include "engine/assets/harmony_rawfile_fs.h"
#include "engine/base/debug.h"

#include <rawfile/raw_file_manager.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_dir.h>

namespace dse::assets {

HarmonyRawFileSystem::HarmonyRawFileSystem(NativeResourceManager* mgr,
                                            const std::string& base_prefix)
    : mgr_(mgr), base_prefix_(base_prefix) {
    if (!base_prefix_.empty() && base_prefix_.back() != '/') {
        base_prefix_ += '/';
    }
}

std::string HarmonyRawFileSystem::FullPath(const std::string& relative) const {
    return base_prefix_ + relative;
}

bool HarmonyRawFileSystem::ReadFile(const std::string& path,
                                     std::vector<uint8_t>& out) const {
    if (!mgr_) return false;

    auto full = FullPath(path);
    RawFile* file = OH_ResourceManager_OpenRawFile(mgr_, full.c_str());
    if (!file) {
        DEBUG_LOG_ERROR("rawfile open failed: {}", full);
        return false;
    }

    long size = OH_ResourceManager_GetRawFileSize(file);
    if (size <= 0) {
        OH_ResourceManager_CloseRawFile(file);
        out.clear();
        return size == 0;
    }

    out.resize(static_cast<size_t>(size));
    int read = OH_ResourceManager_ReadRawFile(file, out.data(), size);
    OH_ResourceManager_CloseRawFile(file);

    if (read != size) {
        DEBUG_LOG_ERROR("rawfile read incomplete: {} ({}/{})", full, read, size);
        return false;
    }
    return true;
}

bool HarmonyRawFileSystem::ReadTextFile(const std::string& path,
                                         std::string& out) const {
    std::vector<uint8_t> buf;
    if (!ReadFile(path, buf)) return false;
    out.assign(reinterpret_cast<const char*>(buf.data()), buf.size());
    return true;
}

bool HarmonyRawFileSystem::Exists(const std::string& path) const {
    if (!mgr_) return false;
    auto full = FullPath(path);
    RawFile* file = OH_ResourceManager_OpenRawFile(mgr_, full.c_str());
    if (file) {
        OH_ResourceManager_CloseRawFile(file);
        return true;
    }
    // 可能是目录
    RawDir* dir = OH_ResourceManager_OpenRawDir(mgr_, full.c_str());
    if (dir) {
        OH_ResourceManager_CloseRawDir(dir);
        return true;
    }
    return false;
}

bool HarmonyRawFileSystem::IsDirectory(const std::string& path) const {
    if (!mgr_) return false;
    auto full = FullPath(path);
    RawDir* dir = OH_ResourceManager_OpenRawDir(mgr_, full.c_str());
    if (!dir) return false;
    OH_ResourceManager_CloseRawDir(dir);
    return true;
}

bool HarmonyRawFileSystem::ListDirectory(const std::string& path,
                                          std::vector<std::string>& out) const {
    if (!mgr_) return false;
    out.clear();

    auto full = FullPath(path);
    RawDir* dir = OH_ResourceManager_OpenRawDir(mgr_, full.c_str());
    if (!dir) return false;

    int count = OH_ResourceManager_GetRawFileCount(dir);
    for (int i = 0; i < count; ++i) {
        const char* name = OH_ResourceManager_GetRawFileName(dir, i);
        if (name) out.emplace_back(name);
    }

    OH_ResourceManager_CloseRawDir(dir);
    return true;
}

std::string HarmonyRawFileSystem::GetBasePath() const {
    return base_prefix_;
}

std::string HarmonyRawFileSystem::ResolvePath(const std::string& relative) const {
    return FullPath(relative);
}

} // namespace dse::assets

#endif // __OHOS__
