#pragma once

#include <filesystem>
#include "editor_context.h"
#include "editor_shared_components.h"

struct UILabelComponent;

namespace dse::editor {

void DrawProjectPanel();

// Project 面板导航状态访问器（供 UI 测试断言/复位浏览目录；普通运行不需要）。
const std::filesystem::path& ProjectPanelCurrentPath();
void SetProjectPanelCurrentPath(const std::filesystem::path& path);
std::filesystem::path ProjectPanelBaseDataPath();
void DrawConsolePanel();
void DrawLocalizationPreviewPanel(EditorContext& ctx,
                                  char* key_buf, std::size_t key_size,
                                  char* fallback_buf, std::size_t fallback_size);
void DrawAnimationPanel(EditorContext& ctx);
void DrawTilePalettePanel(EditorContext& ctx);

} // namespace dse::editor
