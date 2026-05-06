/**
 * @file editor_undo.h
 * @brief 编辑器 Undo/Redo 系统，基于 Command Pattern 实现
 */

#ifndef DSE_EDITOR_UNDO_H
#define DSE_EDITOR_UNDO_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <deque>

namespace dse {
namespace editor {

/**
 * @class ICommand
 * @brief 命令接口，所有可撤销操作的基类
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief 执行命令
     */
    virtual void Execute() = 0;

    /**
     * @brief 撤销命令
     */
    virtual void Undo() = 0;

    /**
     * @brief 获取命令描述（用于 UI 显示）
     */
    virtual std::string GetDescription() const = 0;

    /**
     * @brief 尝试与另一个命令合并（用于连续拖拽等场景）
     * @param other 另一个命令
     * @return 是否成功合并
     */
    virtual bool MergeWith(const ICommand* /*other*/) { return false; }
};

/**
 * @class PropertyChangeCommand
 * @brief 通用属性修改命令，使用 lambda 捕获 get/set 逻辑
 */
template<typename T>
class PropertyChangeCommand : public ICommand {
public:
    PropertyChangeCommand(
        const std::string& description,
        T old_value,
        T new_value,
        std::function<void(const T&)> setter
    ) : description_(description)
      , old_value_(old_value)
      , new_value_(new_value)
      , setter_(setter) {}

    void Execute() override { setter_(new_value_); }
    void Undo() override { setter_(old_value_); }
    std::string GetDescription() const override { return description_; }

    bool MergeWith(const ICommand* other) override {
        auto* typed = dynamic_cast<const PropertyChangeCommand<T>*>(other);
        if (typed && typed->description_ == description_) {
            new_value_ = typed->new_value_;
            return true;
        }
        return false;
    }

    const T& GetOldValue() const { return old_value_; }
    const T& GetNewValue() const { return new_value_; }

private:
    std::string description_;
    T old_value_;
    T new_value_;
    std::function<void(const T&)> setter_;
};

/**
 * @class LambdaCommand
 * @brief 基于 lambda 的通用命令
 */
class LambdaCommand : public ICommand {
public:
    LambdaCommand(
        const std::string& description,
        std::function<void()> execute_fn,
        std::function<void()> undo_fn,
        const std::string& merge_id = ""
    ) : description_(description)
      , execute_fn_(execute_fn)
      , undo_fn_(undo_fn)
      , merge_id_(merge_id) {}

    void Execute() override { execute_fn_(); }
    void Undo() override { undo_fn_(); }
    std::string GetDescription() const override { return description_; }

    bool MergeWith(const ICommand* other) override {
        if (merge_id_.empty()) return false;
        auto* typed = dynamic_cast<const LambdaCommand*>(other);
        if (typed && typed->merge_id_ == merge_id_) {
            // Keep our undo (earliest state), take their execute (latest state)
            execute_fn_ = typed->execute_fn_;
            return true;
        }
        return false;
    }

    const std::string& GetMergeId() const { return merge_id_; }

private:
    std::string description_;
    std::function<void()> execute_fn_;
    std::function<void()> undo_fn_;
    std::string merge_id_;
};

/**
 * @class CompoundCommand
 * @brief 复合命令，将多个命令组合为一个原子操作
 */
class CompoundCommand : public ICommand {
public:
    explicit CompoundCommand(const std::string& description)
        : description_(description) {}

    void AddCommand(std::unique_ptr<ICommand> cmd) {
        commands_.push_back(std::move(cmd));
    }

    void Execute() override {
        for (auto& cmd : commands_) {
            cmd->Execute();
        }
    }

    void Undo() override {
        // Undo in reverse order
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            (*it)->Undo();
        }
    }

    std::string GetDescription() const override { return description_; }

    bool IsEmpty() const { return commands_.empty(); }

private:
    std::string description_;
    std::vector<std::unique_ptr<ICommand>> commands_;
};

/**
 * @class UndoRedoManager
 * @brief Undo/Redo 管理器，维护命令历史栈
 */
class UndoRedoManager {
public:
    explicit UndoRedoManager(int max_history = 100)
        : max_history_(max_history) {}

    /**
     * @brief 执行一个命令并加入历史
     * @param cmd 要执行的命令
     * @param try_merge 是否尝试与上一个命令合并
     */
    void Execute(std::unique_ptr<ICommand> cmd, bool try_merge = false) {
        cmd->Execute();

        if (try_merge && !undo_stack_.empty()) {
            if (undo_stack_.back()->MergeWith(cmd.get())) {
                // Merged successfully, don't add new command
                redo_stack_.clear();
                return;
            }
        }

        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear();

        // Trim history
        while (static_cast<int>(undo_stack_.size()) > max_history_) {
            undo_stack_.pop_front();
        }
    }

    /**
     * @brief 撤销上一个命令
     * @return 是否成功撤销
     */
    bool Undo() {
        if (undo_stack_.empty()) return false;
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->Undo();
        redo_stack_.push_back(std::move(cmd));
        return true;
    }

    /**
     * @brief 重做上一个被撤销的命令
     * @return 是否成功重做
     */
    bool Redo() {
        if (redo_stack_.empty()) return false;
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->Execute();
        undo_stack_.push_back(std::move(cmd));
        return true;
    }

    /**
     * @brief 是否可以撤销
     */
    bool CanUndo() const { return !undo_stack_.empty(); }

    /**
     * @brief 是否可以重做
     */
    bool CanRedo() const { return !redo_stack_.empty(); }

    /**
     * @brief 获取撤销栈中最近命令的描述
     */
    std::string GetUndoDescription() const {
        return undo_stack_.empty() ? "" : undo_stack_.back()->GetDescription();
    }

    /**
     * @brief 获取重做栈中最近命令的描述
     */
    std::string GetRedoDescription() const {
        return redo_stack_.empty() ? "" : redo_stack_.back()->GetDescription();
    }

    /**
     * @brief 获取撤销历史列表（从最新到最旧）
     */
    std::vector<std::string> GetUndoHistory() const {
        std::vector<std::string> history;
        for (auto it = undo_stack_.rbegin(); it != undo_stack_.rend(); ++it) {
            history.push_back((*it)->GetDescription());
        }
        return history;
    }

    /**
     * @brief 获取重做历史列表
     */
    std::vector<std::string> GetRedoHistory() const {
        std::vector<std::string> history;
        for (auto it = redo_stack_.rbegin(); it != redo_stack_.rend(); ++it) {
            history.push_back((*it)->GetDescription());
        }
        return history;
    }

    /**
     * @brief 清空所有历史
     */
    void Clear() {
        undo_stack_.clear();
        redo_stack_.clear();
    }

    /**
     * @brief 获取当前撤销栈深度
     */
    int GetUndoCount() const { return static_cast<int>(undo_stack_.size()); }

    /**
     * @brief 获取当前重做栈深度
     */
    int GetRedoCount() const { return static_cast<int>(redo_stack_.size()); }

private:
    std::deque<std::unique_ptr<ICommand>> undo_stack_;
    std::vector<std::unique_ptr<ICommand>> redo_stack_;
    int max_history_;
};

} // namespace editor
} // namespace dse

#endif
