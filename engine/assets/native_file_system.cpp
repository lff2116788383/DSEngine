/**
 * @file native_file_system.cpp
 * @brief 桌面端原生文件系统实现
 */

#include "engine/assets/native_file_system.h"

#include <filesystem>
#include <fstream>

namespace dse::assets {

NativeFileSystem::NativeFileSystem(const std::string& base_path)
    : base_path_(base_path) {
}

bool NativeFileSystem::ReadFile(const std::string& path, std::vector<uint8_t>& out_data) const {
    const std::string resolved = ResolvePath(path);
    std::ifstream file(resolved, std::ios::binary | std::ios::ate);
    if (!file) return false;

    const std::streamsize size = file.tellg();
    if (size < 0) return false;

    file.seekg(0, std::ios::beg);
    out_data.resize(static_cast<std::size_t>(size));
    if (size == 0) return true;

    return static_cast<bool>(file.read(reinterpret_cast<char*>(out_data.data()), size));
}

bool NativeFileSystem::ReadTextFile(const std::string& path, std::string& out_text) const {
    const std::string resolved = ResolvePath(path);
    std::ifstream file(resolved);
    if (!file) return false;

    out_text.assign(std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>());
    return true;
}

bool NativeFileSystem::Exists(const std::string& path) const {
    const std::string resolved = ResolvePath(path);
    std::error_code ec;
    return std::filesystem::exists(resolved, ec);
}

bool NativeFileSystem::IsDirectory(const std::string& path) const {
    const std::string resolved = ResolvePath(path);
    std::error_code ec;
    return std::filesystem::is_directory(resolved, ec);
}

bool NativeFileSystem::ListDirectory(const std::string& path,
                                     std::vector<std::string>& out_entries) const {
    const std::string resolved = ResolvePath(path);
    std::error_code ec;
    if (!std::filesystem::is_directory(resolved, ec)) return false;

    out_entries.clear();
    for (const auto& entry : std::filesystem::directory_iterator(resolved, ec)) {
        out_entries.push_back(entry.path().filename().generic_string());
    }
    return !ec;
}

std::string NativeFileSystem::GetBasePath() const {
    return base_path_;
}

std::string NativeFileSystem::ResolvePath(const std::string& relative) const {
    if (base_path_.empty()) return relative;

    const std::filesystem::path rel(relative);
    if (rel.is_absolute()) return relative;

    return (std::filesystem::path(base_path_) / rel).lexically_normal().string();
}

void NativeFileSystem::SetBasePath(const std::string& base_path) {
    base_path_ = base_path;
}

} // namespace dse::assets
