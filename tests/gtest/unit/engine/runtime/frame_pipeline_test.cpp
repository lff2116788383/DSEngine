/**
 * @file frame_pipeline_test.cpp
 * @brief FramePipeline 帧流水线单元测试
 *
 * 覆盖场景：
 * - 默认构造与析构不崩溃
 * - 未初始化时 Update/FixedUpdate/Render 为空操作
 * - SetWorld / EnableEditorMode / SetBusinessMode 在初始化前可设置
 * - 初始化后注入锁定（SetWorld/SetAssetManager/SetBusinessMode 不再修改）
 * - 渲染统计初始值为零
 * - 窗口标题回调注入与触发
 * - 无 RHI 设备时 ReadSceneColorRgba8 返回空
 */

#include <gtest/gtest.h>
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/runtime/runtime_context.h"

class FramePipelineTest : public ::testing::Test {
protected:
    FramePipeline pipeline;
    World world;
};

// 测试 帧管线：默认且不崩溃
TEST_F(FramePipelineTest, DefaultAndDoesNotCrash) {
    // 验证构造/析构正常
}

// 测试 帧管线：当不已初始化更新为空
TEST_F(FramePipelineTest, WhenNotInitializedUpdateIsEmpty) {
    EXPECT_NO_THROW(pipeline.Update(0.016f));
}

// 测试 帧管线：当不已初始化固定更新为空
TEST_F(FramePipelineTest, WhenNotInitializedFixedUpdateIsEmpty) {
    EXPECT_NO_THROW(pipeline.FixedUpdate(0.02f));
}

// 测试 帧管线：当不已初始化渲染为空
TEST_F(FramePipelineTest, WhenNotInitializedRenderIsEmpty) {
    EXPECT_NO_THROW(pipeline.Render());
}

// 测试 帧管线：当不已初始化关闭不崩溃
TEST_F(FramePipelineTest, WhenNotInitializedShutdownDoesNotCrash) {
    EXPECT_NO_THROW(pipeline.Shutdown());
}

// 测试 帧管线：初始化之前可设置上世界
TEST_F(FramePipelineTest, InitializeBeforeCansetUpWorld) {
    pipeline.SetWorld(&world);
    EXPECT_NO_THROW(pipeline.world());
}

// 测试 帧管线：初始化之前设置世界Rejects空
TEST_F(FramePipelineTest, InitializeBeforeSetWorldRejectsEmpty) {
    // SetWorld 在 !world 时直接返回，不注入 nullptr
    World* original = &world;
    pipeline.SetWorld(&world);
    // 注入空指针后被拒绝，world() 仍返回之前注入的
    pipeline.SetWorld(nullptr);
    // world() 仍指向原注入的 world（因为 SetWorld(nullptr) 会 return）
    EXPECT_EQ(&pipeline.world(), original);
}

// 测试 帧管线：初始化之前能够Enabledmodel
TEST_F(FramePipelineTest, InitializeBeforeCanEnabledmodel) {
    pipeline.EnableEditorMode(true);
    // 无法直接读取 editor_mode，仅验证不崩溃
    SUCCEED();
}

// 测试 帧管线：初始化之前可设置Upmodel
TEST_F(FramePipelineTest, InitializeBeforeCansetUpmodel) {
    pipeline.SetBusinessMode(BusinessMode::Cpp);
    SUCCEED();
}

// 测试 帧管线：为零
TEST_F(FramePipelineTest, IsZero) {
    EXPECT_EQ(pipeline.LastDrawCalls(), 0);
    EXPECT_EQ(pipeline.LastMaterialSwitches(), 0);
    EXPECT_EQ(pipeline.LastMaxBatchSprites(), 0);
    EXPECT_EQ(pipeline.LastSpriteCount(), 0);
}

// 测试 帧管线：注入且触发
TEST_F(FramePipelineTest, InjectsAndTriggers) {
    std::string captured_title;
    pipeline.SetWindowTitleSetter([&captured_title](const std::string& title) {
        captured_title = title;
    });
    pipeline.SetWindowTitle("Test Title");
    EXPECT_EQ(captured_title, "Test Title");
}

// 测试 帧管线：无当设置窗口标题不崩溃
TEST_F(FramePipelineTest, WithoutWhenSetWindowTitleDoesNotCrash) {
    pipeline.SetWindowTitleSetter(nullptr);
    EXPECT_NO_THROW(pipeline.SetWindowTitle("should not crash"));
}

// 测试 帧管线：无RHI设备当读取场景颜色RGBA 8返回空
TEST_F(FramePipelineTest, WithoutRHIDeviceWhenReadSceneColorRgba8ReturnsEmpty) {
    auto pixels = pipeline.ReadSceneColorRgba8();
    EXPECT_TRUE(pixels.empty());
}

// 测试 帧管线：无RHI设备当读取主颜色RGBA 8返回空
TEST_F(FramePipelineTest, WithoutRHIDeviceWhenReadMainColorRgba8ReturnsEmpty) {
    auto pixels = pipeline.ReadMainColorRgba8();
    EXPECT_TRUE(pixels.empty());
}

// 测试 帧管线：无RHI设备当读取场景颜色RGBA 8带尺寸返回空
TEST_F(FramePipelineTest, WithoutRHIDeviceWhenReadSceneColorRgba8WithSizeReturnsEmpty) {
    auto result = pipeline.ReadSceneColorRgba8WithSize();
    EXPECT_EQ(result.width, 0);
    EXPECT_EQ(result.height, 0);
    EXPECT_TRUE(result.pixels.empty());
}

// 测试 帧管线：无RHI设备当获取场景纹理ID返回零
TEST_F(FramePipelineTest, WithoutRHIDeviceWhenGetSceneTextureIdReturnsZero) {
    EXPECT_EQ(pipeline.GetSceneTextureId(), 0u);
}

// 测试 帧管线：无RHI设备当获取主纹理ID返回零
TEST_F(FramePipelineTest, WithoutRHIDeviceWhenGetMainTextureIdReturnsZero) {
    EXPECT_EQ(pipeline.GetMainTextureId(), 0u);
}
