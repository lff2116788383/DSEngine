#include "editor_file_dialog.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <string>

namespace dse::editor {

namespace {

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

} // namespace

std::string OpenSceneFileDialog() {
    OPENFILENAMEW ofn = {};
    wchar_t file_buf[MAX_PATH] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Open Scene";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    if (GetOpenFileNameW(&ofn)) {
        return WideToUtf8(file_buf);
    }
    return {};
}

std::string SaveSceneFileDialog() {
    OPENFILENAMEW ofn = {};
    wchar_t file_buf[MAX_PATH] = {};
    wcscpy_s(file_buf, L"scene.json");

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Save Scene As";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    if (GetSaveFileNameW(&ofn)) {
        return WideToUtf8(file_buf);
    }
    return {};
}

} // namespace dse::editor

#else
// Non-Windows stub
namespace dse::editor {
std::string OpenSceneFileDialog() { return {}; }
std::string SaveSceneFileDialog() { return {}; }
} // namespace dse::editor
#endif
