#include "engine/assets/pak_writer.h"
#include "engine/base/debug.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace dse::pak {

bool WriteDpak(const std::string& output_path,
               const std::string& base_dir,
               const std::vector<std::string>& file_paths) {
    namespace fs = std::filesystem;

    // Collect valid files and their relative paths
    struct FileInfo {
        std::string abs_path;
        std::string rel_path;
        uint64_t    size = 0;
    };
    std::vector<FileInfo> files;
    files.reserve(file_paths.size());

    fs::path base = fs::absolute(base_dir);
    for (const auto& fp : file_paths) {
        fs::path abs = fs::absolute(fp);
        std::error_code ec;
        if (!fs::exists(abs, ec) || fs::is_directory(abs, ec)) {
            DEBUG_LOG_WARN("[PakWriter] Skipping missing/directory: {}", fp);
            continue;
        }
        auto file_size = fs::file_size(abs, ec);
        if (ec) {
            DEBUG_LOG_WARN("[PakWriter] Cannot get size for: {}", fp);
            continue;
        }

        // Compute relative path
        std::string rel = fs::relative(abs, base, ec).generic_string();
        if (ec || rel.empty()) {
            rel = abs.filename().generic_string();
        }

        if (rel.size() >= 64) {
            DEBUG_LOG_WARN("[PakWriter] Path too long (max 63 chars), skipping: {}", rel);
            continue;
        }

        files.push_back({abs.generic_string(), rel, file_size});
    }

    if (files.empty()) {
        DEBUG_LOG_ERROR("[PakWriter] No files to pack");
        return false;
    }

    // Calculate offsets
    uint32_t entry_count = static_cast<uint32_t>(files.size());
    uint64_t toc_offset  = sizeof(DpakHeader);
    uint64_t data_offset = toc_offset + entry_count * sizeof(DpakTocEntry);

    // Build TOC
    std::vector<DpakTocEntry> toc(entry_count);
    uint64_t current_data_offset = data_offset;
    for (uint32_t i = 0; i < entry_count; ++i) {
        std::memset(&toc[i], 0, sizeof(DpakTocEntry));
        std::strncpy(toc[i].path, files[i].rel_path.c_str(), 63);
        toc[i].data_offset = current_data_offset;
        toc[i].data_size   = files[i].size;
        current_data_offset += files[i].size;
    }

    // Write file
    std::ofstream out(output_path, std::ios::binary);
    if (!out.is_open()) {
        DEBUG_LOG_ERROR("[PakWriter] Cannot open output: {}", output_path);
        return false;
    }

    // Header
    DpakHeader header;
    header.entry_count = entry_count;
    header.toc_offset  = toc_offset;
    header.data_offset = data_offset;
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // TOC
    out.write(reinterpret_cast<const char*>(toc.data()),
              static_cast<std::streamsize>(toc.size() * sizeof(DpakTocEntry)));

    // Data blocks
    constexpr size_t kBufSize = 64 * 1024;
    std::vector<char> buf(kBufSize);
    for (const auto& fi : files) {
        std::ifstream in(fi.abs_path, std::ios::binary);
        if (!in.is_open()) {
            DEBUG_LOG_ERROR("[PakWriter] Cannot read: {}", fi.abs_path);
            out.close();
            fs::remove(output_path);
            return false;
        }
        uint64_t remaining = fi.size;
        while (remaining > 0) {
            auto chunk = static_cast<std::streamsize>(std::min<uint64_t>(remaining, kBufSize));
            in.read(buf.data(), chunk);
            out.write(buf.data(), in.gcount());
            remaining -= static_cast<uint64_t>(in.gcount());
        }
    }

    out.close();
    DEBUG_LOG_INFO("[PakWriter] Wrote {} files to {} ({} bytes)",
                   entry_count, output_path, current_data_offset);
    return true;
}

} // namespace dse::pak
