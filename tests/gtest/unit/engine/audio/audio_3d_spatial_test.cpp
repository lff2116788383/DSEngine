/**
 * @file audio_3d_spatial_test.cpp
 * @brief 音频 3D 空间化单元测试（Phase 1/2/3）
 *
 * 覆盖场景：
 * - Phase 1: AudioListenerComponent 默认值、字段修改、与 Camera3DComponent 共存
 * - Phase 2: AudioAttenuationModel 枚举值、AudioSourceComponent 衰减字段
 * - Phase 3: AudioSourceComponent 遮挡字段默认值、AudioSystem SetRaycastFunction
 *
 * 注意：AudioSystem::Update 需要 miniaudio 后端，
 *       本测试仅覆盖组件数据层和无需后端初始化的 API。
 */

#include <gtest/gtest.h>
#include "engine/ecs/audio.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/audio/audio_system.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

using namespace dse::gameplay2d;

// ============================================================
// Phase 1: AudioListenerComponent
// ============================================================

// 测试 音频监听器组件：默认值
TEST(AudioListenerComponentTest, DefaultValues) {
    AudioListenerComponent listener;
    EXPECT_TRUE(listener.enabled);
    EXPECT_EQ(listener.listener_index, 0u);
}

// 测试 音频监听器组件：字段修改
TEST(AudioListenerComponentTest, FieldModification) {
    AudioListenerComponent listener;
    listener.enabled = false;
    listener.listener_index = 2;
    EXPECT_FALSE(listener.enabled);
    EXPECT_EQ(listener.listener_index, 2u);
}

// 测试 音频监听器组件：挂载到ECS实体
TEST(AudioListenerComponentTest, MountToECSEntity) {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<AudioListenerComponent>(entity);
    auto& tc = registry.emplace<TransformComponent>(entity);
    tc.position = glm::vec3(1.0f, 2.0f, 3.0f);

    bool has_both = registry.all_of<AudioListenerComponent, TransformComponent>(entity);
    EXPECT_TRUE(has_both);
    const auto& listener = registry.get<AudioListenerComponent>(entity);
    const auto& transform = registry.get<TransformComponent>(entity);
    EXPECT_TRUE(listener.enabled);
    EXPECT_FLOAT_EQ(transform.position.x, 1.0f);
    EXPECT_FLOAT_EQ(transform.position.y, 2.0f);
    EXPECT_FLOAT_EQ(transform.position.z, 3.0f);
}

// 测试 音频监听器组件：朝向
TEST(AudioListenerComponentTest, Toward) {
    TransformComponent transform;
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity
    glm::vec3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);

    EXPECT_NEAR(forward.x, 0.0f, 1e-5f);
    EXPECT_NEAR(forward.y, 0.0f, 1e-5f);
    EXPECT_NEAR(forward.z, -1.0f, 1e-5f);
    EXPECT_NEAR(up.x, 0.0f, 1e-5f);
    EXPECT_NEAR(up.y, 1.0f, 1e-5f);
    EXPECT_NEAR(up.z, 0.0f, 1e-5f);
}

// 测试 音频监听器组件：情形90 Y朝向正确
TEST(AudioListenerComponentTest, Case90YTowardCorrect) {
    TransformComponent transform;
    // 绕 Y 轴旋转 90 度
    transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);

    // 绕 Y 轴旋转 90 度后，-Z 方向变为 -X 方向
    EXPECT_NEAR(forward.x, -1.0f, 1e-4f);
    EXPECT_NEAR(forward.y, 0.0f, 1e-4f);
    EXPECT_NEAR(forward.z, 0.0f, 1e-4f);
    EXPECT_NEAR(up.y, 1.0f, 1e-4f);
}

// 测试 音频监听器组件：相机3D Coexistence且Compatibility
TEST(AudioListenerComponentTest, Camera3DCoexistenceAndCompatibility) {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<AudioListenerComponent>(entity);
    registry.emplace<dse::Camera3DComponent>(entity);
    registry.emplace<TransformComponent>(entity);

    bool has_all = registry.all_of<AudioListenerComponent, dse::Camera3DComponent, TransformComponent>(entity);
    EXPECT_TRUE(has_all);
}

// 测试 音频监听器组件：相机3D回退Search
TEST(AudioListenerComponentTest, Camera3DFallbackSearch) {
    entt::registry registry;
    auto cam_entity = registry.create();
    registry.emplace<dse::Camera3DComponent>(cam_entity);
    auto& tc = registry.emplace<TransformComponent>(cam_entity);
    tc.position = glm::vec3(10.0f, 20.0f, 30.0f);

    auto listener_view = registry.view<AudioListenerComponent>();
    EXPECT_TRUE(listener_view.begin() == listener_view.end());

    auto camera_view = registry.view<dse::Camera3DComponent, TransformComponent>();
    bool found_camera = false;
    for (auto e : camera_view) {
        const auto& cam = camera_view.get<dse::Camera3DComponent>(e);
        if (cam.enabled) {
            const auto& t = camera_view.get<TransformComponent>(e);
            EXPECT_FLOAT_EQ(t.position.x, 10.0f);
            found_camera = true;
            break;
        }
    }
    EXPECT_TRUE(found_camera);
}

// ============================================================
// Phase 2: AudioAttenuationModel
// ============================================================

// 测试 音频衰减模型：枚举值
TEST(AudioAttenuationModelTest, EnumerationValue) {
    EXPECT_EQ(static_cast<int>(AudioAttenuationModel::Inverse), 0);
    EXPECT_EQ(static_cast<int>(AudioAttenuationModel::Linear), 1);
    EXPECT_EQ(static_cast<int>(AudioAttenuationModel::Exponential), 2);
}

// 测试 音频衰减模型：默认衰减为Inverse
TEST(AudioAttenuationModelTest, DefaultDecaysIsInverse) {
    AudioSourceComponent audio;
    EXPECT_EQ(audio.attenuation_model, AudioAttenuationModel::Inverse);
}

// 测试 音频衰减模型：可设置上为线性
TEST(AudioAttenuationModelTest, CansetUpIsLinear) {
    AudioSourceComponent audio;
    audio.attenuation_model = AudioAttenuationModel::Linear;
    EXPECT_EQ(audio.attenuation_model, AudioAttenuationModel::Linear);
}

// 测试 音频衰减模型：可设置上为Exponential
TEST(AudioAttenuationModelTest, CansetUpIsExponential) {
    AudioSourceComponent audio;
    audio.attenuation_model = AudioAttenuationModel::Exponential;
    EXPECT_EQ(audio.attenuation_model, AudioAttenuationModel::Exponential);
}

// 测试 音频衰减模型：空Parameterscombination
TEST(AudioAttenuationModelTest, EmptyParameterscombination) {
    AudioSourceComponent audio;
    audio.spatial_enabled = true;
    audio.min_distance = 2.0f;
    audio.max_distance = 50.0f;
    audio.rolloff = 1.5f;
    audio.attenuation_model = AudioAttenuationModel::Exponential;

    EXPECT_TRUE(audio.spatial_enabled);
    EXPECT_FLOAT_EQ(audio.min_distance, 2.0f);
    EXPECT_FLOAT_EQ(audio.max_distance, 50.0f);
    EXPECT_FLOAT_EQ(audio.rolloff, 1.5f);
    EXPECT_EQ(audio.attenuation_model, AudioAttenuationModel::Exponential);
}

// ============================================================
// Phase 3: Occlusion
// ============================================================

// 测试 音频遮挡：默认关闭
TEST(AudioOcclusionTest, DefaultShutdown) {
    AudioSourceComponent audio;
    EXPECT_FALSE(audio.occlusion_enabled);
    EXPECT_FLOAT_EQ(audio.occlusion_factor, 0.2f);
}

// 测试 音频遮挡：能够修订
TEST(AudioOcclusionTest, CanRevise) {
    AudioSourceComponent audio;
    audio.occlusion_enabled = true;
    audio.occlusion_factor = 0.5f;
    EXPECT_TRUE(audio.occlusion_enabled);
    EXPECT_FLOAT_EQ(audio.occlusion_factor, 0.5f);
}

// 测试 音频遮挡：Because
TEST(AudioOcclusionTest, Because) {
    AudioSourceComponent audio;
    audio.occlusion_factor = 0.0f;
    EXPECT_FLOAT_EQ(audio.occlusion_factor, 0.0f);
    audio.occlusion_factor = 1.0f;
    EXPECT_FLOAT_EQ(audio.occlusion_factor, 1.0f);
}

// 测试 音频遮挡：设置射线检测函数空回调不崩溃
TEST(AudioOcclusionTest, SetRaycastFunctionEmptyCallbackDoesNotCrash) {
    AudioSystem audio;
    audio.SetRaycastFunction(nullptr);
    // 空回调表示禁用遮挡检测，不应崩溃
}

// 测试 音频遮挡：设置射线检测函数设置回调无崩溃
TEST(AudioOcclusionTest, SetRaycastFunctionSetCallbackWithoutCrashing) {
    AudioSystem audio;
    audio.SetRaycastFunction([](const glm::vec3&, const glm::vec3&, float) {
        AudioRaycastResult r;
        r.hit = false;
        r.distance = 0.0f;
        return r;
    });
}

// 测试 音频遮挡：空启用
TEST(AudioOcclusionTest, EmptyEnabled) {
    AudioSourceComponent audio;
    audio.spatial_enabled = false;
    audio.occlusion_enabled = true;
    // 遮挡逻辑在 spatial_enabled=false 时不应生效
    // 此处仅验证字段组合不冲突
    EXPECT_FALSE(audio.spatial_enabled);
    EXPECT_TRUE(audio.occlusion_enabled);
}

// 测试 音频遮挡：情形3D配置
TEST(AudioOcclusionTest, Case3DConfiguration) {
    AudioSourceComponent audio;
    audio.spatial_enabled = true;
    audio.min_distance = 1.0f;
    audio.max_distance = 100.0f;
    audio.rolloff = 2.0f;
    audio.attenuation_model = AudioAttenuationModel::Linear;
    audio.occlusion_enabled = true;
    audio.occlusion_factor = 0.3f;
    audio.volume = 0.8f;

    EXPECT_TRUE(audio.spatial_enabled);
    EXPECT_TRUE(audio.occlusion_enabled);
    EXPECT_EQ(audio.attenuation_model, AudioAttenuationModel::Linear);
    EXPECT_FLOAT_EQ(audio.occlusion_factor, 0.3f);
    EXPECT_FLOAT_EQ(audio.volume, 0.8f);
}

// ============================================================
// AudioSystem 无后端测试
// ============================================================

// 测试 音频系统3D：当不已初始化更新不崩溃
TEST(AudioSystem3DTest, WhenNotInitializedUpdateDoesNotCrash) {
    AudioSystem audio;
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<AudioListenerComponent>(entity);
    registry.emplace<TransformComponent>(entity);
    audio.Update(registry, 0.016f);
}

// 测试 音频系统3D：当不已初始化设置射线检测函数不崩溃
TEST(AudioSystem3DTest, WhenNotInitializedSetRaycastFunctionDoesNotCrash) {
    AudioSystem audio;
    audio.SetRaycastFunction(nullptr);
    audio.Shutdown();
}
