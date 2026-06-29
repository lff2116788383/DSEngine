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
inline long long PakTell64(FILE* f) {
#if defined(_WIN32)
    return _ftelli64(f);
#else
    return static_cast<long long>(ftello(f));
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

    // 先量出文件总大小，供下方 TOC / 数据块的偏移+长度做区间校验，
    // 防止畸形/损坏的 .dpak 用未校验的 entry_count / data_offset / data_size 触发超大分配或越界读。
    if (PakSeek64(file_, 0, SEEK_END) != 0) {
        DEBUG_LOG_ERROR("[PakReader] Failed to seek to end: {}", pak_path);
        Close();
        return false;
    }
    const long long file_size_ll = PakTell64(file_);
    if (file_size_ll < static_cast<long long>(sizeof(DpakHeader)) ||
        PakSeek64(file_, 0, SEEK_SET) != 0) {
        DEBUG_LOG_ERROR("[PakReader] File too small or unseekable: {}", pak_path);
        Close();
        return false;
    }
    const uint64_t file_size = static_cast<uint64_t>(file_size_ll);

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

    // 校验 TOC 区间完整落在文件内：toc_offset 合法、且 entry_count 条 80 字节项不越界。
    // 用「文件剩余容量 / 单项大小」反推 entry_count 上限，既防越界又天然把 resize 夹到文件能容纳的量。
    if (header.toc_offset > file_size) {
        DEBUG_LOG_ERROR("[PakReader] TOC offset {} beyond file size {}", header.toc_offset, file_size);
        Close();
        return false;
    }
    const uint64_t max_entries = (file_size - header.toc_offset) / sizeof(DpakTocEntry);
    if (header.entry_count > max_entries) {
        DEBUG_LOG_ERROR("[PakReader] entry_count {} exceeds file capacity {} (corrupt)",
                        header.entry_count, max_entries);
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
        // 每条数据块必须完整落在文件内（offset + size 不溢出、不越界），否则判损坏。
        if (raw.data_offset > file_size || raw.data_size > file_size - raw.data_offset) {
            DEBUG_LOG_ERROR("[PakReader] TOC entry {} data range [{}, +{}) out of file size {} (corrupt)",
                            i, raw.data_offset, raw.data_size, file_size);
            Close();
            return false;
        }
        raw.path[63] = '\0'; // ensure null-terminated
        entries_[i].path        = raw.path;
        entries_[i].data_offset = raw.data_offset;
        entries_[i].data_size   = raw.data_size;
        index_[entries_[i].path] = i;
    }

    file_size_ = file_size;
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
    file_size_ = 0;
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

    // 防御：即便 TOC 已在 Open 校验过，这里再确认区间未越界后才分配，杜绝任何路径下的超大分配/越界读。
    if (entry.data_offset > file_size_ || entry.data_size > file_size_ - entry.data_offset) {
        DEBUG_LOG_ERROR("[PakReader] Entry {} data range out of file bounds", relative_path);
        return false;
    }

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
