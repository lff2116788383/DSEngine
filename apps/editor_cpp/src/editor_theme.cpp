#include "editor_theme.h"

#include <iostream>

#include "editor_icons.h"

namespace dse::editor {

namespace {
EditorFonts g_editor_fonts;
} // namespace

bool LoadEditorFonts(const std::filesystem::path& fonts_dir) {
    ImGuiIO& io = ImGui::GetIO();

    const std::filesystem::path inter_regular = fonts_dir / "Inter-Regular.ttf";
    const std::filesystem::path inter_bold    = fonts_dir / "Inter-Bold.ttf";
    const std::filesystem::path noto_sc       = fonts_dir / "NotoSansSC-Regular.ttf";
    const std::filesystem::path fa_solid      = fonts_dir / "fa-solid-900.ttf";

    const float base_size = 16.0f;

    // ---- Regular font (Inter) ----
    if (std::filesystem::exists(inter_regular)) {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        cfg.PixelSnapH  = true;
        g_editor_fonts.regular = io.Fonts->AddFontFromFileTTF(
            inter_regular.string().c_str(), base_size, &cfg);
    } else {
        std::cerr << "[EditorTheme] Inter-Regular.ttf not found, using default font." << std::endl;
        g_editor_fonts.regular = io.Fonts->AddFontDefault();
    }

    // Merge Noto Sans SC for CJK glyphs
    if (std::filesystem::exists(noto_sc)) {
        ImFontConfig merge_cfg;
        merge_cfg.MergeMode  = true;
        merge_cfg.OversampleH = 1;
        merge_cfg.OversampleV = 1;
        merge_cfg.PixelSnapH  = true;
        static const ImWchar cjk_ranges[] = {
            0x2000, 0x206F,  // General Punctuation
            0x3000, 0x30FF,  // CJK Symbols, Hiragana, Katakana
            0x4E00, 0x9FFF,  // CJK Unified Ideographs
            0xFF00, 0xFFEF,  // Halfwidth and Fullwidth Forms
            0,
        };
        io.Fonts->AddFontFromFileTTF(
            noto_sc.string().c_str(), base_size, &merge_cfg, cjk_ranges);
    }

    // Merge Font Awesome 6 Solid icons (16-bit PUA range)
    if (std::filesystem::exists(fa_solid)) {
        ImFontConfig icon_cfg;
        icon_cfg.MergeMode   = true;
        icon_cfg.OversampleH = 1;
        icon_cfg.OversampleV = 1;
        icon_cfg.PixelSnapH  = true;
        icon_cfg.GlyphMinAdvanceX = base_size;
        static const ImWchar icon_ranges[] = {
            0xF000, 0xF8FF,  // Font Awesome PUA range (16-bit)
            0,
        };
        io.Fonts->AddFontFromFileTTF(
            fa_solid.string().c_str(), base_size, &icon_cfg, icon_ranges);
    }

    // ---- Bold font ----
    if (std::filesystem::exists(inter_bold)) {
        ImFontConfig bold_cfg;
        bold_cfg.OversampleH = 2;
        bold_cfg.OversampleV = 1;
        bold_cfg.PixelSnapH  = true;
        g_editor_fonts.bold = io.Fonts->AddFontFromFileTTF(
            inter_bold.string().c_str(), base_size, &bold_cfg);
    } else {
        g_editor_fonts.bold = g_editor_fonts.regular;
    }

    // NOTE: Do NOT call io.Fonts->Build() here.
    // ImGui 1.92+ backends with ImGuiBackendFlags_RendererHasTextures
    // handle font atlas building automatically during NewFrame().
    return g_editor_fonts.regular != nullptr;
}

void SetupEditorStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Hazel-inspired deep dark theme with soft blue accent
    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.45f, 0.45f, 0.48f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.33f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.43f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.53f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.36f, 0.62f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.56f, 1.00f, 0.75f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.56f, 1.00f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.28f, 0.56f, 1.00f, 0.70f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.28f, 0.56f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.28f, 0.56f, 1.00f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.28f, 0.56f, 1.00f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.28f, 0.56f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.28f, 0.56f, 1.00f, 0.75f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.17f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.28f, 0.56f, 1.00f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // Rounding & spacing
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 10.0f;

    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;
}

const EditorFonts& GetEditorFonts() {
    return g_editor_fonts;
}

void PushBoldFont() {
    if (g_editor_fonts.bold) {
        ImGui::PushFont(g_editor_fonts.bold);
    }
}

} // namespace dse::editor
