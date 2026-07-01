/**
 * @file asset_manager_hotreload.cpp
 * @brief AssetManager LRU cache, hot reload, and file watcher.
 */

#include "engine/assets/asset_manager.h"
#include "engine/assets/dds_parser.h"
#include "engine/assets/dtex.h"
#include "engine/assets/bundle_packer.h"
#include "engine/assets/pak_reader.h"
#include "engine/assets/native_file_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/core/job_system.h"
#include "engine/core/event_bus.h"
#include "engine/core/memory/memory.h"
#include <utility>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <rapidjson/document.h>
#include "bundle/bundle.h"
#include <stb/stb_image.h>

namespace {
std::string NormalizePath(const std::string& path) {
    std::filesystem::path p(path);
    return p.make_preferred().lexically_normal().string();
}
} // anonymous namespace

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#endif
void AssetManager::TouchLru(const std::string& cache_key, std::size_t estimated_bytes) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = lru_entries_.find(cache_key);
    if (it != lru_entries_.end()) {
        it->second.last_access = std::chrono::steady_clock::now();
        return;
    }
    LruEntry entry;
    entry.cache_key = cache_key;
    entry.estimated_bytes = estimated_bytes;
    entry.last_access = std::chrono::steady_clock::now();
    lru_entries_[cache_key] = entry;
    estimated_memory_usage_ += estimated_bytes;
    // åŒæ­¥èµ„äº§ä¼°ç®—ç”¨é‡åˆ°ç»Ÿä¸€é¢„ç®—è§†å›¾ï¼ˆè¡Œä¸ºä¸å˜ï¼Œä»…ä¸ŠæŠ¥ï¼‰ã€‚
    dse::core::Memory::ReportExternalUsage(dse::core::MemoryTag::Asset, estimated_memory_usage_);
}

void AssetManager::RemoveLru(const std::string& cache_key) {
    auto it = lru_entries_.find(cache_key);
    if (it != lru_entries_.end()) {
        if (estimated_memory_usage_ >= it->second.estimated_bytes) {
            estimated_memory_usage_ -= it->second.estimated_bytes;
        } else {
            estimated_memory_usage_ = 0;
        }
        lru_entries_.erase(it);
        dse::core::Memory::ReportExternalUsage(dse::core::MemoryTag::Asset, estimated_memory_usage_);
    }
}

void AssetManager::SetMemoryBudget(std::size_t budget_bytes) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    memory_budget_bytes_ = budget_bytes;
    // å°†èµ„äº§é¢„ç®—çº³å…¥ç»Ÿä¸€è§†å›¾ï¼Œå¹¶ä¸ŠæŠ¥å½“å‰ä¼°ç®—ç”¨é‡ï¼ˆä¸æ”¹å˜ LRU è¡Œä¸ºï¼‰ã€‚
    dse::core::Memory::SetBudget(dse::core::MemoryTag::Asset, budget_bytes);
    dse::core::Memory::ReportExternalUsage(dse::core::MemoryTag::Asset, estimated_memory_usage_);
}

std::size_t AssetManager::EstimatedMemoryUsage() const {
    // cache_mutex_ ä¸æ˜¯ mutableï¼Œä½†æ­¤å¤„ä»…è¯»å–åŽŸå­çº§åˆ«å¯æŽ¥å—çš„ä¼°ç®—å€¼
    // ä¸ºä¿æŒ const æ­£ç¡®æ€§ï¼Œä½¿ç”¨ const_castï¼ˆå†…éƒ¨å®žçŽ°ç»†èŠ‚ï¼Œä¸å½±å“å¤–éƒ¨è¯­ä¹‰ï¼‰
    auto& self = const_cast<AssetManager&>(*this);
    std::lock_guard<std::mutex> lock(self.cache_mutex_);
    return estimated_memory_usage_;
}

std::size_t AssetManager::EvictLRU() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (memory_budget_bytes_ == 0 || estimated_memory_usage_ <= memory_budget_bytes_) {
        return 0;
    }

    // æ”¶é›†æ‰€æœ‰ LRU æ¡ç›®å¹¶æŒ‰ last_access æŽ’åºï¼ˆæœ€æ—©çš„ä¼˜å…ˆæ·˜æ±°ï¼‰
    std::vector<LruEntry*> entries;
    entries.reserve(lru_entries_.size());
    for (auto& pair : lru_entries_) {
        entries.push_back(&pair.second);
    }
    std::sort(entries.begin(), entries.end(), [](const LruEntry* a, const LruEntry* b) {
        return a->last_access < b->last_access;
    });

    std::size_t evicted = 0;
    for (auto* entry : entries) {
        if (estimated_memory_usage_ <= memory_budget_bytes_) {
            break;
        }
        const std::string& key = entry->cache_key;

        // å°è¯•ä»Žå„ç¼“å­˜è¡¨ä¸­é©±é€ï¼ˆä»…é©±é€å·²æ— å¤–éƒ¨å¼•ç”¨çš„æ¡ç›®ï¼‰
        bool evicted_entry = false;
        auto tex_it = textures_.find(key);
        if (tex_it != textures_.end() && tex_it->second.use_count() <= 1) {
            textures_.erase(tex_it);
            evicted_entry = true;
        }
        auto cubemap_it = cubemaps_.find(key);
        if (cubemap_it != cubemaps_.end() && cubemap_it->second.expired()) {
            cubemaps_.erase(cubemap_it);
            evicted_entry = true;
        }
        auto dmesh_it = dmeshes_.find(key);
        if (dmesh_it != dmeshes_.end() && dmesh_it->second.expired()) {
            dmeshes_.erase(dmesh_it);
            evicted_entry = true;
        }
        auto danim_it = danims_.find(key);
        if (danim_it != danims_.end() && danim_it->second.expired()) {
            danims_.erase(danim_it);
            evicted_entry = true;
        }
        auto dskel_it = dskels_.find(key);
        if (dskel_it != dskels_.end() && dskel_it->second.expired()) {
            dskels_.erase(dskel_it);
            evicted_entry = true;
        }
        auto audio_it = audio_clips_.find(key);
        if (audio_it != audio_clips_.end() && audio_it->second.expired()) {
            audio_clips_.erase(audio_it);
            evicted_entry = true;
        }

        if (evicted_entry) {
            if (estimated_memory_usage_ >= entry->estimated_bytes) {
                estimated_memory_usage_ -= entry->estimated_bytes;
            } else {
                estimated_memory_usage_ = 0;
            }
            ++evicted;
            // RemoveLru inline â€” will erase from map after loop
        }
    }

    // æ¸…ç†å·²é©±é€æ¡ç›®çš„ LRU è®°å½•
    for (auto it = lru_entries_.begin(); it != lru_entries_.end(); ) {
        const std::string& key = it->first;
        bool still_alive = false;
        if (textures_.count(key)) still_alive = true;
        if (cubemaps_.count(key)) still_alive = true;
        if (dmeshes_.count(key)) still_alive = true;
        if (danims_.count(key)) still_alive = true;
        if (dskels_.count(key)) still_alive = true;
        if (audio_clips_.count(key)) still_alive = true;
        if (!still_alive) {
            it = lru_entries_.erase(it);
        } else {
            ++it;
        }
    }

    dse::core::Memory::ReportExternalUsage(dse::core::MemoryTag::Asset, estimated_memory_usage_);
    return evicted;
}

// ============================================================
// çƒ­é‡è½½ï¼šæ–‡ä»¶ç›‘å¬
// ============================================================

void AssetManager::StartFileWatcher() {
#if defined(__EMSCRIPTEN__)
    // Web has no pthreads and assets live in MEMFS (no host FS to watch);
    // skip the background hot-reload watcher thread.
    return;
#endif
    // å·²åœ¨è¿è¡Œåˆ™å¿½ç•¥ï¼ˆå¹‚ç­‰ï¼‰ã€‚
    if (file_watcher_running_.load()) {
        return;
    }
    // ç›‘å¬çº¿ç¨‹å¯èƒ½å·²è‡ªè¡Œé€€å‡ºï¼ˆå¦‚ä¸Šæ¬¡ data root æ‰“å¼€å¤±è´¥ï¼‰å´å°šæœª joinï¼Œæ­¤æ—¶
    // file_watcher_thread_ ä» joinableã€‚å¯¹ joinable çš„ std::thread åšç§»åŠ¨èµ‹å€¼ä¼š
    // è§¦å‘ std::terminateï¼Œæ•…å…ˆåœæŽ‰å¹¶ join ä»»ä½•æ®‹ç•™çº¿ç¨‹å†å¯åŠ¨æ–°çº¿ç¨‹ã€‚
    if (file_watcher_thread_.joinable()) {
        file_watcher_thread_.join();
    }
    file_watcher_running_.store(true);
    file_watcher_thread_ = std::thread(&AssetManager::FileWatcherLoop, this);
}

void AssetManager::StopFileWatcher() {
    file_watcher_running_.store(false);
    if (file_watcher_thread_.joinable()) {
        file_watcher_thread_.join();
    }
}

void AssetManager::FileWatcherLoop() {
#ifdef _WIN32
    const std::string data_root = GetDataRoot();
    if (data_root.empty()) {
        DEBUG_LOG_WARN("FileWatcher: data root is empty, watcher exiting");
        file_watcher_running_.store(false);
        return;
    }

    std::wstring wide_path;
    {
        std::filesystem::path fs_path(data_root);
        wide_path = fs_path.wstring();
    }

    HANDLE dir_handle = CreateFileW(
        wide_path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (dir_handle == INVALID_HANDLE_VALUE) {
        DEBUG_LOG_ERROR("FileWatcher: failed to open directory handle for {}", data_root);
        file_watcher_running_.store(false);
        return;
    }

    DEBUG_LOG_INFO("FileWatcher: started monitoring {}", data_root);

    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        DEBUG_LOG_ERROR("FileWatcher: failed to create event object");
        CloseHandle(dir_handle);
        file_watcher_running_.store(false);
        return;
    }

    alignas(DWORD) char buffer[4096];
    while (file_watcher_running_.load()) {
        ResetEvent(overlapped.hEvent);
        BOOL result = ReadDirectoryChangesW(
            dir_handle,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            nullptr,
            &overlapped,
            nullptr);

        if (!result && GetLastError() != ERROR_IO_PENDING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // æ¯ 200ms æ£€æŸ¥ä¸€æ¬¡æ˜¯å¦éœ€è¦é€€å‡º
        while (file_watcher_running_.load()) {
            DWORD wait_result = WaitForSingleObject(overlapped.hEvent, 200);
            if (wait_result == WAIT_OBJECT_0) break;   // IO å®Œæˆ
            if (wait_result == WAIT_TIMEOUT) continue;  // è¶…æ—¶ï¼Œæ£€æŸ¥ running flag
            break; // å‡ºé”™
        }

        if (!file_watcher_running_.load()) {
            CancelIo(dir_handle);
            break;
        }

        DWORD bytes_returned = 0;
        if (!GetOverlappedResult(dir_handle, &overlapped, &bytes_returned, FALSE) || bytes_returned == 0) {
            continue;
        }

        DWORD offset = 0;
        do {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer + offset);
            if (info->Action == FILE_ACTION_MODIFIED || info->Action == FILE_ACTION_ADDED) {
                std::wstring wname(info->FileName, info->FileNameLength / sizeof(WCHAR));
                std::string relative = std::filesystem::path(wname).generic_string();

                {
                    std::lock_guard<std::mutex> lock(hot_reload_mutex_);
                    if (std::find(pending_hot_reloads_.begin(), pending_hot_reloads_.end(), relative) == pending_hot_reloads_.end()) {
                        pending_hot_reloads_.push_back(relative);
                        DEBUG_LOG_INFO("FileWatcher: queued hot-reload for {}", relative);
                    }
                }
            }
            if (info->NextEntryOffset == 0) break;
            offset += info->NextEntryOffset;
        } while (offset < bytes_returned);
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(dir_handle);
#elif defined(__linux__) && !defined(__ANDROID__)
    const std::string data_root = GetDataRoot();
    if (data_root.empty()) {
        DEBUG_LOG_WARN("FileWatcher: data root is empty, watcher exiting");
        file_watcher_running_.store(false);
        return;
    }

    const int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd < 0) {
        DEBUG_LOG_ERROR("FileWatcher: inotify_init1 failed for {}", data_root);
        file_watcher_running_.store(false);
        return;
    }

    // inotify éžé€’å½’ï¼šéœ€ä¸ºæ ¹ç›®å½•åŠå…¨éƒ¨å­ç›®å½•å„æ³¨å†Œä¸€ä¸ª watchã€‚
    // wd â†’ ç›®å½•ç»å¯¹è·¯å¾„ï¼Œç”¨äºŽæŠŠäº‹ä»¶åæ‹¼å›žå®Œæ•´è·¯å¾„å¹¶è®¡ç®—ç›¸å¯¹ data_root çš„ç›¸å¯¹è·¯å¾„ã€‚
    std::unordered_map<int, std::filesystem::path> wd_to_dir;
    const uint32_t watch_mask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE;

    auto add_watch = [&](const std::filesystem::path& dir) {
        const int wd = inotify_add_watch(inotify_fd, dir.c_str(), watch_mask);
        if (wd >= 0) wd_to_dir[wd] = dir;
    };
    auto add_tree = [&](const std::filesystem::path& root) {
        add_watch(root);
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(
            root, std::filesystem::directory_options::skip_permission_denied, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            if (it->is_directory(ec)) add_watch(it->path());
        }
    };

    const std::filesystem::path root_path(data_root);
    add_tree(root_path);
    DEBUG_LOG_INFO("FileWatcher: started monitoring {}", data_root);

    std::vector<char> buffer(64 * 1024);
    while (file_watcher_running_.load()) {
        pollfd pfd{};
        pfd.fd = inotify_fd;
        pfd.events = POLLIN;
        const int pr = ::poll(&pfd, 1, 200);   // 200ms è¶…æ—¶ï¼Œä¾¿äºŽæ£€æŸ¥é€€å‡ºæ ‡å¿—
        if (pr <= 0 || !(pfd.revents & POLLIN)) continue;

        const ssize_t len = ::read(inotify_fd, buffer.data(), buffer.size());
        if (len <= 0) continue;

        ssize_t offset = 0;
        while (offset + static_cast<ssize_t>(sizeof(inotify_event)) <= len) {
            auto* ev = reinterpret_cast<inotify_event*>(buffer.data() + offset);
            if (ev->len > 0) {
                auto dir_it = wd_to_dir.find(ev->wd);
                if (dir_it != wd_to_dir.end()) {
                    const std::filesystem::path full = dir_it->second / std::string(ev->name);
                    if ((ev->mask & IN_ISDIR) && (ev->mask & (IN_CREATE | IN_MOVED_TO))) {
                        // æ–°å»º/ç§»å…¥ç›®å½•ï¼šé€’å½’è¡¥æŒ‚ watchï¼Œä½¿å…¶å†…æ–‡ä»¶åŽç»­æ”¹åŠ¨å¯è¢«æ•èŽ·ã€‚
                        add_tree(full);
                    } else if (ev->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                        std::error_code ec;
                        const std::filesystem::path rel =
                            std::filesystem::relative(full, root_path, ec);
                        const std::string relative =
                            (ec ? full : rel).generic_string();
                        std::lock_guard<std::mutex> lock(hot_reload_mutex_);
                        if (std::find(pending_hot_reloads_.begin(),
                                      pending_hot_reloads_.end(),
                                      relative) == pending_hot_reloads_.end()) {
                            pending_hot_reloads_.push_back(relative);
                            DEBUG_LOG_INFO("FileWatcher: queued hot-reload for {}", relative);
                        }
                    }
                }
            }
            offset += static_cast<ssize_t>(sizeof(inotify_event) + ev->len);
        }
    }

    for (const auto& [wd, dir] : wd_to_dir) inotify_rm_watch(inotify_fd, wd);
    ::close(inotify_fd);
#else
    DEBUG_LOG_WARN("FileWatcher: not implemented on this platform");
    while (file_watcher_running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
#endif
    DEBUG_LOG_INFO("FileWatcher: stopped");
}

std::size_t AssetManager::PumpHotReloads() {
    std::vector<std::string> reloads;
    {
        std::lock_guard<std::mutex> lock(hot_reload_mutex_);
        reloads.swap(pending_hot_reloads_);
    }

    if (reloads.empty()) {
        return 0;
    }

    std::size_t reloaded = 0;
    for (const auto& relative_path : reloads) {
        const std::string logical = NormalizeAssetPath(relative_path);
        const std::string cache_key = logical.empty() ? NormalizePath(relative_path) : logical;

        bool did_reload = false;

        // çº¹ç†çƒ­é‡è½½
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto tex_it = textures_.find(cache_key);
            if (tex_it != textures_.end()) {
                textures_.erase(tex_it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_tex = LoadTexture(relative_path);
            if (reloaded_tex) {
                DEBUG_LOG_INFO("HotReload: reloaded texture {}", relative_path);
                ++reloaded;
            }
            dse::core::EventBus* bus = GetEventBus();
            if (bus) {
                bus->Publish<dse::core::ResourceLoadedEvent>(cache_key, reloaded_tex != nullptr);
            }
            continue;
        }

        // Dmesh çƒ­é‡è½½
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = dmeshes_.find(cache_key);
            if (it != dmeshes_.end()) {
                dmeshes_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadDmesh(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded dmesh {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        // Danim çƒ­é‡è½½
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = danims_.find(cache_key);
            if (it != danims_.end()) {
                danims_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadDanim(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded danim {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        // Dskel çƒ­é‡è½½
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = dskels_.find(cache_key);
            if (it != dskels_.end()) {
                dskels_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadDskel(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded dskel {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        // AudioClip çƒ­é‡è½½
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = audio_clips_.find(cache_key);
            if (it != audio_clips_.end()) {
                audio_clips_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadAudioClip(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded audio clip {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        DEBUG_LOG_INFO("HotReload: no cached asset matched for {}", relative_path);
    }

    return reloaded;
}
