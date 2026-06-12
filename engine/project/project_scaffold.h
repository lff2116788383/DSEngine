/**
 * @file project_scaffold.h
 * @brief 脱离编辑器的项目模板生成（纯 std）。编辑器 New Project 与 dse CLI `new` 共用同一份逻辑。
 */

#ifndef DSE_PROJECT_SCAFFOLD_H
#define DSE_PROJECT_SCAFFOLD_H

#include <string>
#include "engine/core/dse_export.h"

namespace dse::project {

/// 项目模板类型（与编辑器 dse::editor::ProjectTemplate 一一对应）。
enum class ProjectTemplate {
    Empty,
    Game2D,
    Game3D,
    Lua,
};

struct ScaffoldResult {
    bool ok = false;
    std::string error;
};

/**
 * @brief 在 project_root 下生成完整项目骨架（目录会按需创建）。
 *
 * 产物：
 *   - project.dseproj（项目描述，JSON）
 *   - scenes/main.json（按模板预置实体或空场景数组）
 *   - scripts/main.lua（2D/3D/Lua 模板提供入口脚本）
 *   - assets/{textures,models,audio,font}/
 *   - .gitignore
 *
 * @param project_root    项目根目录
 * @param name            项目名
 * @param tmpl            模板类型
 * @param engine_version  引擎版本号（写入 project.dseproj；可留空）
 * @return ok=false 时 error 含失败原因
 */
DSE_EXPORT ScaffoldResult ScaffoldProject(const std::string& project_root,
                                          const std::string& name,
                                          ProjectTemplate tmpl,
                                          const std::string& engine_version = "");

/// 将 CLI 模板 token（empty / 2d / 3d / lua）解析为枚举，未知返回 false。
DSE_EXPORT bool ParseTemplateToken(const std::string& token, ProjectTemplate& out);

/// 模板可读名称（用于日志/提示）。
DSE_EXPORT const char* TemplateDisplayName(ProjectTemplate tmpl);

} // namespace dse::project

#endif // DSE_PROJECT_SCAFFOLD_H
