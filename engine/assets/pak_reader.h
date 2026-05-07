#pragma once

#include "engine/assets/pak_format.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace dse::pak {

/// Reads assets from a .dpak archive. Thread-safe for concurrent reads.
class PakReader {
public:
    PakReader() = default;
    ~PakReader();

    PakReader(const PakReader&) = delete;
    PakReader& operator=(const PakReader&) = delete;

    /// Open a .dpak file and parse its TOC.
    bool Open(const std::string& pak_path);

    /// Close the archive.
    void Close();

    /// Check if a file exists in the archive.
    bool Contains(const std::string& relative_path) const;

    /// Read a file's contents into a byte buffer.
    /// @return true on success.
    bool ReadFile(const std::string& relative_path, std::vector<uint8_t>& out_data) const;

    /// Get the list of all entries.
    const std::vector<PakEntry>& GetEntries() const { return entries_; }

    /// Check if archive is open.
    bool IsOpen() const { return file_ != nullptr; }

    /// Get the path of the opened .dpak file.
    const std::string& GetPath() const { return pak_path_; }

private:
    std::string pak_path_;
    FILE* file_ = nullptr;
    std::vector<PakEntry> entries_;
    std::unordered_map<std::string, size_t> index_; // path -> entries_ index
};

} // namespace dse::pak
