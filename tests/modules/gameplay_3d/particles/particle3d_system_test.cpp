#include "catch/catch.hpp"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "modules/gameplay_3d/particles/particle3d_system.h"

using namespace dse;
using namespace dse::gameplay3d;

namespace {

class MockRhiDevice final : public RhiDevice {
public:
    void Shutdown() override {}
    void BeginFrame() override {}
    unsigned int CreateRenderTarget(const RenderTargetDesc&) override { return 1; }
    unsigned int GetRenderTargetColorTexture(unsigned int) const override { return 1; }
    unsigned int GetRenderTargetDepthTexture(unsigned int) const override { return 1; }
    unsigned int GetRenderTargetDepthTextureFace(unsigned int, unsigned int) const override { return 1; }
    unsigned int CreateTexture2D(int, int, const unsigned char*, bool) override { return 1; }
    unsigned int CreateTextureCube(int, int, const unsigned char* const[6], bool) override { return 1; }
    void DeleteTexture(unsigned int) override {}
    unsigned int CreateShaderProgram(const std::string&, const std::string&) override { return 1; }
    void DeleteShaderProgram(unsigned int) override {}
    unsigned int CreatePipelineState(const PipelineStateDesc&) override { return 1; }
    unsigned int CreateBuffer(size_t, const void*, bool, bool) override {
        create_buffer_calls++;
        return next_buffer_handle;
    }
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override {
        update_buffer_calls++;
        last_update_handle = handle;
        last_update_offset = offset;
        last_update_size = size;
        last_update_is_index = is_index;
        last_update_data = data;
    }
    void DeleteBuffer(unsigned int handle) override {
        delete_buffer_calls++;
        last_deleted_buffer = handle;
    }
    unsigned int CreateVertexArray() override { return 1; }
    void DeleteVertexArray(unsigned int) override {}
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return nullptr; }
    void Submit(std::shared_ptr<CommandBuffer>) override {}
    void EndFrame() override {}
    const RenderStats& LastFrameStats() const override { return stats; }

    unsigned int next_buffer_handle = 77;
    int create_buffer_calls = 0;
    int update_buffer_calls = 0;
    int delete_buffer_calls = 0;
    unsigned int last_update_handle = 0;
    size_t last_update_offset = 0;
    size_t last_update_size = 0;
    bool last_update_is_index = false;
    const void* last_update_data = nullptr;
    unsigned int last_deleted_buffer = 0;
    RenderStats stats{};
};

} // namespace

TEST_CASE("Given_DefaultParticleSystem3DComponent_When_Created_Then_DefaultRuntimeStateIsValid", "[engine][unit][particle3d]") {
    ParticleSystem3DComponent ps;

    REQUIRE(ps.enabled == true);
    REQUIRE(ps.max_particles == 1000);
    REQUIRE(ps.emission_rate == Approx(100.0f));
    REQUIRE(ps.texture_handle == 0);
    REQUIRE(ps.instance_vbo == 0);
    REQUIRE(ps.active_particle_count == 0);
    REQUIRE(ps.initialized == false);
}

TEST_CASE("Given_DisabledParticle3DSystem_When_Updated_Then_GpuBufferIsInitializedButNoParticlesAreEmitted", "[engine][unit][particle3d]") {
    World world;
    MockRhiDevice rhi;
    AssetManager asset_manager;
    Particle3DSystem system;
    system.Init(world, &rhi);
    system.SetAssetManager(&asset_manager);

    const auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    auto& ps = world.registry().emplace<ParticleSystem3DComponent>(entity);
    ps.enabled = false;
    ps.max_particles = 8;
    ps.emission_rate = 10.0f;

    system.Update(world, 0.5f);

    REQUIRE(ps.initialized == true);
    REQUIRE(ps.instance_vbo == 77);
    REQUIRE(ps.particles.size() == 8);
    REQUIRE(ps.active_particle_count == 0);
    REQUIRE(rhi.create_buffer_calls == 1);
    REQUIRE(rhi.update_buffer_calls == 0);
}

TEST_CASE("Given_EnabledParticle3DSystem_When_Updated_Then_EmitterCreatesParticlesAndUploadsInstanceData", "[engine][unit][particle3d]") {
    World world;
    MockRhiDevice rhi;
    AssetManager asset_manager;
    Particle3DSystem system;
    system.Init(world, &rhi);
    system.SetAssetManager(&asset_manager);

    const auto entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(3.0f, 4.0f, 5.0f);
    auto& ps = world.registry().emplace<ParticleSystem3DComponent>(entity);
    ps.enabled = true;
    ps.max_particles = 6;
    ps.emission_rate = 4.0f;
    ps.start_life_min = 2.0f;
    ps.start_life_max = 2.0f;
    ps.start_size_min = 0.25f;
    ps.start_size_max = 0.25f;
    ps.start_speed_min = 1.0f;
    ps.start_speed_max = 1.0f;
    ps.gravity = glm::vec3(0.0f);

    system.Update(world, 0.5f);

    REQUIRE(ps.initialized == true);
    REQUIRE(ps.instance_vbo == 77);
    REQUIRE(rhi.create_buffer_calls == 1);
    REQUIRE(ps.active_particle_count > 0);
    REQUIRE(ps.active_particle_count <= ps.max_particles);
    REQUIRE(rhi.update_buffer_calls == 1);
    REQUIRE(rhi.last_update_handle == 77);
    REQUIRE(rhi.last_update_offset == 0);
    REQUIRE(rhi.last_update_is_index == false);
    REQUIRE(rhi.last_update_data != nullptr);
    REQUIRE(rhi.last_update_size == static_cast<size_t>(ps.active_particle_count * 8 * sizeof(float)));
}

TEST_CASE("Given_InitializedParticle3DSystem_When_Shutdown_Then_InstanceBufferIsReleased", "[engine][unit][particle3d]") {
    World world;
    MockRhiDevice rhi;
    AssetManager asset_manager;
    Particle3DSystem system;
    system.Init(world, &rhi);
    system.SetAssetManager(&asset_manager);

    const auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    auto& ps = world.registry().emplace<ParticleSystem3DComponent>(entity);
    ps.initialized = true;
    ps.instance_vbo = 123;

    system.Shutdown(world);

    REQUIRE(ps.instance_vbo == 0);
    REQUIRE(rhi.delete_buffer_calls == 1);
    REQUIRE(rhi.last_deleted_buffer == 123);
}
