/**
 * @file editor_core_isolation_test.cpp
 * @brief CI 门禁：apps/editor_cpp/core/ 不得出现任何 UI 框架引用
 *
 * "逻辑与 UI 隔离"的可执行守卫。core/ 是编辑器的命令/查询门面层，必须对任何 UI 框架
 * （ImGui / ImGuizmo）零知识。一旦有人把 UI 类型/调用泄漏进 core/，本测试立即失败，
 * 从而把隔离边界长期钉死，防止回归。
 *
 * 与 scripts/check_core_ui_free.py 同源（脚本供 CI workflow 调用，本测试供 ctest）。
 * core 目录路径在 CMake 配置期通过 DSE_EDITOR_CORE_DIR 注入。
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#ifndef DSE_EDITOR_CORE_DIR
#error "DSE_EDITOR_CORE_DIR must be defined by CMake"
#endif

namespace {

// 禁用 token（大小写敏感即可命中实际用法）。"ImVec"/"ImGuizmo" 覆盖宏与类型，
// "imgui" 覆盖 include 与标识符（小写）。
const std::vector<std::string>& ForbiddenTokens() {
    static const std::vector<std::string> kTokens = {
        "imgui", "ImGui", "ImVec", "ImGuizmo", "imgui_impl",
    };
    return kTokens;
}

bool IsScannableSource(const fs::path& p) {
    const auto ext = p.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".cpp" ||
           ext == ".cc" || ext == ".inl";
}

} // namespace

TEST(EditorCoreIsolation, CoreDirHasNoUiFrameworkReferences) {
    const fs::path core_dir{DSE_EDITOR_CORE_DIR};
    ASSERT_TRUE(fs::exists(core_dir))
        << "core dir not found: " << core_dir.string();

    int scanned = 0;
    std::vector<std::string> violations;

    for (const auto& entry : fs::recursive_directory_iterator(core_dir)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& path = entry.path();
        if (!IsScannableSource(path)) continue;
        ++scanned;

        std::ifstream in(path);
        ASSERT_TRUE(in) << "cannot open " << path.string();

        std::string line;
        int line_no = 0;
        while (std::getline(in, line)) {
            ++line_no;
            for (const auto& tok : ForbiddenTokens()) {
                if (line.find(tok) != std::string::npos) {
                    std::ostringstream msg;
                    msg << path.filename().string() << ":" << line_no
                        << " contains forbidden token '" << tok << "'";
                    violations.push_back(msg.str());
                }
            }
        }
    }

    EXPECT_GT(scanned, 0) << "no core source files scanned — wrong path?";
    EXPECT_TRUE(violations.empty())
        << "EditorCore must stay UI-framework free. Violations:\n"
        << [&] {
               std::ostringstream o;
               for (const auto& v : violations) o << "  - " << v << "\n";
               return o.str();
           }();
}
