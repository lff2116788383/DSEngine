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

std::string BrowseFolderDialog(const char* title) {
    std::string result;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD options;
        pfd->GetOptions(&options);
        pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_NOCHANGEDIR);
        // Convert title to wide
        int wlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        std::wstring wtitle(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle.data(), wlen);
        pfd->SetTitle(wtitle.c_str());
        hr = pfd->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = WideToUtf8(path);
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    CoUninitialize();
    return result;
}

} // namespace dse::editor

#else
// Non-Windows stub
namespace dse::editor {
std::string OpenSceneFileDialog() { return {}; }
std::string SaveSceneFileDialog() { return {}; }
std::string BrowseFolderDialog(const char*) { return {}; }
} // namespace dse::editor
#endif
