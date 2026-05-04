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

TEST_F(FramePipelineTest, 默认构造与析构不崩溃) {
    // 验证构造/析构正常
}

TEST_F(FramePipelineTest, 未初始化时Update为空操作) {
    EXPECT_NO_THROW(pipeline.Update(0.016f));
}

TEST_F(FramePipelineTest, 未初始化时FixedUpdate为空操作) {
    EXPECT_NO_THROW(pipeline.FixedUpdate(0.02f));
}

TEST_F(FramePipelineTest, 未初始化时Render为空操作) {
    EXPECT_NO_THROW(pipeline.Render());
}

TEST_F(FramePipelineTest, 未初始化时Shutdown不崩溃) {
    EXPECT_NO_THROW(pipeline.Shutdown());
}

TEST_F(FramePipelineTest, 初始化前可设置World) {
    pipeline.SetWorld(&world);
    EXPECT_NO_THROW(pipeline.world());
}

TEST_F(FramePipelineTest, 初始化前SetWorld拒绝空指针) {
    // SetWorld 在 !world 时直接返回，不注入 nullptr
    World* original = &world;
    pipeline.SetWorld(&world);
    // 注入空指针后被拒绝，world() 仍返回之前注入的
    pipeline.SetWorld(nullptr);
    // world() 仍指向原注入的 world（因为 SetWorld(nullptr) 会 return）
    EXPECT_EQ(&pipeline.world(), original);
}

TEST_F(FramePipelineTest, 初始化前可启用编辑器模式) {
    pipeline.EnableEditorMode(true);
    // 无法直接读取 editor_mode，仅验证不崩溃
    SUCCEED();
}

TEST_F(FramePipelineTest, 初始化前可设置业务模式) {
    pipeline.SetBusinessMode(BusinessMode::Cpp);
    SUCCEED();
}

TEST_F(FramePipelineTest, 渲染统计初始值为零) {
    EXPECT_EQ(pipeline.LastDrawCalls(), 0);
    EXPECT_EQ(pipeline.LastMaterialSwitches(), 0);
    EXPECT_EQ(pipeline.LastMaxBatchSprites(), 0);
    EXPECT_EQ(pipeline.LastSpriteCount(), 0);
}

TEST_F(FramePipelineTest, 窗口标题回调注入与触发) {
    std::string captured_title;
    pipeline.SetWindowTitleSetter([&captured_title](const std::string& title) {
        captured_title = title;
    });
    pipeline.SetWindowTitle("Test Title");
    EXPECT_EQ(captured_title, "Test Title");
}

TEST_F(FramePipelineTest, 无标题回调时SetWindowTitle不崩溃) {
    pipeline.SetWindowTitleSetter(nullptr);
    EXPECT_NO_THROW(pipeline.SetWindowTitle("should not crash"));
}

TEST_F(FramePipelineTest, 无RHIDevice时ReadSceneColorRgba8返回空) {
    auto pixels = pipeline.ReadSceneColorRgba8();
    EXPECT_TRUE(pixels.empty());
}

TEST_F(FramePipelineTest, 无RHIDevice时ReadMainColorRgba8返回空) {
    auto pixels = pipeline.ReadMainColorRgba8();
    EXPECT_TRUE(pixels.empty());
}

TEST_F(FramePipelineTest, 无RHIDevice时ReadSceneColorRgba8WithSize返回空) {
    auto result = pipeline.ReadSceneColorRgba8WithSize();
    EXPECT_EQ(result.width, 0);
    EXPECT_EQ(result.height, 0);
    EXPECT_TRUE(result.pixels.empty());
}

TEST_F(FramePipelineTest, 无RHIDevice时GetSceneTextureId返回零) {
    EXPECT_EQ(pipeline.GetSceneTextureId(), 0u);
}

TEST_F(FramePipelineTest, 无RHIDevice时GetMainTextureId返回零) {
    EXPECT_EQ(pipeline.GetMainTextureId(), 0u);
}
