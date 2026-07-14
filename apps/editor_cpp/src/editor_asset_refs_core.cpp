/**
 * @file editor_asset_refs_core.cpp
 * @brief editor_asset_refs_core.h 的实现（纯文件系统 + rapidjson，无编辑器运行时依赖）。
 */

#include "editor_asset_refs_core.h"

#include <algorithm>
#include <fstream>
#include <system_error>

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace dse::editor {

std::string NormalizeAssetRefPath(const std::string& path) {
    std::string s = path;
    std::replace(s.begin(), s.end(), '\\', '/');
    // 去掉前导 "./"
    while (s.size() >= 2 && s[0] == '.' && s[1] == '/') {
        s.erase(0, 2);
    }
    return s;
}

int RewriteJsonPathRefs(rapidjson::Value& value,
                        const std::string& old_rel,
                        const std::string& new_rel,
                        rapidjson::Document::AllocatorType& alloc) {
    int count = 0;
    if (value.IsString()) {
        if (NormalizeAssetRefPath(value.GetString()) == old_rel) {
            value.SetString(new_rel.c_str(), static_cast<rapidjson::SizeType>(new_rel.size()), alloc);
            ++count;
        }
    } else if (value.IsArray()) {
        for (auto& e : value.GetArray()) {
            count += RewriteJsonPathRefs(e, old_rel, new_rel, alloc);
        }
    } else if (value.IsObject()) {
        for (auto& m : value.GetObject()) {
            count += RewriteJsonPathRefs(m.value, old_rel, new_rel, alloc);
        }
    }
    return count;
}

bool RewriteFileJsonPathRefs(const std::filesystem::path& file,
                             const std::string& old_rel,
                             const std::string& new_rel,
                             int& out_count) {
    out_count = 0;
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || (!doc.IsObject() && !doc.IsArray())) {
        return false;  // 非 JSON 资产（如二进制 .dmesh / 缓存），跳过
    }

    const int n = RewriteJsonPathRefs(doc, old_rel, new_rel, doc.GetAllocator());
    if (n == 0) {
        out_count = 0;
        return true;  // 已处理，无引用需改写
    }

    rapidjson::StringBuffer buf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    std::ofstream ofs(file, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs << buf.GetString();
    out_count = n;
    return true;
}

MoveAssetResult MoveAssetWithFixup(const std::filesystem::path& asset_root,
                                   const std::string& old_rel_in,
                                   const std::string& new_rel_in) {
    MoveAssetResult result;
    const std::string old_rel = NormalizeAssetRefPath(old_rel_in);
    const std::string new_rel = NormalizeAssetRefPath(new_rel_in);

    if (old_rel.empty() || new_rel.empty()) {
        result.errors.push_back("empty source or destination path");
        return result;
    }
    if (old_rel == new_rel) {
        result.errors.push_back("source and destination are identical");
        return result;
    }

    std::error_code ec;
    const std::filesystem::path src = asset_root / old_rel;
    const std::filesystem::path dst = asset_root / new_rel;

    if (!std::filesystem::exists(src, ec)) {
        result.errors.push_back("source asset does not exist: " + old_rel);
        return result;
    }
    if (std::filesystem::exists(dst, ec)) {
        result.errors.push_back("destination already exists: " + new_rel);
        return result;
    }

    // 保留 GUID：读取伴随 .meta（若有），移动后原样写到新位置。
    const std::filesystem::path src_meta(src.string() + ".meta");
    std::string meta_content;
    bool has_meta = false;
    if (std::filesystem::exists(src_meta, ec)) {
        std::ifstream mf(src_meta, std::ios::binary);
        if (mf) {
            meta_content.assign((std::istreambuf_iterator<char>(mf)),
                                std::istreambuf_iterator<char>());
            has_meta = true;
            rapidjson::Document mdoc;
            if (!mdoc.Parse(meta_content.c_str()).HasParseError() && mdoc.IsObject() &&
                mdoc.HasMember("guid") && mdoc["guid"].IsString()) {
                result.guid = mdoc["guid"].GetString();
            }
        }
    }

    // 建目标目录并移动资产文件。
    std::filesystem::create_directories(dst.parent_path(), ec);
    std::filesystem::rename(src, dst, ec);
    if (ec) {
        result.errors.push_back("failed to move asset: " + ec.message());
        return result;
    }

    // 移动 .meta（保留 GUID）。
    if (has_meta) {
        const std::filesystem::path dst_meta(dst.string() + ".meta");
        std::ofstream mof(dst_meta, std::ios::binary | std::ios::trunc);
        if (mof) mof << meta_content;
        mof.close();
        std::filesystem::remove(src_meta, ec);
    }

    result.ok = true;

    // 引用修正：遍历 asset_root 下所有常规文件，改写 JSON 中的旧路径引用。
    for (auto& entry : std::filesystem::recursive_directory_iterator(asset_root, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!entry.is_regular_file(ec)) continue;
        const std::filesystem::path& p = entry.path();
        if (p.extension() == ".meta") continue;  // .meta 仅含 guid/type，无路径引用
        int n = 0;
        if (RewriteFileJsonPathRefs(p, old_rel, new_rel, n)) {
            result.references_rewritten += n;
        }
    }

    return result;
}

}  // namespace dse::editor
