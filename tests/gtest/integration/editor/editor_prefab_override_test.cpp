/**
 * @file editor_prefab_override_test.cpp
 * @brief Prefab override 纯逻辑核心（editor_prefab_override_core）的无头测试。
 *
 * 验证实例 entity 与磁盘 .dprefab 源文件之间的 diff（ComputePrefabOverrides）、
 * 单属性还原（RevertPrefabOverride）、全量还原（RevertAllPrefabOverrides）、
 * 反向写回源文件（ApplyOverridesToPrefab）。DrawPrefabOverrideSection（ImGui）不在此覆盖。
 */

#include <gtest/gtest.h>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "editor_prefab_override.h"
#include "editor_prefab.h"
#include "editor_prefab_marker.h"
#include "editor_shared_components.h"
#include "engine/ecs/components_3d.h"

using namespace dse::editor;

namespace {

constexpr const char* kPrefabJson = R"({
  "type": "dprefab",
  "version": 1,
  "name": "OriginalName",
  "transform": {
    "position": [1.0, 2.0, 3.0],
    "rotation": [1.0, 0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  },
  "mesh_renderer": {
    "mesh_path": "source.dmesh",
    "shader_variant": "MESH_UNLIT",
    "color": [1.0, 1.0, 1.0, 1.0],
    "metallic": 0.0,
    "roughness": 0.5,
    "ao": 1.0,
    "visible": true
  }
})";

class PrefabOverrideTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto dir = std::filesystem::temp_directory_path() / "dse_prefab_override_test";
        std::filesystem::create_directories(dir);
        prefab_path_ = (dir / "fixture.dprefab").string();
        WritePrefabFile(kPrefabJson);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(prefab_path_, ec);
    }

    void WritePrefabFile(const std::string& json) {
        std::ofstream ofs(prefab_path_, std::ios::trunc);
        ofs << json;
    }

    std::string ReadPrefabFile() const {
        std::ifstream ifs(prefab_path_);
        return std::string((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());
    }

    /// Create an entity whose components exactly match the source prefab file.
    entt::entity MakeMatchingInstance() {
        entt::entity e = registry_.create();
        registry_.emplace<PrefabMarkerComponent>(e, PrefabMarkerComponent{prefab_path_});
        registry_.emplace<EditorNameComponent>(e, EditorNameComponent{"OriginalName"});

        TransformComponent tf;
        tf.position = glm::vec3(1.0f, 2.0f, 3.0f);
        tf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        tf.scale = glm::vec3(1.0f);
        registry_.emplace<TransformComponent>(e, tf);

        dse::MeshRendererComponent mr;
        mr.mesh_path = "source.dmesh";
        mr.color = glm::vec4(1.0f);
        mr.metallic = 0.0f;
        mr.roughness = 0.5f;
        mr.visible = true;
        registry_.emplace<dse::MeshRendererComponent>(e, mr);
        return e;
    }

    entt::registry registry_;
    std::string prefab_path_;
};

}  // namespace

// ── ComputePrefabOverrides：退化输入 ─────────────────────────────────────────

TEST_F(PrefabOverrideTest, InvalidEntityYieldsEmpty) {
    auto info = ComputePrefabOverrides(registry_, entt::null);
    EXPECT_TRUE(info.overrides.empty());
    EXPECT_TRUE(info.prefab_source_path.empty());
}

TEST_F(PrefabOverrideTest, NonPrefabEntityYieldsEmpty) {
    entt::entity e = registry_.create();
    registry_.emplace<TransformComponent>(e);
    auto info = ComputePrefabOverrides(registry_, e);
    EXPECT_TRUE(info.overrides.empty());
    EXPECT_TRUE(info.prefab_source_path.empty());
}

// ── ComputePrefabOverrides：无差异 ───────────────────────────────────────────

TEST_F(PrefabOverrideTest, MatchingInstanceHasNoOverrides) {
    entt::entity e = MakeMatchingInstance();
    auto info = ComputePrefabOverrides(registry_, e);
    EXPECT_EQ(info.entity, e);
    EXPECT_EQ(info.prefab_source_path, prefab_path_);
    EXPECT_TRUE(info.overrides.empty()) << "expected clean instance, got "
                                        << info.overrides.size() << " overrides";
}

// ── ComputePrefabOverrides：检测各类差异 ─────────────────────────────────────

TEST_F(PrefabOverrideTest, DetectsTransformPositionOverride) {
    entt::entity e = MakeMatchingInstance();
    registry_.get<TransformComponent>(e).position = glm::vec3(9.0f, 2.0f, 3.0f);

    auto info = ComputePrefabOverrides(registry_, e);
    ASSERT_EQ(info.overrides.size(), 1u);
    EXPECT_EQ(info.overrides[0].component_name, "Transform");
    EXPECT_EQ(info.overrides[0].property_name, "Position");
}

TEST_F(PrefabOverrideTest, DetectsNameOverride) {
    entt::entity e = MakeMatchingInstance();
    registry_.get<EditorNameComponent>(e).name = "Renamed";

    auto info = ComputePrefabOverrides(registry_, e);
    ASSERT_EQ(info.overrides.size(), 1u);
    EXPECT_EQ(info.overrides[0].component_name, "Name");
    EXPECT_EQ(info.overrides[0].original_value, "OriginalName");
    EXPECT_EQ(info.overrides[0].current_value, "Renamed");
}

TEST_F(PrefabOverrideTest, DetectsMeshRendererOverrides) {
    entt::entity e = MakeMatchingInstance();
    auto& mr = registry_.get<dse::MeshRendererComponent>(e);
    mr.mesh_path = "changed.dmesh";
    mr.visible = false;
    mr.roughness = 0.9f;

    auto info = ComputePrefabOverrides(registry_, e);
    EXPECT_EQ(info.overrides.size(), 3u);
    bool path = false, vis = false, rough = false;
    for (auto& ov : info.overrides) {
        EXPECT_EQ(ov.component_name, "MeshRenderer");
        if (ov.property_name == "MeshPath") path = true;
        if (ov.property_name == "Visible") vis = true;
        if (ov.property_name == "Roughness") rough = true;
    }
    EXPECT_TRUE(path && vis && rough);
}

TEST_F(PrefabOverrideTest, DetectsMultipleComponentOverrides) {
    entt::entity e = MakeMatchingInstance();
    registry_.get<TransformComponent>(e).scale = glm::vec3(2.0f);
    registry_.get<EditorNameComponent>(e).name = "Renamed";

    auto info = ComputePrefabOverrides(registry_, e);
    EXPECT_EQ(info.overrides.size(), 2u);
}

// ── RevertPrefabOverride：单属性 ─────────────────────────────────────────────

TEST_F(PrefabOverrideTest, RevertSingleOverrideRestoresValue) {
    entt::entity e = MakeMatchingInstance();
    registry_.get<TransformComponent>(e).position = glm::vec3(9.0f, 9.0f, 9.0f);

    auto info = ComputePrefabOverrides(registry_, e);
    ASSERT_EQ(info.overrides.size(), 1u);
    EXPECT_TRUE(RevertPrefabOverride(registry_, e, info.overrides[0]));

    auto& tf = registry_.get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(tf.position.x, 1.0f);
    EXPECT_FLOAT_EQ(tf.position.y, 2.0f);
    EXPECT_FLOAT_EQ(tf.position.z, 3.0f);
    // recompute -> clean
    EXPECT_TRUE(ComputePrefabOverrides(registry_, e).overrides.empty());
}

TEST_F(PrefabOverrideTest, RevertReturnsFalseForNonInstance) {
    entt::entity e = registry_.create();
    registry_.emplace<TransformComponent>(e);
    PrefabPropertyOverride ov{"Transform", "Position", "", ""};
    EXPECT_FALSE(RevertPrefabOverride(registry_, e, ov));
}

// ── RevertAllPrefabOverrides ─────────────────────────────────────────────────

TEST_F(PrefabOverrideTest, RevertAllRestoresEverything) {
    entt::entity e = MakeMatchingInstance();
    registry_.get<TransformComponent>(e).position = glm::vec3(5.0f);
    registry_.get<TransformComponent>(e).scale = glm::vec3(7.0f);
    registry_.get<EditorNameComponent>(e).name = "Renamed";
    registry_.get<dse::MeshRendererComponent>(e).visible = false;

    EXPECT_TRUE(RevertAllPrefabOverrides(registry_, e));
    EXPECT_TRUE(ComputePrefabOverrides(registry_, e).overrides.empty());
}

TEST_F(PrefabOverrideTest, RevertAllReturnsFalseWhenClean) {
    entt::entity e = MakeMatchingInstance();
    EXPECT_FALSE(RevertAllPrefabOverrides(registry_, e));
}

// ── ApplyOverridesToPrefab：反向写回 ─────────────────────────────────────────

TEST_F(PrefabOverrideTest, ApplyWritesInstanceStateToSource) {
    entt::entity e = MakeMatchingInstance();
    registry_.get<EditorNameComponent>(e).name = "AppliedName";
    registry_.get<TransformComponent>(e).position = glm::vec3(4.0f, 5.0f, 6.0f);

    // sanity: there are overrides before apply
    ASSERT_FALSE(ComputePrefabOverrides(registry_, e).overrides.empty());

    EXPECT_TRUE(ApplyOverridesToPrefab(registry_, e));

    // After writing instance state back to the source file, the diff is clean.
    auto info = ComputePrefabOverrides(registry_, e);
    EXPECT_TRUE(info.overrides.empty()) << "apply should make instance match source";
    EXPECT_NE(ReadPrefabFile().find("AppliedName"), std::string::npos);
}

TEST_F(PrefabOverrideTest, ApplyReturnsFalseForNonInstance) {
    entt::entity e = registry_.create();
    EXPECT_FALSE(ApplyOverridesToPrefab(registry_, e));
}
