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

std::string OpenFileDialog(const char* title, const char* filter, const char* def_ext,
                           const char* initial_dir) {
    OPENFILENAMEW ofn = {};
    wchar_t file_buf[MAX_PATH] = {};
    std::wstring wdir; // 必须存活到 GetOpenFileNameW 调用结束
    if (initial_dir && *initial_dir) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, initial_dir, -1, nullptr, 0);
        if (wlen > 0) {
            wdir.assign(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, initial_dir, -1, wdir.data(), wlen);
            ofn.lpstrInitialDir = wdir.c_str();
        }
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    std::wstring wfilter;
    if (filter && *filter) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filter, -1, nullptr, 0);
        if (wlen > 0) {
            wfilter.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, filter, -1, wfilter.data(), wlen);
        }
    }
    std::wstring wtitle;
    if (title && *title) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        if (wlen > 0) {
            wtitle.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle.data(), wlen);
        }
    }
    std::wstring wdefext;
    if (def_ext && *def_ext) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, def_ext, -1, nullptr, 0);
        if (wlen > 0) {
            wdefext.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, def_ext, -1, wdefext.data(), wlen);
        }
    }
    ofn.lpstrFilter = wfilter.empty() ? nullptr : wfilter.c_str();
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = wdefext.empty() ? nullptr : wdefext.c_str();

    if (GetOpenFileNameW(&ofn)) {
        return WideToUtf8(file_buf);
    }
    return {};
}

std::string SaveFileDialog(const char* title, const char* filter, const char* def_ext,
                           const char* default_name, const char* initial_dir) {
    OPENFILENAMEW ofn = {};
    wchar_t file_buf[MAX_PATH] = {};
    if (default_name && *default_name) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, default_name, -1, nullptr, 0);
        if (wlen > 0 && wlen <= MAX_PATH) {
            MultiByteToWideChar(CP_UTF8, 0, default_name, -1, file_buf, wlen);
        }
    }
    std::wstring wdir; // 必须存活到 GetSaveFileNameW 调用结束
    if (initial_dir && *initial_dir) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, initial_dir, -1, nullptr, 0);
        if (wlen > 0) {
            wdir.assign(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, initial_dir, -1, wdir.data(), wlen);
            ofn.lpstrInitialDir = wdir.c_str();
        }
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    std::wstring wfilter;
    if (filter && *filter) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filter, -1, nullptr, 0);
        if (wlen > 0) {
            wfilter.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, filter, -1, wfilter.data(), wlen);
        }
    }
    std::wstring wtitle;
    if (title && *title) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        if (wlen > 0) {
            wtitle.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle.data(), wlen);
        }
    }
    std::wstring wdefext;
    if (def_ext && *def_ext) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, def_ext, -1, nullptr, 0);
        if (wlen > 0) {
            wdefext.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, def_ext, -1, wdefext.data(), wlen);
        }
    }
    ofn.lpstrFilter = wfilter.empty() ? nullptr : wfilter.c_str();
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = wdefext.empty() ? nullptr : wdefext.c_str();

    if (GetSaveFileNameW(&ofn)) {
        return WideToUtf8(file_buf);
    }
    return {};
}

std::string OpenSceneFileDialog() {
    return OpenFileDialog("Open Scene", "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0", "json");
}

std::string SaveSceneFileDialog() {
    return SaveFileDialog("Save Scene As", "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0",
                          "json", "scene.json");
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
std::string OpenFileDialog(const char*, const char*, const char*, const char*) { return {}; }
std::string SaveFileDialog(const char*, const char*, const char*, const char*, const char*) { return {}; }
std::string BrowseFolderDialog(const char*) { return {}; }
} // namespace dse::editor
#endif
