/**
 * @file sub_scene_test.cpp
 * @brief SubScene 单元测试
 */

#include <gtest/gtest.h>
#include "engine/scene/sub_scene.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include <filesystem>
#include <fstream>

using namespace scene;

namespace {

std::filesystem::path WriteTempScene(const std::string& name, const std::string& json) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << json;
    out.close();
    return path;
}

const char* kTwoEntityScene = R"({
    "name": "sub_test",
    "entities": [
        { "id": 1, "components": { "TransformComponent": { "position": [1,2,3], "rotation": [0,0,0,1], "scale": [1,1,1] } } },
        { "id": 2, "components": { "TransformComponent": { "position": [4,5,6], "rotation": [0,0,0,1], "scale": [2,2,2] } } }
    ]
})";

const char* kEmptyScene = R"({ "name": "empty" })";

const char* kSingleEntityScene = R"({
    "name": "single",
    "entities": [
        { "id": 10, "components": { "TransformComponent": { "position": [7,8,9], "rotation": [0,0,0,1], "scale": [1,1,1] } } }
    ]
})";

} // namespace

class SubSceneTest : public ::testing::Test {
protected:
    World world;
    AssetManager asset_manager;

    void TearDown() override {
        world.Clear();
    }
};

TEST_F(SubSceneTest, 默认状态为Unloaded) {
    SubScene sub;
    EXPECT_EQ(sub.GetState(), SubSceneState::Unloaded);
    EXPECT_FALSE(sub.IsLoaded());
    EXPECT_EQ(sub.EntityCount(), 0u);
}

TEST_F(SubSceneTest, 构造函数设置路径) {
    SubScene sub("test/path.dscene");
    EXPECT_EQ(sub.GetPath(), "test/path.dscene");
    EXPECT_EQ(sub.GetState(), SubSceneState::Unloaded);
}

TEST_F(SubSceneTest, Load反序列化实体到World) {
    auto path = WriteTempScene("dse_sub_scene_load.dscene", kTwoEntityScene);
    SubScene sub;
    bool ok = sub.Load(world, asset_manager, path.string());
    EXPECT_TRUE(ok);
    EXPECT_TRUE(sub.IsLoaded());
    EXPECT_EQ(sub.EntityCount(), 2u);
    EXPECT_EQ(world.EntityCount(), 2u);

    // 验证 Transform 数据
    const auto& entities = sub.GetEntities();
    auto& t0 = world.registry().get<TransformComponent>(entities[0]);
    EXPECT_FLOAT_EQ(t0.position.x, 1.0f);
    EXPECT_FLOAT_EQ(t0.position.y, 2.0f);
    auto& t1 = world.registry().get<TransformComponent>(entities[1]);
    EXPECT_FLOAT_EQ(t1.position.x, 4.0f);

    std::filesystem::remove(path);
}

TEST_F(SubSceneTest, Unload销毁所有拥有的Entity) {
    auto path = WriteTempScene("dse_sub_scene_unload.dscene", kTwoEntityScene);
    SubScene sub;
    sub.Load(world, asset_manager, path.string());
    EXPECT_EQ(world.EntityCount(), 2u);

    sub.Unload(world);
    EXPECT_EQ(sub.GetState(), SubSceneState::Unloaded);
    EXPECT_EQ(sub.EntityCount(), 0u);
    EXPECT_EQ(world.EntityCount(), 0u);

    std::filesystem::remove(path);
}

TEST_F(SubSceneTest, 空场景加载成功) {
    auto path = WriteTempScene("dse_sub_scene_empty.dscene", kEmptyScene);
    SubScene sub;
    bool ok = sub.Load(world, asset_manager, path.string());
    EXPECT_TRUE(ok);
    EXPECT_TRUE(sub.IsLoaded());
    EXPECT_EQ(sub.EntityCount(), 0u);
    std::filesystem::remove(path);
}

TEST_F(SubSceneTest, 不存在的文件加载失败) {
    SubScene sub;
    bool ok = sub.Load(world, asset_manager, "nonexistent_file_12345.dscene");
    EXPECT_FALSE(ok);
    EXPECT_EQ(sub.GetState(), SubSceneState::Unloaded);
}

TEST_F(SubSceneTest, 重复Load返回false) {
    auto path = WriteTempScene("dse_sub_scene_double.dscene", kTwoEntityScene);
    SubScene sub;
    EXPECT_TRUE(sub.Load(world, asset_manager, path.string()));
    EXPECT_FALSE(sub.Load(world, asset_manager, path.string()));
    EXPECT_EQ(sub.EntityCount(), 2u);
    std::filesystem::remove(path);
}

TEST_F(SubSceneTest, Unload后可重新Load) {
    auto path = WriteTempScene("dse_sub_scene_reload.dscene", kSingleEntityScene);
    SubScene sub;
    sub.Load(world, asset_manager, path.string());
    EXPECT_EQ(sub.EntityCount(), 1u);
    sub.Unload(world);
    EXPECT_EQ(sub.EntityCount(), 0u);

    bool ok = sub.Load(world, asset_manager, path.string());
    EXPECT_TRUE(ok);
    EXPECT_EQ(sub.EntityCount(), 1u);
    std::filesystem::remove(path);
}

TEST_F(SubSceneTest, 多SubScene共享World互不干扰) {
    auto path1 = WriteTempScene("dse_sub1.dscene", kTwoEntityScene);
    auto path2 = WriteTempScene("dse_sub2.dscene", kSingleEntityScene);

    SubScene sub1, sub2;
    sub1.Load(world, asset_manager, path1.string());
    sub2.Load(world, asset_manager, path2.string());

    EXPECT_EQ(sub1.EntityCount(), 2u);
    EXPECT_EQ(sub2.EntityCount(), 1u);
    EXPECT_EQ(world.EntityCount(), 3u);

    // 卸载 sub1，sub2 不受影响
    sub1.Unload(world);
    EXPECT_EQ(world.EntityCount(), 1u);
    EXPECT_EQ(sub2.EntityCount(), 1u);
    EXPECT_TRUE(sub2.IsLoaded());

    for (auto e : sub2.GetEntities()) {
        EXPECT_TRUE(world.IsAlive(e));
    }

    std::filesystem::remove(path1);
    std::filesystem::remove(path2);
}

TEST_F(SubSceneTest, 未加载时Unload无副作用) {
    SubScene sub;
    sub.Unload(world);
    EXPECT_EQ(sub.GetState(), SubSceneState::Unloaded);
    EXPECT_EQ(sub.EntityCount(), 0u);
}

TEST_F(SubSceneTest, 移动语义转移所有权) {
    auto path = WriteTempScene("dse_sub_move.dscene", kTwoEntityScene);
    SubScene sub1;
    sub1.Load(world, asset_manager, path.string());

    SubScene sub2(std::move(sub1));
    EXPECT_TRUE(sub2.IsLoaded());
    EXPECT_EQ(sub2.EntityCount(), 2u);
    EXPECT_EQ(sub1.GetState(), SubSceneState::Unloaded);
    EXPECT_EQ(sub1.EntityCount(), 0u);

    sub2.Unload(world);
    EXPECT_EQ(world.EntityCount(), 0u);
    std::filesystem::remove(path);
}
