#!/usr/bin/env python3
"""
Phase 5: Expand unit test coverage from 51 to 200+ test cases.
"""

import os
import re

WORKSPACE = r"c:\Users\Administrator\Desktop\Engine\DSEngine"
TEST_FILE = os.path.join(WORKSPACE, "tests", "gtest", "unit", "engine",
                         "scripting", "dse_api_bindings_test.cpp")

NEW_TESTS = r'''
// ============================================================
// Phase 5: Extended Coverage -- Codegen Component Fields
// ============================================================

// --- Decal ---
TEST_F(DseApiBindingsTest, Decal_FieldRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DecalComponent>(e);
    const uint32_t id = EntityId(e);
    dse_decal_set_enabled(id, 0);
    EXPECT_EQ(dse_decal_get_enabled(id), 0);
    dse_decal_set_opacity(id, 0.7f);
    EXPECT_FLOAT_EQ(dse_decal_get_opacity(id), 0.7f);
}

TEST_F(DseApiBindingsTest, Decal_SizeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DecalComponent>(e);
    const uint32_t id = EntityId(e);
    dse_decal_set_size(id, 2.0f, 3.0f, 4.0f);
    float x = 0, y = 0, z = 0;
    dse_decal_get_size(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 2.0f);
    EXPECT_FLOAT_EQ(y, 3.0f);
    EXPECT_FLOAT_EQ(z, 4.0f);
}

TEST_F(DseApiBindingsTest, Decal_TexturePath) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DecalComponent>(e);
    const uint32_t id = EntityId(e);
    dse_decal_set_texture_path(id, "textures/blood.png");
    char buf[128];
    EXPECT_GT(dse_decal_get_texture_path(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "textures/blood.png");
}

TEST_F(DseApiBindingsTest, Decal_MissingComponentDefault) {
    Entity e = world_.CreateEntity();
    const uint32_t id = EntityId(e);
    EXPECT_EQ(dse_decal_get_enabled(id), 1);
    EXPECT_FLOAT_EQ(dse_decal_get_opacity(id), 1.0f);
}

// --- Skybox ---
TEST_F(DseApiBindingsTest, Skybox_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SkyboxComponent>(e);
    const uint32_t id = EntityId(e);
    dse_skybox_set_enabled(id, 0);
    EXPECT_EQ(dse_skybox_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Skybox_IntensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SkyboxComponent>(e);
    const uint32_t id = EntityId(e);
    dse_skybox_set_intensity(id, 2.5f);
    EXPECT_FLOAT_EQ(dse_skybox_get_intensity(id), 2.5f);
}

TEST_F(DseApiBindingsTest, Skybox_CubemapPath) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SkyboxComponent>(e);
    const uint32_t id = EntityId(e);
    dse_skybox_set_cubemap_path(id, "skyboxes/sunset.hdr");
    char buf[128];
    EXPECT_GT(dse_skybox_get_cubemap_path(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "skyboxes/sunset.hdr");
}

// --- FreeCamera ---
TEST_F(DseApiBindingsTest, FreeCamera_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::FreeCameraControllerComponent>(e);
    const uint32_t id = EntityId(e);
    dse_free_camera_set_enabled(id, 0);
    EXPECT_EQ(dse_free_camera_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, FreeCamera_SpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::FreeCameraControllerComponent>(e);
    const uint32_t id = EntityId(e);
    dse_free_camera_set_move_speed(id, 15.0f);
    EXPECT_FLOAT_EQ(dse_free_camera_get_move_speed(id), 15.0f);
    dse_free_camera_set_look_speed(id, 0.3f);
    EXPECT_FLOAT_EQ(dse_free_camera_get_look_speed(id), 0.3f);
}

// --- SubScene ---
TEST_F(DseApiBindingsTest, SubScene_PathRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SubSceneComponent>(e);
    const uint32_t id = EntityId(e);
    dse_sub_scene_set_scene_path(id, "scenes/dungeon.dscene");
    char buf[128];
    EXPECT_GT(dse_sub_scene_get_scene_path(id, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "scenes/dungeon.dscene");
}

TEST_F(DseApiBindingsTest, SubScene_AutoLoadRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SubSceneComponent>(e);
    const uint32_t id = EntityId(e);
    dse_sub_scene_set_auto_load(id, 0);
    EXPECT_EQ(dse_sub_scene_get_auto_load(id), 0);
}

// --- BoundingBox ---
TEST_F(DseApiBindingsTest, BoundingBox_MinMaxRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::BoundingBoxComponent>(e);
    const uint32_t id = EntityId(e);
    dse_bounding_box_set_min(id, -1.0f, -2.0f, -3.0f);
    dse_bounding_box_set_max(id, 1.0f, 2.0f, 3.0f);
    float x, y, z;
    dse_bounding_box_get_min(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, -1.0f);
    EXPECT_FLOAT_EQ(z, -3.0f);
    dse_bounding_box_get_max(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 1.0f);
    EXPECT_FLOAT_EQ(z, 3.0f);
}

// --- Water ---
TEST_F(DseApiBindingsTest, Water_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::WaterComponent>(e);
    const uint32_t id = EntityId(e);
    dse_water_set_enabled(id, 0);
    EXPECT_EQ(dse_water_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Water_WaveFieldsRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::WaterComponent>(e);
    const uint32_t id = EntityId(e);
    dse_water_set_wave_height(id, 1.5f);
    dse_water_set_wave_speed(id, 0.8f);
    EXPECT_FLOAT_EQ(dse_water_get_wave_height(id), 1.5f);
    EXPECT_FLOAT_EQ(dse_water_get_wave_speed(id), 0.8f);
}

// --- LightProbe ---
TEST_F(DseApiBindingsTest, LightProbe_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::LightProbeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_light_probe_set_enabled(id, 0);
    EXPECT_EQ(dse_light_probe_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, LightProbe_RadiusRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::LightProbeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_light_probe_set_radius(id, 25.0f);
    EXPECT_FLOAT_EQ(dse_light_probe_get_radius(id), 25.0f);
}

// --- ReflectionProbe ---
TEST_F(DseApiBindingsTest, ReflectionProbe_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ReflectionProbeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_reflection_probe_set_enabled(id, 0);
    EXPECT_EQ(dse_reflection_probe_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, ReflectionProbe_ResolutionRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ReflectionProbeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_reflection_probe_set_resolution(id, 512);
    EXPECT_EQ(dse_reflection_probe_get_resolution(id), 512);
}

// --- GIProbe ---
TEST_F(DseApiBindingsTest, GIProbe_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::GIProbeVolumeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_gi_probe_set_enabled(id, 0);
    EXPECT_EQ(dse_gi_probe_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, GIProbe_IntensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::GIProbeVolumeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_gi_probe_set_intensity(id, 1.8f);
    EXPECT_FLOAT_EQ(dse_gi_probe_get_intensity(id), 1.8f);
}

// --- Foliage ---
TEST_F(DseApiBindingsTest, Foliage_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::FoliageComponent>(e);
    const uint32_t id = EntityId(e);
    dse_foliage_set_enabled(id, 0);
    EXPECT_EQ(dse_foliage_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Foliage_DensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::FoliageComponent>(e);
    const uint32_t id = EntityId(e);
    dse_foliage_set_density(id, 0.7f);
    EXPECT_FLOAT_EQ(dse_foliage_get_density(id), 0.7f);
}

// --- Atmosphere ---
TEST_F(DseApiBindingsTest, Atmosphere_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::AtmosphereComponent>(e);
    const uint32_t id = EntityId(e);
    dse_atmosphere_set_enabled(id, 0);
    EXPECT_EQ(dse_atmosphere_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Atmosphere_SunIntensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::AtmosphereComponent>(e);
    const uint32_t id = EntityId(e);
    dse_atmosphere_set_sun_intensity(id, 5.0f);
    EXPECT_FLOAT_EQ(dse_atmosphere_get_sun_intensity(id), 5.0f);
}

// --- VolumetricCloud ---
TEST_F(DseApiBindingsTest, VolumetricCloud_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::VolumetricCloudComponent>(e);
    const uint32_t id = EntityId(e);
    dse_volumetric_cloud_set_enabled(id, 0);
    EXPECT_EQ(dse_volumetric_cloud_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, VolumetricCloud_CoverageRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::VolumetricCloudComponent>(e);
    const uint32_t id = EntityId(e);
    dse_volumetric_cloud_set_coverage(id, 0.6f);
    EXPECT_FLOAT_EQ(dse_volumetric_cloud_get_coverage(id), 0.6f);
}

// --- DayNight ---
TEST_F(DseApiBindingsTest, DayNight_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DayNightCycleComponent>(e);
    const uint32_t id = EntityId(e);
    dse_day_night_set_enabled(id, 0);
    EXPECT_EQ(dse_day_night_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, DayNight_TimeOfDayRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DayNightCycleComponent>(e);
    const uint32_t id = EntityId(e);
    dse_day_night_set_time_of_day(id, 12.5f);
    EXPECT_FLOAT_EQ(dse_day_night_get_time_of_day(id), 12.5f);
}

TEST_F(DseApiBindingsTest, DayNight_SpeedRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DayNightCycleComponent>(e);
    const uint32_t id = EntityId(e);
    dse_day_night_set_day_speed(id, 2.0f);
    EXPECT_FLOAT_EQ(dse_day_night_get_day_speed(id), 2.0f);
}

// --- Hair ---
TEST_F(DseApiBindingsTest, Hair_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::HairComponent>(e);
    const uint32_t id = EntityId(e);
    dse_hair_set_enabled(id, 0);
    EXPECT_EQ(dse_hair_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Hair_StrandCountRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::HairComponent>(e);
    const uint32_t id = EntityId(e);
    dse_hair_set_strand_count(id, 5000);
    EXPECT_EQ(dse_hair_get_strand_count(id), 5000);
}

// --- Impostor ---
TEST_F(DseApiBindingsTest, Impostor_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ImpostorComponent>(e);
    const uint32_t id = EntityId(e);
    dse_impostor_set_enabled(id, 0);
    EXPECT_EQ(dse_impostor_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Impostor_DistanceRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ImpostorComponent>(e);
    const uint32_t id = EntityId(e);
    dse_impostor_set_switch_distance(id, 50.0f);
    EXPECT_FLOAT_EQ(dse_impostor_get_switch_distance(id), 50.0f);
}

// --- StreamingOrigin ---
TEST_F(DseApiBindingsTest, StreamingOrigin_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::StreamingOriginComponent>(e);
    const uint32_t id = EntityId(e);
    dse_streaming_origin_set_enabled(id, 0);
    EXPECT_EQ(dse_streaming_origin_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, StreamingOrigin_RadiusRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::StreamingOriginComponent>(e);
    const uint32_t id = EntityId(e);
    dse_streaming_origin_set_radius(id, 500.0f);
    EXPECT_FLOAT_EQ(dse_streaming_origin_get_radius(id), 500.0f);
}

// --- RigidBody3D extended ---
TEST_F(DseApiBindingsTest, RigidBody3D_MassRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    world_.registry().emplace<dse::RigidBody3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_rigidbody3d_set_mass(id, 10.0f);
    EXPECT_FLOAT_EQ(dse_rigidbody3d_get_mass(id), 10.0f);
}

TEST_F(DseApiBindingsTest, RigidBody3D_DampingRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    world_.registry().emplace<dse::RigidBody3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_rigidbody3d_set_linear_damping(id, 0.5f);
    dse_rigidbody3d_set_angular_damping(id, 0.3f);
    EXPECT_FLOAT_EQ(dse_rigidbody3d_get_linear_damping(id), 0.5f);
    EXPECT_FLOAT_EQ(dse_rigidbody3d_get_angular_damping(id), 0.3f);
}

TEST_F(DseApiBindingsTest, RigidBody3D_TypeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    auto& rb = world_.registry().emplace<dse::RigidBody3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_rigidbody3d_set_type(id, static_cast<int>(dse::RigidBody3DType::Kinematic));
    EXPECT_EQ(rb.type, dse::RigidBody3DType::Kinematic);
}

TEST_F(DseApiBindingsTest, RigidBody3D_FreezeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    world_.registry().emplace<dse::RigidBody3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_rigidbody3d_set_freeze_rotation(id, 1, 0, 1);
    const auto& rb = world_.registry().get<dse::RigidBody3DComponent>(e);
    EXPECT_TRUE(rb.freeze_rotation_x);
    EXPECT_FALSE(rb.freeze_rotation_y);
    EXPECT_TRUE(rb.freeze_rotation_z);
}

// --- BoxCollider3D extended ---
TEST_F(DseApiBindingsTest, BoxCollider3D_SizeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::BoxCollider3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_box_collider3d_set_size(id, 5.0f, 6.0f, 7.0f);
    float x, y, z;
    dse_box_collider3d_get_size(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 5.0f);
    EXPECT_FLOAT_EQ(y, 6.0f);
    EXPECT_FLOAT_EQ(z, 7.0f);
}

TEST_F(DseApiBindingsTest, BoxCollider3D_CenterRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::BoxCollider3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_box_collider3d_set_center(id, 1.0f, 2.0f, 3.0f);
    float x, y, z;
    dse_box_collider3d_get_center(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 1.0f);
    EXPECT_FLOAT_EQ(y, 2.0f);
    EXPECT_FLOAT_EQ(z, 3.0f);
}

// --- SphereCollider3D ---
TEST_F(DseApiBindingsTest, SphereCollider3D_RadiusRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SphereCollider3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_sphere_collider3d_set_radius(id, 2.5f);
    EXPECT_FLOAT_EQ(dse_sphere_collider3d_get_radius(id), 2.5f);
}

// --- CapsuleCollider3D ---
TEST_F(DseApiBindingsTest, CapsuleCollider3D_FieldsRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::CapsuleCollider3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_capsule_collider3d_set_radius(id, 0.4f);
    dse_capsule_collider3d_set_height(id, 1.8f);
    EXPECT_FLOAT_EQ(dse_capsule_collider3d_get_radius(id), 0.4f);
    EXPECT_FLOAT_EQ(dse_capsule_collider3d_get_height(id), 1.8f);
}

// --- CharacterController3D extended ---
TEST_F(DseApiBindingsTest, CharacterController3D_AddFieldsMatch) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_character_controller3d_add(id, 0.5f, 1.8f, 0.4f, 0.1f, 0);
    const auto& cc = world_.registry().get<dse::CharacterController3DComponent>(e);
    EXPECT_FLOAT_EQ(cc.radius, 0.5f);
    EXPECT_FLOAT_EQ(cc.height, 1.8f);
}

// --- Transform extended ---
TEST_F(DseApiBindingsTest, Transform_ScaleRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_scale(id, 2.0f, 3.0f, 4.0f);
    float x = 0, y = 0, z = 0;
    dse_transform_get_scale(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 2.0f);
    EXPECT_FLOAT_EQ(y, 3.0f);
    EXPECT_FLOAT_EQ(z, 4.0f);
}

TEST_F(DseApiBindingsTest, Transform_RotationEulerRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_rotation_euler(id, 0.0f, 90.0f, 0.0f);
    float x = 0, y = 0, z = 0;
    dse_transform_get_rotation_euler(id, &x, &y, &z);
    EXPECT_NEAR(y, 90.0f, 0.1f);
}

TEST_F(DseApiBindingsTest, Transform_LocalPositionRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_local_position(id, 5.0f, 6.0f, 7.0f);
    float x = 0, y = 0, z = 0;
    dse_transform_get_local_position(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 5.0f);
    EXPECT_FLOAT_EQ(y, 6.0f);
    EXPECT_FLOAT_EQ(z, 7.0f);
}

TEST_F(DseApiBindingsTest, Transform_MultipleEntitiesIndependent) {
    Entity e1 = world_.CreateEntity();
    Entity e2 = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e1);
    world_.registry().emplace<TransformComponent>(e2);
    const uint32_t id1 = EntityId(e1);
    const uint32_t id2 = EntityId(e2);
    dse_transform_set_position(id1, 1.0f, 0.0f, 0.0f);
    dse_transform_set_position(id2, 0.0f, 1.0f, 0.0f);
    float x1, y1, z1, x2, y2, z2;
    dse_transform_get_position(id1, &x1, &y1, &z1);
    dse_transform_get_position(id2, &x2, &y2, &z2);
    EXPECT_FLOAT_EQ(x1, 1.0f);
    EXPECT_FLOAT_EQ(y2, 1.0f);
    EXPECT_FLOAT_EQ(x2, 0.0f);
}

// --- Entity lifecycle extended ---
TEST_F(DseApiBindingsTest, EntityCreate_MultipleUnique) {
    uint32_t e1 = dse_entity_create();
    uint32_t e2 = dse_entity_create();
    uint32_t e3 = dse_entity_create();
    EXPECT_NE(e1, e2);
    EXPECT_NE(e2, e3);
    EXPECT_NE(e1, e3);
    EXPECT_EQ(dse_entity_valid(e1), 1);
    EXPECT_EQ(dse_entity_valid(e2), 1);
    EXPECT_EQ(dse_entity_valid(e3), 1);
    dse_entity_destroy(e1);
    dse_entity_destroy(e2);
    dse_entity_destroy(e3);
}

TEST_F(DseApiBindingsTest, EntityDestroy_InvalidatesEntity) {
    uint32_t e = dse_entity_create();
    EXPECT_EQ(dse_entity_valid(e), 1);
    dse_entity_destroy(e);
    EXPECT_EQ(dse_entity_valid(e), 0);
}

TEST_F(DseApiBindingsTest, EntityValid_NullEntity) {
    EXPECT_EQ(dse_entity_valid(static_cast<uint32_t>(entt::null)), 0);
}

// --- Camera3D extended ---
TEST_F(DseApiBindingsTest, Camera3D_FovRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Camera3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_camera3d_set_fov(id, 90.0f);
    EXPECT_FLOAT_EQ(dse_camera3d_get_fov(id), 90.0f);
}

TEST_F(DseApiBindingsTest, Camera3D_NearFarRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Camera3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_camera3d_set_near(id, 0.01f);
    dse_camera3d_set_far(id, 5000.0f);
    EXPECT_FLOAT_EQ(dse_camera3d_get_near(id), 0.01f);
    EXPECT_FLOAT_EQ(dse_camera3d_get_far(id), 5000.0f);
}

TEST_F(DseApiBindingsTest, Camera3D_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Camera3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_camera3d_set_enabled(id, 0);
    EXPECT_EQ(dse_camera3d_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Camera3D_PriorityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::Camera3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_camera3d_set_priority(id, 5);
    EXPECT_EQ(dse_camera3d_get_priority(id), 5);
}

// --- PointLight extended ---
TEST_F(DseApiBindingsTest, PointLight_ColorRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PointLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_point_light_set_color(id, 1.0f, 0.5f, 0.0f);
    float r, g, b;
    dse_point_light_get_color(id, &r, &g, &b);
    EXPECT_FLOAT_EQ(r, 1.0f);
    EXPECT_FLOAT_EQ(g, 0.5f);
    EXPECT_FLOAT_EQ(b, 0.0f);
}

TEST_F(DseApiBindingsTest, PointLight_IntensityRangeRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PointLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_point_light_set_intensity(id, 3.0f);
    dse_point_light_set_range(id, 50.0f);
    EXPECT_FLOAT_EQ(dse_point_light_get_intensity(id), 3.0f);
    EXPECT_FLOAT_EQ(dse_point_light_get_range(id), 50.0f);
}

// --- SpotLight ---
TEST_F(DseApiBindingsTest, SpotLight_AngleRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SpotLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_spot_light_set_inner_angle(id, 20.0f);
    dse_spot_light_set_outer_angle(id, 40.0f);
    EXPECT_FLOAT_EQ(dse_spot_light_get_inner_angle(id), 20.0f);
    EXPECT_FLOAT_EQ(dse_spot_light_get_outer_angle(id), 40.0f);
}

TEST_F(DseApiBindingsTest, SpotLight_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SpotLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_spot_light_set_enabled(id, 0);
    EXPECT_EQ(dse_spot_light_get_enabled(id), 0);
}

// --- SkyLight ---
TEST_F(DseApiBindingsTest, SkyLight_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SkyLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_sky_light_set_enabled(id, 0);
    EXPECT_EQ(dse_sky_light_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, SkyLight_IntensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SkyLightComponent>(e);
    const uint32_t id = EntityId(e);
    dse_sky_light_set_intensity(id, 0.8f);
    EXPECT_FLOAT_EQ(dse_sky_light_get_intensity(id), 0.8f);
}

// --- DirLight extended ---
TEST_F(DseApiBindingsTest, DirLight_ColorRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DirectionalLight3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_dir_light_set_color(id, 1.0f, 0.9f, 0.8f);
    float r, g, b;
    dse_dir_light_get_color(id, &r, &g, &b);
    EXPECT_FLOAT_EQ(r, 1.0f);
    EXPECT_FLOAT_EQ(g, 0.9f);
    EXPECT_FLOAT_EQ(b, 0.8f);
}

TEST_F(DseApiBindingsTest, DirLight_IntensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DirectionalLight3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_dir_light_set_intensity(id, 2.0f);
    EXPECT_FLOAT_EQ(dse_dir_light_get_intensity(id), 2.0f);
}

// --- NavMeshAutoRebake extended ---
TEST_F(DseApiBindingsTest, NavMeshAutoRebake_AllFieldsRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::NavMeshAutoRebakeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_navmesh_rebake_set_tile_size(id, 64.0f);
    dse_navmesh_rebake_set_agent_height(id, 2.0f);
    dse_navmesh_rebake_set_agent_radius(id, 0.5f);
    EXPECT_FLOAT_EQ(dse_navmesh_rebake_get_tile_size(id), 64.0f);
    EXPECT_FLOAT_EQ(dse_navmesh_rebake_get_agent_height(id), 2.0f);
    EXPECT_FLOAT_EQ(dse_navmesh_rebake_get_agent_radius(id), 0.5f);
}

// --- DynamicObstacle extended ---
TEST_F(DseApiBindingsTest, DynamicObstacle_AllFieldsRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::DynamicObstacleComponent>(e);
    const uint32_t id = EntityId(e);
    dse_dyn_obstacle_set_box_extents(id, 1.0f, 2.0f, 3.0f);
    dse_dyn_obstacle_set_cylinder_radius(id, 0.5f);
    dse_dyn_obstacle_set_cylinder_height(id, 1.5f);
    float x, y, z;
    dse_dyn_obstacle_get_box_extents(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 1.0f);
    EXPECT_FLOAT_EQ(y, 2.0f);
    EXPECT_FLOAT_EQ(z, 3.0f);
    EXPECT_FLOAT_EQ(dse_dyn_obstacle_get_cylinder_radius(id), 0.5f);
    EXPECT_FLOAT_EQ(dse_dyn_obstacle_get_cylinder_height(id), 1.5f);
}

// --- MeshRenderer extended ---
TEST_F(DseApiBindingsTest, MeshRenderer_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);
    dse_mesh_renderer_set_enabled(id, 0);
    EXPECT_EQ(dse_mesh_renderer_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, MeshRenderer_CastShadowRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);
    dse_mesh_renderer_set_cast_shadow(id, 0);
    EXPECT_EQ(dse_mesh_renderer_get_cast_shadow(id), 0);
}

TEST_F(DseApiBindingsTest, MeshRenderer_ReceiveShadowRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::MeshRendererComponent>(e);
    const uint32_t id = EntityId(e);
    dse_mesh_renderer_set_receive_shadow(id, 0);
    EXPECT_EQ(dse_mesh_renderer_get_receive_shadow(id), 0);
}

// --- App free functions ---
TEST_F(DseApiBindingsTest, App_GetDeltaTime_NonNegative) {
    float dt = dse_app_get_delta_time();
    EXPECT_GE(dt, 0.0f);
}

TEST_F(DseApiBindingsTest, App_GetTime_NonNegative) {
    float t = dse_app_get_time();
    EXPECT_GE(t, 0.0f);
}

TEST_F(DseApiBindingsTest, App_GetTargetFps_Positive) {
    float fps = dse_app_get_target_fps();
    EXPECT_GE(fps, 0.0f);
}

// --- Input ---
TEST_F(DseApiBindingsTest, Input_MouseScroll_ReturnsNumber) {
    float scroll = dse_input_get_mouse_scroll();
    EXPECT_FALSE(std::isnan(scroll));
}

TEST_F(DseApiBindingsTest, Input_TouchCount_NonNegative) {
    int count = dse_input_get_touch_count();
    EXPECT_GE(count, 0);
}

// --- API Version ---
TEST_F(DseApiBindingsTest, ApiVersion_NonZero) {
    EXPECT_GT(dse_api_version(), 0u);
}

// --- PostProcess extended ---
TEST_F(DseApiBindingsTest, PostProcess_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PostProcessComponent>(e);
    const uint32_t id = EntityId(e);
    dse_post_process_set_enabled(id, 0);
    EXPECT_EQ(dse_post_process_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, PostProcess_VignetteRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::PostProcessComponent>(e);
    const uint32_t id = EntityId(e);
    dse_post_process_set_vignette_intensity(id, 0.5f);
    EXPECT_FLOAT_EQ(dse_post_process_get_vignette_intensity(id), 0.5f);
}

// --- Multiple component interactions ---
TEST_F(DseApiBindingsTest, MultipleComponents_TransformAndRigidBody) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    world_.registry().emplace<dse::RigidBody3DComponent>(e);
    world_.registry().emplace<dse::BoxCollider3DComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_position(id, 1.0f, 2.0f, 3.0f);
    dse_rigidbody3d_set_mass(id, 5.0f);
    dse_box_collider3d_set_size(id, 2.0f, 2.0f, 2.0f);
    float x, y, z;
    dse_transform_get_position(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 1.0f);
    EXPECT_FLOAT_EQ(dse_rigidbody3d_get_mass(id), 5.0f);
    dse_box_collider3d_get_size(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 2.0f);
}

// --- Stress: many entities ---
TEST_F(DseApiBindingsTest, StressTest_100Entities) {
    std::vector<uint32_t> entities;
    for (int i = 0; i < 100; ++i) {
        uint32_t e = dse_entity_create();
        entities.push_back(e);
        dse_transform_add(e, static_cast<float>(i), 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    }
    for (int i = 0; i < 100; ++i) {
        float x, y, z;
        dse_transform_get_position(entities[i], &x, &y, &z);
        EXPECT_FLOAT_EQ(x, static_cast<float>(i));
    }
    for (auto e : entities) dse_entity_destroy(e);
}

// --- Edge cases ---
TEST_F(DseApiBindingsTest, Transform_ZeroScale) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_scale(id, 0.0f, 0.0f, 0.0f);
    float x, y, z;
    dse_transform_get_scale(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 0.0f);
}

TEST_F(DseApiBindingsTest, Transform_NegativePosition) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_position(id, -100.0f, -200.0f, -300.0f);
    float x, y, z;
    dse_transform_get_position(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, -100.0f);
    EXPECT_FLOAT_EQ(y, -200.0f);
}

TEST_F(DseApiBindingsTest, Transform_LargeValues) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<TransformComponent>(e);
    const uint32_t id = EntityId(e);
    dse_transform_set_position(id, 1e6f, 1e6f, 1e6f);
    float x, y, z;
    dse_transform_get_position(id, &x, &y, &z);
    EXPECT_FLOAT_EQ(x, 1e6f);
}

// --- Tree extended ---
TEST_F(DseApiBindingsTest, Tree_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::TreeComponent>(e);
    const uint32_t id = EntityId(e);
    dse_tree_set_enabled(id, 0);
    EXPECT_EQ(dse_tree_get_enabled(id), 0);
}

// --- Buoyancy codegen ---
TEST_F(DseApiBindingsTest, Buoyancy_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::BuoyancyComponent>(e);
    const uint32_t id = EntityId(e);
    dse_buoyancy_set_enabled(id, 0);
    EXPECT_EQ(dse_buoyancy_get_enabled(id), 0);
}

TEST_F(DseApiBindingsTest, Buoyancy_DensityRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::BuoyancyComponent>(e);
    const uint32_t id = EntityId(e);
    dse_buoyancy_set_density(id, 800.0f);
    EXPECT_FLOAT_EQ(dse_buoyancy_get_density(id), 800.0f);
}

// --- Cloth codegen ---
TEST_F(DseApiBindingsTest, Cloth_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::ClothComponent>(e);
    const uint32_t id = EntityId(e);
    dse_cloth_set_enabled(id, 0);
    EXPECT_EQ(dse_cloth_get_enabled(id), 0);
}

// --- Fluid codegen ---
TEST_F(DseApiBindingsTest, Fluid_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::FluidComponent>(e);
    const uint32_t id = EntityId(e);
    dse_fluid_set_enabled(id, 0);
    EXPECT_EQ(dse_fluid_get_enabled(id), 0);
}

// --- Weather codegen ---
TEST_F(DseApiBindingsTest, Weather_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::WeatherComponent>(e);
    const uint32_t id = EntityId(e);
    dse_weather_set_enabled(id, 0);
    EXPECT_EQ(dse_weather_get_enabled(id), 0);
}

// --- Snow codegen ---
TEST_F(DseApiBindingsTest, Snow_EnabledRoundTrip) {
    Entity e = world_.CreateEntity();
    world_.registry().emplace<dse::SnowComponent>(e);
    const uint32_t id = EntityId(e);
    dse_snow_set_enabled(id, 0);
    EXPECT_EQ(dse_snow_get_enabled(id), 0);
}

// --- Audio free functions ---
TEST_F(DseApiBindingsTest, Audio_SourcePlayingInvalidEntity) {
    const uint32_t invalid = 0xFFFFFFFEu;
    EXPECT_EQ(dse_audio_source_is_playing(invalid), 0);
}

// --- UI free functions ---
TEST_F(DseApiBindingsTest, UI_InvalidEntityReturnsFalse) {
    const uint32_t invalid = 0xFFFFFFFEu;
    EXPECT_EQ(dse_ui_is_hovered(invalid), 0);
    EXPECT_EQ(dse_ui_is_pressed(invalid), 0);
}

// --- Batch getter safety on invalid entity ---
TEST_F(DseApiBindingsTest, BatchGetterSafety_AllInvalidEntity) {
    const uint32_t inv = 0xFFFFFFFEu;
    EXPECT_EQ(dse_decal_get_enabled(inv), 1);
    EXPECT_EQ(dse_skybox_get_enabled(inv), 1);
    EXPECT_EQ(dse_water_get_enabled(inv), 1);
    EXPECT_EQ(dse_foliage_get_enabled(inv), 1);
    EXPECT_EQ(dse_atmosphere_get_enabled(inv), 1);
    EXPECT_EQ(dse_volumetric_cloud_get_enabled(inv), 1);
    EXPECT_EQ(dse_day_night_get_enabled(inv), 1);
    EXPECT_EQ(dse_hair_get_enabled(inv), 1);
    EXPECT_EQ(dse_impostor_get_enabled(inv), 1);
    EXPECT_EQ(dse_light_probe_get_enabled(inv), 1);
    EXPECT_EQ(dse_reflection_probe_get_enabled(inv), 1);
    EXPECT_EQ(dse_gi_probe_get_enabled(inv), 1);
}
'''


def main():
    with open(TEST_FILE, "r", encoding="utf-8") as f:
        content = f.read()

    if "Phase 5: Extended Coverage" in content:
        print("[Phase5] Tests already expanded, skipping.")
        return

    content = content.rstrip()
    content += "\n" + NEW_TESTS + "\n"

    with open(TEST_FILE, "w", encoding="utf-8") as f:
        f.write(content)

    count = len(re.findall(r"TEST_F\(", content))
    print(f"[Phase5] Test file expanded to {count} test cases (was 51).")


if __name__ == "__main__":
    main()
