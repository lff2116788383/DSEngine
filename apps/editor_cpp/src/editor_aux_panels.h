#pragma once

#include <filesystem>
#include "editor_context.h"
#include "editor_shared_components.h"

struct UILabelComponent;

namespace dse::editor {

void DrawProjectPanel();
void DrawConsolePanel();
void DrawLocalizationPreviewPanel(EditorContext& ctx,
                                  char* key_buf, std::size_t key_size,
                                  char* fallback_buf, std::size_t fallback_size);
void DrawAnimationPanel(entt::registry& registry, entt::entity selected_entity);
void DrawTilePalettePanel(EditorContext& ctx);

} // namespace dse::editor
