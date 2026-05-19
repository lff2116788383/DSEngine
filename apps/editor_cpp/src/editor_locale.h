#pragma once
#include <string>

namespace dse::editor {

/// Returns translated string for the given English key.
/// Falls back to the key itself if no translation is found.
const char* T(const char* en_key);

/// Set editor UI locale. Call once at startup before the first frame.
/// Supported values: "en" (default), "zh-CN"
void SetEditorLocale(const std::string& locale);

/// Get current editor UI locale string.
const std::string& GetEditorLocale();

} // namespace dse::editor
