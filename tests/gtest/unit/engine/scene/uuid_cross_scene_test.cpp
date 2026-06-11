/**
 * @file uuid_cross_scene_test.cpp
 * @brief UUID 组件与跨场景 Entity 引用单元测试（Phase 4）
 */

#include <gtest/gtest.h>
#include "engine/ecs/uuid_component.h"
#include "engine/scene/scene_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/event_bus.h"
#include <filesystem>
#include <fstream>
#include <set>

using namespace scene;
using namespace dse::core;

// ========== UUIDComponent 单元测试 ==========

TEST(UUIDComponentTest, GenerategenerateANonZeroValue) {
    auto id = UUIDComponent::Generate();
    EXPECT_NE(id, 0u);
}

TEST(UUIDComponentTest, GeneratemultipleTimesWithoutRepeating) {
    std::set<uint64_t> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.insert(UUIDComponent::Generate());
    }
    EXPECT_EQ(ids.size(), 1000u);
}

TEST(UUIDComponentTest, ToStringAndFromStringreciprocal) {
    UUIDComponent comp;
    comp.uuid = 0xDEADBEEFCAFE1234ULL;
    std::string str = comp.ToString();
    EXPECT_EQ(UUIDComponent::FromString(str), comp.uuid);
}

TEST(UUIDComponentTest, FromStringEmptyStringReturnsZero) {
    EXPECT_EQ(UUIDComponent::FromString(""), 0u);
}

// ========== 跨场景引用测试 ==========

namespace {

std::filesystem::path WriteTempScene(const std::string& name, const std::string& json) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << json;
    out.close();
    return path;
}

const char* kSceneWithUUID_A = R"({ "name": "uuid_a", "entities": [
    { "id": 1, "components": {
        "TransformComponent": { "position": [1,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] },
        "UUIDComponent": { "uuid": "00000000000000aa" }
    }},
    { "id": 2, "components": {
        "TransformComponent": { "position": [2,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] },
        "UUIDComponent": { "uuid": "00000000000000bb" }
    }}
]})";

const char* kSceneWithUUID_B = R"({ "name": "uuid_b", "entities": [
    { "id": 1, "components": {
        "TransformComponent": { "position": [3,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] },
        "UUIDComponent": { "uuid": "00000000000000cc" }
    }}
]})";

const char* kSceneNoUUID = R"({ "name": "no_uuid", "entities": [
    { "id": 1, "components": {
        "TransformComponent": { "position": [4,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] }
    }}
]})";

} // namespace

class CrossSceneRefTest : public ::testing::Test {
protected:
    World world;
    AssetManager asset_manager;
    EventBus event_bus;
    SceneManager mgr;

    void SetUp() override {
        mgr.SetWorld(&world);
        mgr.SetAssetManager(&asset_manager);
        mgr.SetEventBus(&event_bus);
    }

    void TearDown() override {
        mgr.UnloadAll();
    }
};

TEST_F(CrossSceneRefTest, UUIDserializationdeserialization) {
    auto path = WriteTempScene("dse_uuid_ser.dscene", kSceneWithUUID_A);
    mgr.LoadSubScene(path.string());

    auto* sub = mgr.GetSubScene(path.string());
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->EntityCount(), 2u);

    // 验证 UUID 组件已正确附加
    const auto& entities = sub->GetEntities();
    EXPECT_TRUE(world.registry().all_of<UUIDComponent>(entities[0]));
    EXPECT_TRUE(world.registry().all_of<UUIDComponent>(entities[1]));
    EXPECT_EQ(world.registry().get<UUIDComponent>(entities[0]).uuid, 0xaa);
    EXPECT_EQ(world.registry().get<UUIDComponent>(entities[1]).uuid, 0xbb);

    std::filesystem::remove(path);
}

TEST_F(CrossSceneRefTest, ResolveReferenceSearchWithinASingleScene) {
    auto path = WriteTempScene("dse_uuid_resolve1.dscene", kSceneWithUUID_A);
    mgr.LoadSubScene(path.string());

    Entity found = mgr.ResolveReference(0xaa);
    EXPECT_TRUE(found != entt::null);
    EXPECT_FLOAT_EQ(world.registry().get<TransformComponent>(found).position.x, 1.0f);

    found = mgr.ResolveReference(0xbb);
    EXPECT_TRUE(found != entt::null);
    EXPECT_FLOAT_EQ(world.registry().get<TransformComponent>(found).position.x, 2.0f);

    std::filesystem::remove(path);
}

TEST_F(CrossSceneRefTest, ResolveReferenceAcrossMultipleSubSceneFind) {
    auto pathA = WriteTempScene("dse_uuid_cross_a.dscene", kSceneWithUUID_A);
    auto pathB = WriteTempScene("dse_uuid_cross_b.dscene", kSceneWithUUID_B);

    mgr.LoadSubScene(pathA.string());
    mgr.LoadSubScene(pathB.string());
    EXPECT_EQ(mgr.LoadedCount(), 2u);

    // 跨场景查找
    Entity e_aa = mgr.ResolveReference(0xaa);
    Entity e_cc = mgr.ResolveReference(0xcc);
    EXPECT_TRUE(e_aa != entt::null);
    EXPECT_TRUE(e_cc != entt::null);
    EXPECT_FLOAT_EQ(world.registry().get<TransformComponent>(e_aa).position.x, 1.0f);
    EXPECT_FLOAT_EQ(world.registry().get<TransformComponent>(e_cc).position.x, 3.0f);

    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

TEST_F(CrossSceneRefTest, ResolveReferencedoesNotExistUUIDReturnsnull) {
    auto path = WriteTempScene("dse_uuid_miss.dscene", kSceneWithUUID_A);
    mgr.LoadSubScene(path.string());

    Entity found = mgr.ResolveReference(0xFFFF);
    EXPECT_TRUE(found == entt::null);

    std::filesystem::remove(path);
}

TEST_F(CrossSceneRefTest, ResolveReference_UUIDReturnsZeronull) {
    auto path = WriteTempScene("dse_uuid_zero.dscene", kSceneWithUUID_A);
    mgr.LoadSubScene(path.string());
    EXPECT_TRUE(mgr.ResolveReference(0) == entt::null);
    std::filesystem::remove(path);
}

TEST_F(CrossSceneRefTest, SubSceneAfterUUIDNotAgainCan) {
    auto path = WriteTempScene("dse_uuid_unload.dscene", kSceneWithUUID_A);
    mgr.LoadSubScene(path.string());
    EXPECT_TRUE(mgr.ResolveReference(0xaa) != entt::null);

    mgr.UnloadSubScene(path.string());
    EXPECT_TRUE(mgr.ResolveReference(0xaa) == entt::null);

    std::filesystem::remove(path);
}

TEST_F(CrossSceneRefTest, WithoutUUIDComponentEntityNotFind) {
    auto pathUuid = WriteTempScene("dse_uuid_mix_a.dscene", kSceneWithUUID_A);
    auto pathNoUuid = WriteTempScene("dse_uuid_mix_b.dscene", kSceneNoUUID);

    mgr.LoadSubScene(pathUuid.string());
    mgr.LoadSubScene(pathNoUuid.string());

    EXPECT_TRUE(mgr.ResolveReference(0xaa) != entt::null);
    EXPECT_EQ(world.EntityCount(), 3u);

    std::filesystem::remove(pathUuid);
    std::filesystem::remove(pathNoUuid);
}
