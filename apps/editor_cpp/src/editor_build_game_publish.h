#pragma once

#include <string>
#include <atomic>
#include <future>

namespace dse::editor {

/// State for the Web publish workflow (offline export / online upload).
struct PublishState {
    // --- Configuration (persisted in editor prefs) ---
    bool enable_upload = false;             // true = upload after build; false = local zip only
    char game_id[64] = "";                  // custom game ID (empty = auto-generate)
    char upload_url[256] = "https://api.dse.run/api/publish";
    char api_key[128] = "";                 // server API Key
    bool auto_copy_url = true;              // copy link to clipboard on success

    // --- Results ---
    std::string publish_url;                // URL returned by server (online mode)
    std::string local_zip_path;             // path to exported zip (offline mode)
    bool publish_done = false;
    bool publish_success = false;
    std::string publish_error;

    // --- Progress ---
    std::atomic<float> upload_progress{0.0f};
    std::string status_text;

    // --- Lifecycle ---
    std::future<void> build_future;         // async task handle (safe lifetime)
    std::string pending_clipboard;          // text to copy on main thread
};

/// Compress a directory into a .zip file using minizip (cross-platform).
/// Returns the path to the created zip, or empty string on failure.
std::string ZipDirectory(const std::string& dir_path);

/// Draw the Web publish section inside the Build Game dialog.
/// Call after platform == Web is selected.
void DrawWebPublishSection(PublishState& state, bool busy);

/// Start the Web build + optional publish workflow.
void StartWebBuildAndPublish(PublishState& state, const std::string& output_dir,
                             const std::string& game_title);

} // namespace dse::editor
