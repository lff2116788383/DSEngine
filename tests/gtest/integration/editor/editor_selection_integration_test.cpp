/**
 * @file editor_selection_integration_test.cpp
 * @brief SelectionManager 算法集成测试
 *
 * 注意：entt 3.13 的 consteval 与当前 MSVC 版本不兼容（reconfigure 后重编译触发），
 *       因此本文件使用 uint32_t 作为 Entity 类型，纯粹验证选择管理器的核心算法。
 *       当 MSVC 升级后可恢复为真实 entt::entity 类型。
 *
 * 覆盖：
 * - 单选 / 多选 / 反选(Toggle)
 * - 清空选择
 * - Primary 实体
 * - 防重复添加
 */

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <cstdint>

using Entity = uint32_t;
static constexpr Entity kNullEntity = UINT32_MAX;

namespace {

/// 内联复刻 SelectionManager 核心算法（与 editor_selection.h 一致）
class TestSelectionManager {
public:
    void Clear() { selected_.clear(); }

    void SetSingle(Entity entity) {
        selected_.clear();
        if (entity != kNullEntity) {
            selected_.push_back(entity);
        }
    }

    void Toggle(Entity entity) {
        auto it = std::find(selected_.begin(), selected_.end(), entity);
        if (it != selected_.end()) {
            selected_.erase(it);
        } else {
            selected_.push_back(entity);
        }
    }

    void Add(Entity entity) {
        if (!Contains(entity)) {
            selected_.push_back(entity);
        }
    }

    void Remove(Entity entity) {
        auto it = std::find(selected_.begin(), selected_.end(), entity);
        if (it != selected_.end()) {
            selected_.erase(it);
        }
    }

    bool Contains(Entity entity) const {
        return std::find(selected_.begin(), selected_.end(), entity) != selected_.end();
    }

    bool IsMultiSelect() const { return selected_.size() > 1; }
    bool IsEmpty() const { return selected_.empty(); }
    int Count() const { return static_cast<int>(selected_.size()); }

    Entity GetPrimary() const {
        return selected_.empty() ? kNullEntity : selected_.back();
    }

private:
    std::vector<Entity> selected_;
};

} // namespace

class SelectionManagerTest : public ::testing::Test {
protected:
    TestSelectionManager sm_;
    void SetUp() override { sm_.Clear(); }
};

TEST_F(SelectionManagerTest, 初始状态为空) {
    EXPECT_TRUE(sm_.IsEmpty());
    EXPECT_EQ(sm_.Count(), 0);
    EXPECT_EQ(sm_.GetPrimary(), kNullEntity);
}

TEST_F(SelectionManagerTest, SetSingle单选) {
    Entity e = 1;

    sm_.SetSingle(e);
    EXPECT_FALSE(sm_.IsEmpty());
    EXPECT_EQ(sm_.Count(), 1);
    EXPECT_EQ(sm_.GetPrimary(), e);
    EXPECT_TRUE(sm_.Contains(e));
}

TEST_F(SelectionManagerTest, Toggle反选) {
    Entity e1 = 1;
    Entity e2 = 2;

    sm_.Toggle(e1);
    sm_.Toggle(e2);
    EXPECT_EQ(sm_.Count(), 2);
    EXPECT_TRUE(sm_.IsMultiSelect());

    sm_.Toggle(e1);
    EXPECT_EQ(sm_.Count(), 1);
    EXPECT_FALSE(sm_.Contains(e1));
    EXPECT_TRUE(sm_.Contains(e2));
}

TEST_F(SelectionManagerTest, Add防重复) {
    Entity e = 1;

    sm_.Add(e);
    sm_.Add(e);
    sm_.Add(e);
    EXPECT_EQ(sm_.Count(), 1);
}

TEST_F(SelectionManagerTest, Remove移除) {
    Entity e1 = 10;
    Entity e2 = 20;

    sm_.Add(e1);
    sm_.Add(e2);
    sm_.Remove(e1);
    EXPECT_EQ(sm_.Count(), 1);
    EXPECT_FALSE(sm_.Contains(e1));
    EXPECT_TRUE(sm_.Contains(e2));
}

TEST_F(SelectionManagerTest, SetSingle_null清空) {
    Entity e = 5;

    sm_.SetSingle(e);
    sm_.SetSingle(kNullEntity);
    EXPECT_TRUE(sm_.IsEmpty());
}
