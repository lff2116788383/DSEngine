/**
 * @file editor_asset_refs_core.h
 * @brief 资产引用修正（rename/move reference fixup）的纯逻辑，无 ProjectManager /
 *        ImGui / 单例依赖，供无头 gtest 直接链接测试。
 *
 * DSEngine 的场景 / 预制体等以「相对资产根的路径字符串」引用其它资产
 * （如 MeshRendererComponent.mesh_path、Animator3DComponent.dskel_path/danim_path、
 *  Sky.cubemap_path、Terrain.heightmap_path 等）。当某资产被重命名 / 移动时，
 * 这些引用必须随之更新，否则场景重开会丢失引用。本模块提供确定性、可无头验证的
 * 路径引用重写：递归遍历 JSON 字符串值，对「规范化后完全等于旧相对路径」的值改写为
 * 新相对路径（不做子串替换，避免误伤 a.dmesh / a.dmesh2 这类前缀相同的路径）。
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace dse::editor {

/// 把路径分隔符统一为正斜杠，去掉多余的前导 "./"，用于引用比较与索引。
std::string NormalizeAssetRefPath(const std::string& path);

/// 递归遍历 JSON 值，把「规范化后完全等于 old_rel」的字符串值改写为 new_rel。
/// 返回改写的字符串值个数。仅精确匹配整条相对路径，不做子串替换。
int RewriteJsonPathRefs(rapidjson::Value& value,
                        const std::string& old_rel,
                        const std::string& new_rel,
                        rapidjson::Document::AllocatorType& alloc);

/// 读取单个文件；若为合法 JSON 则重写其中的路径引用并原样（pretty）写回。
/// out_count 置为改写的引用数。返回：true=已处理（含 0 改写）；
/// false=文件无法读取 / 非 JSON（跳过，非致命）。
bool RewriteFileJsonPathRefs(const std::filesystem::path& file,
                             const std::string& old_rel,
                             const std::string& new_rel,
                             int& out_count);

struct MoveAssetResult {
    bool ok = false;                    ///< 磁盘移动是否成功
    int references_rewritten = 0;       ///< 跨文件改写的引用总数
    std::string guid;                   ///< 从 .meta 保留下来的 GUID（无 .meta 时为空）
    std::vector<std::string> errors;
};

/// 将资产文件及其伴随的 .meta 从 old_rel 移动到 new_rel（均相对 asset_root），
/// 保留 GUID，然后遍历 asset_root 下所有 JSON 资产文件把旧路径引用改写为新路径。
/// 纯文件系统操作，可无头测试（只需一个临时目录）。
MoveAssetResult MoveAssetWithFixup(const std::filesystem::path& asset_root,
                                   const std::string& old_rel,
                                   const std::string& new_rel);

}  // namespace dse::editor
