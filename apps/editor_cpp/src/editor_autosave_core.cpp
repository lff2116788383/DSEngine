#include "editor_autosave_core.h"

#include <algorithm>
#include <filesystem>

namespace dse::editor {

std::string SanitizeSceneName(std::string scene_name) {
    if (scene_name.empty()) return "Untitled";
    for (char& c : scene_name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
    return scene_name;
}

std::string MakeAutoSaveFileName(const std::string& scene_name) {
    return SanitizeSceneName(scene_name) + ".autosave.dscene";
}

bool IsAutoSaveRecoveryFile(const std::string& filename) {
    std::filesystem::path p(filename);
    return p.extension() == ".dscene" &&
           p.stem().string().find(".autosave") != std::string::npos;
}

double ClampAutoSaveInterval(double interval_sec) {
    return interval_sec < 10.0 ? 10.0 : interval_sec;
}

AutoSaveDecision DecideAutoSave(bool in_play_mode,
                               bool enabled,
                               bool dirty,
                               double last_save_time,
                               double now,
                               double interval_sec) {
    if (in_play_mode || !enabled || !dirty) return AutoSaveDecision::Skip;
    if (last_save_time == 0.0) return AutoSaveDecision::InitTimer;
    double interval = ClampAutoSaveInterval(interval_sec);
    if ((now - last_save_time) < interval) return AutoSaveDecision::Skip;
    return AutoSaveDecision::Save;
}

std::string MakeAtomicTempPath(const std::string& final_path) {
    return final_path + ".tmp";
}

bool AtomicWriteFile(const std::string& final_path,
                     const std::function<bool(const std::string&)>& write_to,
                     std::error_code& ec) {
    ec.clear();
    const std::string tmp = MakeAtomicTempPath(final_path);

    // 清掉上次中断留下的陈旧临时文件，避免误当作有效内容。
    std::error_code cleanup_ec;
    std::filesystem::remove(tmp, cleanup_ec);

    bool ok = false;
    try {
        ok = write_to ? write_to(tmp) : false;
    } catch (...) {
        ok = false;
    }
    if (!ok) {
        std::filesystem::remove(tmp, cleanup_ec);
        ec = std::make_error_code(std::errc::io_error);
        return false;
    }

    // rename 覆盖：同卷下为原子替换（MSVC 走 ReplaceIfExists，POSIX 走 rename(2)）。
    std::filesystem::rename(tmp, final_path, ec);
    if (ec) {
        std::filesystem::remove(tmp, cleanup_ec);
        return false;
    }
    return true;
}

bool AtomicWriteAll(const std::vector<AutoSaveItem>& items, std::error_code& ec) {
    ec.clear();

    std::vector<std::string> staged_tmps;
    staged_tmps.reserve(items.size());
    std::error_code cleanup_ec;

    auto remove_all_tmps = [&]() {
        for (const auto& t : staged_tmps) std::filesystem::remove(t, cleanup_ec);
    };

    // 阶段一：把每一项都写到各自的临时文件；任一失败即全部回滚（删临时文件），
    // 不改动任何最终文件。
    for (const auto& item : items) {
        const std::string tmp = MakeAtomicTempPath(item.final_path);
        std::filesystem::remove(tmp, cleanup_ec);  // 清陈旧临时文件

        bool ok = false;
        try {
            ok = item.write_to ? item.write_to(tmp) : false;
        } catch (...) {
            ok = false;
        }
        if (!ok) {
            std::filesystem::remove(tmp, cleanup_ec);
            remove_all_tmps();
            ec = std::make_error_code(std::errc::io_error);
            return false;
        }
        staged_tmps.push_back(tmp);
    }

    // 阶段二：所有内容已完整落盘，逐个 rename 提交。
    for (size_t i = 0; i < items.size(); ++i) {
        std::filesystem::rename(staged_tmps[i], items[i].final_path, ec);
        if (ec) {
            // 提交阶段失败：删除尚未提交的临时文件（已提交的最终文件是完整内容，
            // 保留）。返回失败让调用方在下个间隔重试。
            for (size_t j = i; j < staged_tmps.size(); ++j)
                std::filesystem::remove(staged_tmps[j], cleanup_ec);
            return false;
        }
    }
    return true;
}

std::vector<std::string> CollectRecoveryFiles(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) return out;

    for (std::filesystem::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        const std::string path = it->path().string();
        if (IsAutoSaveRecoveryFile(path)) out.push_back(path);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace dse::editor
