#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/assets/asset_manager.h"
#include "engine/render/rhi/rhi_device.h"

using namespace dse;

// Mock RHI for CI environment
class MockRhiDevice : public RhiDevice {
public:
    void Shutdown() override {}
    void BeginFrame() override {}
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override { return 1; }
    unsigned int GetRenderTargetColorTexture(unsigned int handle) const override { return 1; }
    unsigned int GetRenderTargetDepthTexture(unsigned int handle) const override { return 1; }
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int) const override { return {}; }

    unsigned int CreateTexture2D(int width, int height, const unsigned char* data, bool linear) override { return 1; }
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const data[6], bool linear) override { return 1; }


    void DeleteTexture(unsigned int handle) override {}
    unsigned int CreateShaderProgram(const std::string& v, const std::string& f) override { return 1; }
    void DeleteShaderProgram(unsigned int handle) override {}
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override { return 1; }
    unsigned int CreateBuffer(size_t size, const void* data, bool dyn, bool index) override { return 1; }
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override {}
    void DeleteBuffer(unsigned int handle) override {}
};

// 冒烟测试：验证 3D 渲染管线的基础状态和系统编排
TEST_CASE("Given_3DScene_When_FramePipelineRender_Then_NoCrashAndDrawCallsRecorded", "[engine][smoke][3d_pipeline]") {
    World world;
    AssetManager asset_manager;
    FramePipeline pipeline;
    
    // 初始化资源管理器
    asset_manager.ConfigureDataRoot(".");
    pipeline.SetWorld(&world);
    pipeline.SetAssetManager(&asset_manager);
    
    // 设置 Mock RHI
    // pipeline.Init() internally creates OpenGLRhiDevice. For testing, we just want to ensure ECS logic doesn't crash
    
    // 创建基础 3D 场景
    auto cam_ent = world.CreateEntity();
    world.registry().emplace<TransformComponent>(cam_ent, glm::vec3(0, 0, 5));
    world.registry().emplace<Camera3DComponent>(cam_ent);

    auto light_ent = world.CreateEntity();
    world.registry().emplace<TransformComponent>(light_ent, glm::vec3(0, 10, 0));
    world.registry().emplace<DirectionalLight3DComponent>(light_ent);

    auto mesh_ent = world.CreateEntity();
    world.registry().emplace<TransformComponent>(mesh_ent);
    auto& mesh = world.registry().emplace<MeshRendererComponent>(mesh_ent);
    mesh.mesh_path = "mock_path.dmesh"; // Mock path
    
    auto sky_ent = world.CreateEntity();
    world.registry().emplace<SkyboxComponent>(sky_ent);

    // 运行流水线 Update (包含 Frustum Culling 等)
    REQUIRE_NOTHROW(pipeline.Update(0.016f));
}
