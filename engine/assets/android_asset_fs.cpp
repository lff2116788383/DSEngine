/**
 * @file android_asset_fs.cpp
 * @brief AndroidAssetFileSystem 实现 — AAssetManager 读取 APK assets/
 *
 * 仅在 __ANDROID__ 下编译（PC 构建中此文件被 CMake 过滤排除）。
 */

#ifdef __ANDROID__

#include "engine/assets/android_asset_fs.h"

#include <android/asset_manager.h>
#include <android/log.h>

#define ALOG_TAG "DSEngine"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, ALOG_TAG, __VA_ARGS__)

namespace dse::assets {

AndroidAssetFileSystem::AndroidAssetFileSystem(AAssetManager* mgr,
                                               const std::string& base_prefix)
    : mgr_(mgr), base_prefix_(base_prefix) {
}

// ─── 路径构造 ─────────────────────────────────────────────────

std::string AndroidAssetFileSystem::MakePath(const std::string& path) const {
    if (base_prefix_.empty()) return path;
    // 避免双斜杠
    if (!base_prefix_.empty() && base_prefix_.back() == '/')
        return base_prefix_ + path;
    return base_prefix_ + "/" + path;
}

// ─── 读取 ─────────────────────────────────────────────────────

bool AndroidAssetFileSystem::ReadFile(const std::string& path,
                                      std::vector<uint8_t>& out_data) const {
    if (!mgr_) return false;
    const std::string full = MakePath(path);
    AAsset* asset = AAssetManager_open(mgr_, full.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        ALOGE("AndroidAssetFS: cannot open '%s'", full.c_str());
        return false;
    }
    const off_t size = AAsset_getLength(asset);
    out_data.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        AAsset_read(asset, out_data.data(), static_cast<std::size_t>(size));
    }
    AAsset_close(asset);
    return true;
}

bool AndroidAssetFileSystem::ReadTextFile(const std::string& path,
                                          std::string& out_text) const {
    std::vector<uint8_t> buf;
    if (!ReadFile(path, buf)) return false;
    out_text.assign(reinterpret_cast<const char*>(buf.data()), buf.size());
    return true;
}

// ─── 查询 ─────────────────────────────────────────────────────

bool AndroidAssetFileSystem::Exists(const std::string& path) const {
    if (!mgr_) return false;
    const std::string full = MakePath(path);

    // 尝试作为文件打开
    AAsset* asset = AAssetManager_open(mgr_, full.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) { AAsset_close(asset); return true; }

    // 尝试作为目录打开
    AAssetDir* dir = AAssetManager_openDir(mgr_, full.c_str());
    if (dir) { AAssetDir_close(dir); return true; }

    return false;
}

bool AndroidAssetFileSystem::IsDirectory(const std::string& path) const {
    if (!mgr_) return false;
    const std::string full = MakePath(path);
    AAssetDir* dir = AAssetManager_openDir(mgr_, full.c_str());
    if (!dir) return false;
    AAssetDir_close(dir);
    return true;
}

bool AndroidAssetFileSystem::ListDirectory(const std::string& path,
                                           std::vector<std::string>& out_entries) const {
    if (!mgr_) return false;
    const std::string full = MakePath(path);
    AAssetDir* dir = AAssetManager_openDir(mgr_, full.c_str());
    if (!dir) return false;

    out_entries.clear();
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
        out_entries.emplace_back(name);
    }
    AAssetDir_close(dir);
    return true;
}

// ─── 路径工具 ─────────────────────────────────────────────────

std::string AndroidAssetFileSystem::GetBasePath() const {
    return base_prefix_;
}

std::string AndroidAssetFileSystem::ResolvePath(const std::string& relative) const {
    return MakePath(relative);
}

} // namespace dse::assets

#endif // __ANDROID__
