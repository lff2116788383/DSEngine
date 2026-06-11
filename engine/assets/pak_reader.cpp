#include "engine/assets/pak_reader.h"
#include "engine/base/debug.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace dse::pak {

// 跨平台 64 位文件定位：MSVC 用 _fseeki64，POSIX 用 fseeko（off_t 为 64 位）
namespace {
inline int PakSeek64(FILE* f, long long offset, int origin) {
#if defined(_WIN32)
    return _fseeki64(f, offset, origin);
#else
    return fseeko(f, static_cast<off_t>(offset), origin);
#endif
}
} // namespace

PakReader::~PakReader() {
    Close();
}

bool PakReader::Open(const std::string& pak_path) {
    Close();

#if defined(_WIN32)
    errno_t err = fopen_s(&file_, pak_path.c_str(), "rb");
    if (err != 0 || !file_) {
#else
    file_ = fopen(pak_path.c_str(), "rb");
    if (!file_) {
#endif
        DEBUG_LOG_ERROR("[PakReader] Cannot open: {}", pak_path);
        return false;
    }

    // Read header
    DpakHeader header;
    if (fread(&header, sizeof(header), 1, file_) != 1) {
        DEBUG_LOG_ERROR("[PakReader] Failed to read header: {}", pak_path);
        Close();
        return false;
    }

    if (header.magic != kDpakMagic) {
        DEBUG_LOG_ERROR("[PakReader] Invalid magic: expected 0x{:08X}, got 0x{:08X}",
                        kDpakMagic, header.magic);
        Close();
        return false;
    }

    if (header.version != kDpakVersion) {
        DEBUG_LOG_ERROR("[PakReader] Unsupported version: {}", header.version);
        Close();
        return false;
    }

    // Read TOC
    if (PakSeek64(file_, static_cast<long long>(header.toc_offset), SEEK_SET) != 0) {
        DEBUG_LOG_ERROR("[PakReader] Failed to seek to TOC");
        Close();
        return false;
    }

    entries_.resize(header.entry_count);
    index_.reserve(header.entry_count);

    for (uint32_t i = 0; i < header.entry_count; ++i) {
        DpakTocEntry raw;
        if (fread(&raw, sizeof(raw), 1, file_) != 1) {
            DEBUG_LOG_ERROR("[PakReader] Failed to read TOC entry {}", i);
            Close();
            return false;
        }
        raw.path[63] = '\0'; // ensure null-terminated
        entries_[i].path        = raw.path;
        entries_[i].data_offset = raw.data_offset;
        entries_[i].data_size   = raw.data_size;
        index_[entries_[i].path] = i;
    }

    pak_path_ = pak_path;
    DEBUG_LOG_INFO("[PakReader] Opened {} ({} entries)", pak_path, header.entry_count);
    return true;
}

void PakReader::Close() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
    entries_.clear();
    index_.clear();
    pak_path_.clear();
}

bool PakReader::Contains(const std::string& relative_path) const {
    return index_.find(relative_path) != index_.end();
}

bool PakReader::ReadFile(const std::string& relative_path, std::vector<uint8_t>& out_data) const {
    auto it = index_.find(relative_path);
    if (it == index_.end()) {
        return false;
    }

    const auto& entry = entries_[it->second];
    if (!file_) return false;

    if (PakSeek64(file_, static_cast<long long>(entry.data_offset), SEEK_SET) != 0) {
        DEBUG_LOG_ERROR("[PakReader] Failed to seek to data for: {}", relative_path);
        return false;
    }

    out_data.resize(static_cast<size_t>(entry.data_size));
    size_t read = fread(out_data.data(), 1, static_cast<size_t>(entry.data_size), file_);
    if (read != static_cast<size_t>(entry.data_size)) {
        DEBUG_LOG_ERROR("[PakReader] Short read for {}: {} / {}", relative_path, read, entry.data_size);
        out_data.clear();
        return false;
    }

    return true;
}

} // namespace dse::pak
