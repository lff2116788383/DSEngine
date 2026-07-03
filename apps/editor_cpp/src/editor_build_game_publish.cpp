#include "editor_build_game_publish.h"

#include "imgui.h"
#include "editor_icons.h"
#include "editor_console_panel.h"

#include "zip.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>

#ifdef DSE_PUBLISH_ENABLED
#include <curl/curl.h>
#include <rapidjson/document.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace dse::editor {

namespace fs = std::filesystem;

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// ZipDirectory: compress a directory into a .zip using minizip (cross-platform)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::string ZipDirectory(const std::string& dir_path) {
    std::string zip_path = dir_path + ".zip";

    zipFile zf = zipOpen(zip_path.c_str(), APPEND_STATUS_CREATE);
    if (!zf) return "";

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(dir_path, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string rel_path = fs::relative(entry.path(), dir_path, ec).string();
        if (ec) continue;
        std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

        zip_fileinfo zi = {};
        if (zipOpenNewFileInZip(zf, rel_path.c_str(), &zi,
                                nullptr, 0, nullptr, 0, nullptr,
                                Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK) {
            continue;
        }

        std::ifstream ifs(entry.path(), std::ios::binary);
        char buf[8192];
        while (ifs.read(buf, sizeof(buf)) || ifs.gcount() > 0) {
            zipWriteInFileInZip(zf, buf, static_cast<unsigned int>(ifs.gcount()));
        }
        zipCloseFileInZip(zf);
    }

    zipClose(zf, nullptr);
    return fs::exists(zip_path, ec) ? zip_path : "";
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Upload (only compiled when DSE_PUBLISH_ENABLED is defined)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

#ifdef DSE_PUBLISH_ENABLED

struct UploadResult {
    bool success = false;
    std::string url;
    std::string error;
};

static int UploadProgressCb(void* userdata, curl_off_t /*dltotal*/,
                            curl_off_t /*dlnow*/, curl_off_t ultotal,
                            curl_off_t ulnow) {
    auto* progress = static_cast<std::atomic<float>*>(userdata);
    if (ultotal > 0) {
        *progress = static_cast<float>(ulnow) / static_cast<float>(ultotal);
    }
    return 0;
}

static UploadResult DoUpload(const std::string& zip_path,
                             const std::string& server_url,
                             const std::string& game_id,
                             const std::string& api_key,
                             std::atomic<float>& progress) {
    UploadResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "Failed to initialize HTTP client";
        return result;
    }

    curl_mime* mime = curl_mime_init(curl);

    // File part
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "package");
    curl_mime_filedata(part, zip_path.c_str());

    // Game ID part
    if (!game_id.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "game_id");
        curl_mime_data(part, game_id.c_str(), CURL_ZERO_TERMINATED);
    }

    // Response buffer
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // API Key header
    struct curl_slist* headers = nullptr;
    std::string auth_header = "X-API-Key: " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            auto* resp = static_cast<std::string*>(userdata);
            resp->append(ptr, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Progress callback
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, UploadProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // Timeouts
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error = std::string("Network error: ") + curl_easy_strerror(res);
        return result;
    }
    if (http_code != 200) {
        result.error = "Server returned HTTP " + std::to_string(http_code) + ": " + response;
        return result;
    }

    // Parse JSON response (rapidjson)
    rapidjson::Document doc;
    doc.Parse(response.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        result.error = "Failed to parse server response";
        return result;
    }
    if (doc.HasMember("error") && doc["error"].IsString()) {
        result.error = doc["error"].GetString();
    } else if (doc.HasMember("url") && doc["url"].IsString()) {
        result.success = true;
        result.url = doc["url"].GetString();
    } else {
        result.error = "Unexpected server response format";
    }

    return result;
}

#endif // DSE_PUBLISH_ENABLED

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Web Build + Publish workflow
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void StartWebBuildAndPublish(PublishState& state, const std::string& output_dir,
                             const std::string& game_title) {
    state.publish_done = false;
    state.publish_success = false;
    state.publish_error.clear();
    state.publish_url.clear();
    state.local_zip_path.clear();
    state.upload_progress = 0.0f;
    state.status_text = "Compressing Web build output...";

    state.build_future = std::async(std::launch::async, [&state, output_dir, game_title]() {
        // 1. Compress the Web build output directory
        std::string zip_path = ZipDirectory(output_dir);
        if (zip_path.empty()) {
            state.publish_error = "Failed to compress build output";
            state.publish_done = true;
            return;
        }

        // Verify zip contains index.html
        {
            std::error_code ec;
            bool has_index = fs::exists(fs::path(output_dir) / "index.html", ec);
            if (!has_index) {
                state.publish_error = "Build output missing index.html - Web build may have failed";
                state.publish_done = true;
                return;
            }
        }

#ifdef DSE_PUBLISH_ENABLED
        // 2. Online mode: upload to server
        if (state.enable_upload && state.upload_url[0] != '\0' && state.api_key[0] != '\0') {
            state.status_text = "Uploading to server...";

            auto result = DoUpload(zip_path, state.upload_url,
                                   state.game_id, state.api_key,
                                   state.upload_progress);

            // Clean up zip after upload
            std::error_code ec;
            fs::remove(zip_path, ec);

            if (result.success) {
                state.publish_url = result.url;
                state.publish_success = true;
                if (state.auto_copy_url) {
                    state.pending_clipboard = result.url;
                }
            } else {
                state.publish_error = result.error;
            }

            state.publish_done = true;
            return;
        }
#endif
        // 3. Offline mode: keep the zip locally
        state.local_zip_path = zip_path;
        state.publish_success = true;
        state.status_text = "Export complete!";
        state.publish_done = true;
    });
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// UI Section: Web Publish (drawn inside Build Game dialog)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static void OpenInExplorerPublish(const std::string& path) {
#if defined(_WIN32)
    fs::path p(path);
    if (p.has_parent_path()) {
        ShellExecuteA(nullptr, "explore", p.parent_path().string().c_str(),
                      nullptr, nullptr, SW_SHOWDEFAULT);
    }
#endif
}

void DrawWebPublishSection(PublishState& state, bool busy) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), MDI_ICON_CLOUD_UPLOAD " Web Export / Publish");

#ifdef DSE_PUBLISH_ENABLED
    ImGui::Checkbox("Upload to server after build", &state.enable_upload);

    if (state.enable_upload) {
        ImGui::Indent();
        ImGui::Text("Game ID:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##pub_game_id", state.game_id, sizeof(state.game_id));
        ImGui::SameLine();
        ImGui::TextDisabled("(empty = auto)");

        ImGui::Text("API Key:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##pub_api_key", state.api_key, sizeof(state.api_key),
                         ImGuiInputTextFlags_Password);

        ImGui::Text("Server: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##pub_url", state.upload_url, sizeof(state.upload_url));

        ImGui::Checkbox("Auto-copy link to clipboard", &state.auto_copy_url);
        ImGui::Unindent();
    }
#else
    ImGui::TextDisabled("Online publish disabled (compile with DSE_PUBLISH_ENABLED=ON)");
    ImGui::TextDisabled("Offline export is always available.");
#endif

    // Progress
    if (busy && !state.status_text.empty()) {
        if (state.upload_progress > 0.01f) {
            ImGui::ProgressBar(state.upload_progress, ImVec2(-1, 0), state.status_text.c_str());
        } else {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", state.status_text.c_str());
        }
    }

    // Handle clipboard on main thread (ImGui clipboard is not thread-safe)
    if (!state.pending_clipboard.empty()) {
        ImGui::SetClipboardText(state.pending_clipboard.c_str());
        state.pending_clipboard.clear();
    }

    // Results
    if (state.publish_done) {
        ImGui::Separator();
        if (state.publish_success) {
            if (!state.publish_url.empty()) {
                // Online mode success
                ImGui::TextColored(ImVec4(0, 1, 0, 1), MDI_ICON_CHECK " Published!");
                ImGui::Text("URL: %s", state.publish_url.c_str());
                if (ImGui::SmallButton("Copy Link")) {
                    ImGui::SetClipboardText(state.publish_url.c_str());
                }
            } else if (!state.local_zip_path.empty()) {
                // Offline mode success
                ImGui::TextColored(ImVec4(0, 1, 0, 1), MDI_ICON_CHECK " Exported!");
                ImGui::Text("File: %s", state.local_zip_path.c_str());
                if (ImGui::SmallButton("Open Folder")) {
                    OpenInExplorerPublish(state.local_zip_path);
                }
            }
        } else if (!state.publish_error.empty()) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), MDI_ICON_ALERT " Error: %s",
                              state.publish_error.c_str());
        }
    }
}

} // namespace dse::editor
