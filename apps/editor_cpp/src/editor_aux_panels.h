#pragma once

#include <filesystem>
#include <entt/entt.hpp>

#include "editor_shared_components.h"

struct UILabelComponent;

namespace dse::editor {

struct EditorAuxPanelsContext {
    entt::registry& registry;
    entt::entity selected_entity;
    bool is_2d = false;
    char* localization_preview_key = nullptr;
    std::size_t localization_preview_key_size = 0;
    char* localization_preview_fallback = nullptr;
    std::size_t localization_preview_fallback_size = 0;
};

void DrawProjectPanel();
void DrawConsolePanel();
void DrawLocalizationPreviewPanel(EditorAuxPanelsContext& context);
void DrawAnimationPanel();
void DrawTilePalettePanel(const EditorAuxPanelsContext& context);

} // namespace dse::editor
