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
    // 同步资产估算用量到统一预算视图（行为不变，仅上报）。
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
    // 将资产预算纳入统一视图，并上报当前估算用量（不改变 LRU 行为）。
    dse::core::Memory::SetBudget(dse::core::MemoryTag::Asset, budget_bytes);
    dse::core::Memory::ReportExternalUsage(dse::core::MemoryTag::Asset, estimated_memory_usage_);
}

std::size_t AssetManager::EstimatedMemoryUsage() const {
    // cache_mutex_ 不是 mutable，但此处仅读取原子级别可接受的估算值
    // 为保持 const 正确性，使用 const_cast（内部实现细节，不影响外部语义）
    auto& self = const_cast<AssetManager&>(*this);
    std::lock_guard<std::mutex> lock(self.cache_mutex_);
    return estimated_memory_usage_;
}

std::size_t AssetManager::EvictLRU() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (memory_budget_bytes_ == 0 || estimated_memory_usage_ <= memory_budget_bytes_) {
        return 0;
    }

    // 收集所有 LRU 条目并按 last_access 排序（最早的优先淘汰）
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

        // 尝试从各缓存表中驱逐（仅驱逐已无外部引用的条目）
        bool evicted_entry = false;
        auto tex_it = textures_.find(key);
        if (tex_it != textures_.end() && tex_it->second.use_count() <= 1) {
            const dse::render::TextureHandle handle =
                tex_it->second ? tex_it->second->GetHandle() : dse::render::TextureHandle{};
            // 仅当无任何存活 TextureRef 引用该句柄时才真正释放 GPU 显存；
            // 否则只从缓存表移除 CPU 壳，保留 GPU 纹理供仍持引用的消费者使用。
            if (dse::render::TextureRefRegistry::Instance().RefCount(handle.id) == 0) {
                DeleteGpuTextureLocked(handle);
                textures_.erase(tex_it);
                evicted_entry = true;
            }
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

    // 清理已驱逐条目的 LRU 记录
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
// 热重载：文件监听
// ============================================================

void AssetManager::StartFileWatcher() {
#if defined(__EMSCRIPTEN__)
    // Web has no pthreads and assets live in MEMFS (no host FS to watch);
    // skip the background hot-reload watcher thread.
    return;
#endif
    // 已在运行则忽略（幂等）。
    if (file_watcher_running_.load()) {
        return;
    }
    // 监听线程可能已自行退出（如上次 data root 打开失败）却尚未 join，此时
    // file_watcher_thread_ 仍 joinable。对 joinable 的 std::thread 做移动赋值会
    // 触发 std::terminate，故先停掉并 join 任何残留线程再启动新线程。
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

        // 每 200ms 检查一次是否需要退出
        while (file_watcher_running_.load()) {
            DWORD wait_result = WaitForSingleObject(overlapped.hEvent, 200);
            if (wait_result == WAIT_OBJECT_0) break;   // IO 完成
            if (wait_result == WAIT_TIMEOUT) continue;  // 超时，检查 running flag
            break; // 出错
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

    // inotify 非递归：需为根目录及全部子目录各注册一个 watch。
    // wd → 目录绝对路径，用于把事件名拼回完整路径并计算相对 data_root 的相对路径。
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
        const int pr = ::poll(&pfd, 1, 200);   // 200ms 超时，便于检查退出标志
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
                        // 新建/移入目录：递归补挂 watch，使其内文件后续改动可被捕获。
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

        // 纹理热重载
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto tex_it = textures_.find(cache_key);
            if (tex_it != textures_.end()) {
                const dse::render::TextureHandle old_handle =
                    tex_it->second ? tex_it->second->GetHandle() : dse::render::TextureHandle{};
                // 无任何 TextureRef 引用旧句柄时，重载前先释放旧 GPU 纹理，避免泄漏；
                // 仍被引用则保留旧句柄（消费者仍在使用它，删除会导致黑纹理）。
                if (dse::render::TextureRefRegistry::Instance().RefCount(old_handle.id) == 0) {
                    DeleteGpuTextureLocked(old_handle);
                }
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

        // Dmesh 热重载
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

        // Danim 热重载
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

        // Dskel 热重载
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

        // AudioClip 热重载
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
