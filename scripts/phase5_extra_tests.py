#!/usr/bin/env python3
"""Phase 5 extra: Additional tests to reach 200+ total."""
import os
import re

WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST_FILE = os.path.join(WORKSPACE, "tests", "gtest", "unit", "engine",
                         "scripting", "dse_api_bindings_test.cpp")

EXTRA_TESTS = r'''
// ============================================================
// Phase 5 Extra: Additional Coverage to reach 200+
// ============================================================

// --- WorldPartitionConfig ---
TEST_F(DseApiBindingsTest, WorldPartitionConfig_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::WorldPartitionConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_world_partition_set_enabled(id, 0);
    EXPECT_EQ(dse_world_partition_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, WorldPartitionConfig_CellSizeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::WorldPartitionConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_world_partition_set_cell_size(id, 128.0f);
    EXPECT_FLOAT_EQ(dse_world_partition_get_cell_size(id), 128.0f);
}

// --- HLODConfig ---
TEST_F(DseApiBindingsTest, HLODConfig_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::HLODConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_hlod_config_set_enabled(id, 0);
    EXPECT_EQ(dse_hlod_config_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, HLODConfig_MaxLevelRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::HLODConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_hlod_config_set_max_level(id, 4);
    EXPECT_EQ(dse_hlod_config_get_max_level(id), 4);
}

// --- VirtualTexture ---
TEST_F(DseApiBindingsTest, VirtualTexture_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::VirtualTextureComponent>(e);
    const uint32_t id = EntityId(e);
    dse_virtual_texture_set_enabled(id, 0);
    EXPECT_EQ(dse_virtual_texture_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, VirtualTexture_TileSizeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::VirtualTextureComponent>(e);
    const uint32_t id = EntityId(e);
    dse_virtual_texture_set_tile_size(id, 256);
    EXPECT_EQ(dse_virtual_texture_get_tile_size(id), 256);
}

// --- Lightmap ---
TEST_F(DseApiBindingsTest, Lightmap_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::LightmapComponent>(e);
    const uint32_t id = EntityId(e);
    dse_lightmap_set_enabled(id, 0);
    EXPECT_EQ(dse_lightmap_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Lightmap_ResolutionRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::LightmapComponent>(e);
    const uint32_t id = EntityId(e);
    dse_lightmap_set_resolution(id, 1024);
    EXPECT_EQ(dse_lightmap_get_resolution(id), 1024);
}

// --- CharacterMovementConfig ---
TEST_F(DseApiBindingsTest, CharMovementConfig_WalkSpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::CharacterMovementConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_character_movement_cfg_set_walk_speed(id, 5.0f);
    EXPECT_FLOAT_EQ(dse_character_movement_cfg_get_walk_speed(id), 5.0f);
}

TEST_F(DseApiBindingsTest, CharMovementConfig_RunSpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::CharacterMovementConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_character_movement_cfg_set_run_speed(id, 10.0f);
    EXPECT_FLOAT_EQ(dse_character_movement_cfg_get_run_speed(id), 10.0f);
}

TEST_F(DseApiBindingsTest, CharMovementConfig_JumpHeightRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::CharacterMovementConfigComponent>(e);
    const uint32_t id = EntityId(e);
    dse_character_movement_cfg_set_jump_height(id, 2.0f);
    EXPECT_FLOAT_EQ(dse_character_movement_cfg_get_jump_height(id), 2.0f);
}

// --- CharacterMovementState ---
TEST_F(DseApiBindingsTest, CharMovementState_GroundedDefault) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::CharacterMovementStateComponent>(e);
    const uint32_t id = EntityId(e);
    int grounded = dse_character_movement_get_is_grounded(id);
    EXPECT_GE(grounded, 0);
}

// --- SpringArm3D ---
TEST_F(DseApiBindingsTest, SpringArm_LengthRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SpringArm3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_spring_arm_set_arm_length(id, 5.0f);
    EXPECT_FLOAT_EQ(dse_spring_arm_get_arm_length(id), 5.0f);
}

TEST_F(DseApiBindingsTest, SpringArm_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SpringArm3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_spring_arm_set_enabled(id, 0);
    EXPECT_EQ(dse_spring_arm_get_enabled(id), 0);
}

// --- PlayerController ---
TEST_F(DseApiBindingsTest, PlayerController_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PlayerControllerComponent>(e);
    const uint32_t id = EntityId(e);
    dse_player_controller_set_enabled(id, 0);
    EXPECT_EQ(dse_player_controller_get_enabled(id), 0);
}

// --- MeshCollider3D ---
TEST_F(DseApiBindingsTest, MeshCollider3D_ConvexRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshCollider3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_mesh_collider3d_set_convex(id, 1);
    EXPECT_EQ(dse_mesh_collider3d_get_convex(id), 1);
}

// --- Joint3D extended ---
TEST_F(DseApiBindingsTest, Joint3D_TypeFieldRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Joint3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_joint3d_set_type(id, 1);
    EXPECT_EQ(dse_joint3d_get_type(id), 1);
}

// --- SoftBody extended ---
TEST_F(DseApiBindingsTest, SoftBody_StiffnessRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SoftBodyComponent>(e);
    const uint32_t id = EntityId(e);
    dse_soft_body_set_stiffness(id, 0.9f);
    EXPECT_FLOAT_EQ(dse_soft_body_get_stiffness(id), 0.9f);
}

// --- Vehicle extended ---
TEST_F(DseApiBindingsTest, Vehicle_MaxSpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::VehicleComponent>(e);
    const uint32_t id = EntityId(e);
    dse_vehicle_set_max_speed(id, 200.0f);
    EXPECT_FLOAT_EQ(dse_vehicle_get_max_speed(id), 200.0f);
}

// --- Ragdoll extended ---
TEST_F(DseApiBindingsTest, Ragdoll_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::RagdollComponent>(e);
    const uint32_t id = EntityId(e);
    dse_ragdoll_set_enabled(id, 0);
    EXPECT_EQ(dse_ragdoll_get_enabled(id), 0);
}

// --- Rope extended ---
TEST_F(DseApiBindingsTest, Rope_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::RopeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_rope_set_enabled(id, 0);
    EXPECT_EQ(dse_rope_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Rope_GravityRoundTrip) {
    Entity e = world_.CreateEntity();
    auto& rope = world_.registry().emplace<dse::RopeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_rope_set_gravity(id, 1, 2.0f);
    EXPECT_TRUE(rope.use_gravity);
    EXPECT_FLOAT_EQ(rope.gravity_scale, 2.0f);
}

// --- Animator3D extended ---
TEST_F(DseApiBindingsTest, Animator3D_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Animator3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_animator3d_set_enabled(id, 0);
    EXPECT_EQ(dse_animator3d_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Animator3D_SpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Animator3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_animator3d_set_speed(id, 2.0f);
    EXPECT_FLOAT_EQ(dse_animator3d_get_speed(id), 2.0f);
}

// --- TerrainTile ---
TEST_F(DseApiBindingsTest, TerrainTile_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::TerrainTileManagerComponent>(e);
    const uint32_t id = EntityId(e);
    dse_terrain_tile_set_enabled(id, 0);
    EXPECT_EQ(dse_terrain_tile_get_enabled(id), 0);
}

// --- PostProcess extended fields ---
TEST_F(DseApiBindingsTest, PostProcess_BloomIntensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PostProcessComponent>(e);
    const uint32_t id = EntityId(e);
    dse_post_process_set_bloom_intensity(id, 0.8f);
    EXPECT_FLOAT_EQ(dse_post_process_get_bloom_intensity(id), 0.8f);
}

TEST_F(DseApiBindingsTest, PostProcess_ExposureRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PostProcessComponent>(e);
    const uint32_t id = EntityId(e);
    dse_post_process_set_exposure(id, 1.5f);
    EXPECT_FLOAT_EQ(dse_post_process_get_exposure(id), 1.5f);
}

// --- Transform batch operations ---
TEST_F(DseApiBindingsTest, Transform_SetAndGet_AllAxes) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    for (int i = 0; i < 10; ++i) {
        float v = static_cast<float>(i) * 1.5f;
        dse_transform_set_position(id, v, v + 1.0f, v + 2.0f);
        float x, y, z;
        dse_transform_get_position(id, &x, &y, &z);
        EXPECT_FLOAT_EQ(x, v);
        EXPECT_FLOAT_EQ(y, v + 1.0f);
        EXPECT_FLOAT_EQ(z, v + 2.0f);
    }
}

// --- DirectionalLight extended ---
TEST_F(DseApiBindingsTest, DirLight_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DirectionalLight3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_dir_light_set_enabled(id, 0);
    EXPECT_EQ(dse_dir_light_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, DirLight_DirectionRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DirectionalLight3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_dir_light_set_direction(id, 0.0f, -1.0f, 0.5f);
    float dx, dy, dz;
    dse_dir_light_get_direction(id, &dx, &dy, &dz);
    EXPECT_FLOAT_EQ(dx, 0.0f);
    EXPECT_FLOAT_EQ(dy, -1.0f);
    EXPECT_FLOAT_EQ(dz, 0.5f);
}

// --- PointLight extended ---
TEST_F(DseApiBindingsTest, PointLight_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PointLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_point_light_set_enabled(id, 0);
    EXPECT_EQ(dse_point_light_get_enabled(id), 0);
}

// --- MeshRenderer path + lod ---
TEST_F(DseApiBindingsTest, MeshRenderer_PathRoundTrip_2) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);
    dse_mesh_renderer_set_mesh_path(id, "meshes/hero.dsmesh");
    char buf[128];
    int len = dse_mesh_renderer_get_mesh_path(id, buf, sizeof(buf));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buf, "meshes/hero.dsmesh");
}

// --- Stress: create-destroy cycle ---
TEST_F(DseApiBindingsTest, StressTest_CreateDestroyLoop) {
    for (int i = 0; i < 50; ++i) {
        uint32_t e = dse_entity_create();
        EXPECT_EQ(dse_entity_valid(e), 1);
        dse_entity_destroy(e);
        EXPECT_EQ(dse_entity_valid(e), 0);
    }
}

// --- Edge: component setters on non-existent entity ---
TEST_F(DseApiBindingsTest, MissingEntity_TransformDefaultsToZero) {
    const uint32_t bad = 0xFFFFFFFEu;
    float x = 999, y = 999, z = 999;
    dse_transform_get_position(bad, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 0.0f);
    EXPECT_FLOAT_EQ(y, 0.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}

// --- Edge: double set ---
TEST_F(DseApiBindingsTest, Transform_DoubleSet_TakesLast) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_position(id, 1.0f, 2.0f, 3.0f);
    dse_transform_set_position(id, 4.0f, 5.0f, 6.0f);
    float x, y, z;
    dse_transform_get_position(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 4.0f);
    EXPECT_FLOAT_EQ(y, 5.0f);
    EXPECT_FLOAT_EQ(z, 6.0f);
}

// --- Hair additional ---
TEST_F(DseApiBindingsTest, Hair_WidthRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::HairComponent>(e);
    const uint32_t id = EntityId(e);
    dse_hair_set_width(id, 0.02f);
    EXPECT_FLOAT_EQ(dse_hair_get_width(id), 0.02f);
}

// --- Impostor additional ---
TEST_F(DseApiBindingsTest, Impostor_FrameCountRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ImpostorComponent>(e);
    const uint32_t id = EntityId(e);
    dse_impostor_set_frame_count(id, 16);
    EXPECT_EQ(dse_impostor_get_frame_count(id), 16);
}

// --- Atmosphere additional ---
TEST_F(DseApiBindingsTest, Atmosphere_MieScatteringRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::AtmosphereComponent>(e);
    const uint32_t id = EntityId(e);
    dse_atmosphere_set_mie_scattering(id, 0.003f);
    EXPECT_FLOAT_EQ(dse_atmosphere_get_mie_scattering(id), 0.003f);
}

// --- Foliage additional ---
TEST_F(DseApiBindingsTest, Foliage_LodDistanceRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::FoliageComponent>(e);
    const uint32_t id = EntityId(e);
    dse_foliage_set_lod_distance(id, 100.0f);
    EXPECT_FLOAT_EQ(dse_foliage_get_lod_distance(id), 100.0f);
}

// --- Water additional ---
TEST_F(DseApiBindingsTest, Water_TransparencyRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::WaterComponent>(e);
    const uint32_t id = EntityId(e);
    dse_water_set_transparency(id, 0.8f);
    EXPECT_FLOAT_EQ(dse_water_get_transparency(id), 0.8f);
}

// --- VolumetricCloud additional ---
TEST_F(DseApiBindingsTest, VolumetricCloud_DensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::VolumetricCloudComponent>(e);
    const uint32_t id = EntityId(e);
    dse_volumetric_cloud_set_density(id, 1.2f);
    EXPECT_FLOAT_EQ(dse_volumetric_cloud_get_density(id), 1.2f);
}

// --- DayNight additional ---
TEST_F(DseApiBindingsTest, DayNight_NightSpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DayNightCycleComponent>(e);
    const uint32_t id = EntityId(e);
    dse_day_night_set_night_speed(id, 3.0f);
    EXPECT_FLOAT_EQ(dse_day_night_get_night_speed(id), 3.0f);
}

// --- Camera3D aspect ratio ---
TEST_F(DseApiBindingsTest, Camera3D_AspectRatioRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Camera3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_camera3d_set_aspect_ratio(id, 1.77f);
    EXPECT_NEAR(dse_camera3d_get_aspect_ratio(id), 1.77f, 0.01f);
}

// --- Batch safety: multiple invalid getters ---
TEST_F(DseApiBindingsTest, BatchInvalid_PhysicsComponentsDefault) {
    const uint32_t inv = 0xFFFFFFFEu;
    EXPECT_EQ(dse_rigidbody3d_get_type(inv), 0);
    EXPECT_FLOAT_EQ(dse_rigidbody3d_get_mass(inv), 1.0f);
    EXPECT_FLOAT_EQ(dse_sphere_collider3d_get_radius(inv), 0.5f);
}

TEST_F(DseApiBindingsTest, BatchInvalid_RenderComponentsDefault) {
    const uint32_t inv = 0xFFFFFFFEu;
    EXPECT_EQ(dse_mesh_renderer_get_enabled(inv), 1);
    EXPECT_EQ(dse_post_process_get_enabled(inv), 1);
    EXPECT_EQ(dse_tree_get_enabled(inv), 1);
}

// --- Stress: many components on same entity ---
TEST_F(DseApiBindingsTest, StressTest_ManyComponentsSameEntity) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    world_.registry().emplace<dse::PostProcessComponent>(e);
    world_.registry().emplace<dse::PointLightComponent>(e);
    const uint32_t id = EntityId(e);

    dse_transform_set_position(id, 1.0f, 2.0f, 3.0f);
    dse_mesh_renderer_set_enabled(id, 1);
    dse_post_process_set_enabled(id, 1);
    dse_point_light_set_intensity(id, 5.0f);

    float x, y, z;
    dse_transform_get_position(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 1.0f);
    EXPECT_EQ(dse_mesh_renderer_get_enabled(id), 1);
    EXPECT_FLOAT_EQ(dse_point_light_get_intensity(id), 5.0f);
}

// --- Edge: zero-value floats ---
TEST_F(DseApiBindingsTest, PointLight_ZeroIntensity) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PointLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_point_light_set_intensity(id, 0.0f);
    EXPECT_FLOAT_EQ(dse_point_light_get_intensity(id), 0.0f);
}

TEST_F(DseApiBindingsTest, PointLight_VerySmallRange) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PointLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_point_light_set_range(id, 0.001f);
    EXPECT_FLOAT_EQ(dse_point_light_get_range(id), 0.001f);
}

// --- GIProbe extent ---
TEST_F(DseApiBindingsTest, GIProbe_ExtentRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::GIProbeVolumeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_gi_probe_set_extent(id, 10.0f, 20.0f, 30.0f);
    float x, y, z;
    dse_gi_probe_get_extent(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 10.0f);
    EXPECT_FLOAT_EQ(y, 20.0f);
    EXPECT_FLOAT_EQ(z, 30.0f);
}

// --- LightProbe position ---
TEST_F(DseApiBindingsTest, LightProbe_PositionOffset) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::LightProbeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_light_probe_set_offset(id, 0.0f, 1.0f, 0.0f);
    float x, y, z;
    dse_light_probe_get_offset(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(y, 1.0f);
}

// --- ReflectionProbe box ---
TEST_F(DseApiBindingsTest, ReflectionProbe_BoxExtentRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ReflectionProbeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_reflection_probe_set_box_extent(id, 5.0f, 5.0f, 5.0f);
    float x, y, z;
    dse_reflection_probe_get_box_extent(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 5.0f);
    EXPECT_FLOAT_EQ(z, 5.0f);
}
'''


def main():
    with open(TEST_FILE, "r", encoding="utf-8") as f:
        content = f.read()

    if "Phase 5 Extra" in content:
        print("[Phase5Extra] Extra tests already present, skipping.")
    else:
        content = content.rstrip() + "\n" + EXTRA_TESTS + "\n"
        with open(TEST_FILE, "w", encoding="utf-8") as f:
            f.write(content)

    count = len(re.findall(r"TEST_F\(", content))
    print(f"[Phase5Extra] Total test cases: {count}")


if __name__ == "__main__":
    main()
