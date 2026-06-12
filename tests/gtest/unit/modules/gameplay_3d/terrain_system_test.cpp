/**
 * @file terrain_system_test.cpp
 * @brief TerrainSystem + TerrainComponent 无 GPU 单元测试
 *
 * 测试策略：
 * - TerrainComponent 默认值完整性
 * - TerrainSystem 空 World 不崩溃
 * - LOD 距离因子 / dirty 标记
 * - 高度数据容器
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/opengl/gl_command_buffer.h"

using namespace dse;
using namespace dse::render;
using namespace dse::gameplay3d;

// ============================================================
// TerrainComponent 默认值
// ============================================================

// 测试 地形组件：默认值
TEST(TerrainComponentTest, DefaultValues) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.enabled);
    EXPECT_TRUE(tc.heightmap_path.empty());
    EXPECT_TRUE(tc.texture_path.empty());
    EXPECT_EQ(tc.texture_handle, 0u);
    EXPECT_FLOAT_EQ(tc.width, 100.0f);
    EXPECT_FLOAT_EQ(tc.depth, 100.0f);
    EXPECT_FLOAT_EQ(tc.max_height, 20.0f);
    EXPECT_EQ(tc.resolution_x, 64);
    EXPECT_EQ(tc.resolution_z, 64);
}

// 测试 地形组件：LOD参数默认值
TEST(TerrainComponentTest, LODParameterDefaultValue) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.use_dynamic_lod);
    EXPECT_EQ(tc.max_lod_levels, 4);
    EXPECT_FLOAT_EQ(tc.lod_distance_factor, 50.0f);
    EXPECT_EQ(tc.current_lod, 0);
    EXPECT_TRUE(tc.visible);
}

// 测试 地形组件：泼溅映射默认值
TEST(TerrainComponentTest, SplatMapDefaultValues) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.splat_data.empty());
    EXPECT_TRUE(tc.splat_dirty);
    EXPECT_EQ(tc.splat_weight_texture, 0u);  // 未上传前权重图句柄为 0
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(tc.splat_texture_paths[i].empty());
        EXPECT_EQ(tc.splat_texture_handles[i], 0u);
    }
}

// 测试 地形组件：内部状态默认值
TEST(TerrainComponentTest, InsideStateDefaultValues) {
    TerrainComponent tc;
    EXPECT_TRUE(tc.is_dirty);
    EXPECT_TRUE(tc.height_data.empty());
    EXPECT_EQ(tc.vao.raw(), 0u);
    EXPECT_EQ(tc.vbo.raw(), 0u);
    EXPECT_EQ(tc.ebo.raw(), 0u);
    EXPECT_TRUE(tc.lod_ebos.empty());
    EXPECT_TRUE(tc.lod_index_counts.empty());
    EXPECT_EQ(tc.index_count, 0u);
}

// 测试 地形组件：数据写入
TEST(TerrainComponentTest, DataWrite) {
    TerrainComponent tc;
    tc.resolution_x = 4;
    tc.resolution_z = 4;
    tc.height_data.resize(16, 0.0f);
    tc.height_data[5] = 10.0f;
    EXPECT_FLOAT_EQ(tc.height_data[5], 10.0f);
    EXPECT_FLOAT_EQ(tc.height_data[0], 0.0f);
}

// ============================================================
// TerrainSystem
// ============================================================

// 测试 地形系统：默认安全
TEST(TerrainSystemTest, DefaultSafety) {
    TerrainSystem sys;
    (void)sys;
}

// 测试 地形系统：空世界不崩溃
TEST(TerrainSystemTest, EmptyWorldDoesNotCrash) {
    TerrainSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    sys.Render(world, cmd);
}

// 测试 地形系统：禁用不渲染
TEST(TerrainSystemTest, DisabledDoesNotRender) {
    TerrainSystem sys;
    World world;
    auto entity = world.registry().create();
    auto& tc = world.registry().emplace<TerrainComponent>(entity);
    tc.enabled = false;
    world.registry().emplace<TransformComponent>(entity);

    OpenGLCommandBuffer cmd;
    sys.Render(world, cmd);
}

// ============================================================
// UploadSplatWeightMap 分支测试（Fake RhiDevice，无真实 GPU）
// ============================================================

namespace {

// 记录式 Fake：捕获 splat 权重图上传的尺寸/像素/采样描述与纹理释放，
// 用于在无 GL 环境下驱动 UploadSplatWeightMap 的 CPU 侧分支。
class TerrainFakeRhiDevice final : public RhiDevice {
public:
    struct CreatedTexture {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> rgba8;
        TextureFilter filter = TextureFilter::Linear;
        TextureWrap wrap = TextureWrap::Repeat;
    };

    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                 const TextureSamplerDesc& sampler) override {
        CreatedTexture t;
        t.width = width;
        t.height = height;
        t.filter = sampler.filter;
        t.wrap = sampler.wrap;
        if (rgba8_data && width > 0 && height > 0) {
            t.rgba8.assign(rgba8_data,
                           rgba8_data + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        }
        created.push_back(std::move(t));
        return next_texture_handle_++;
    }

    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override {
        TextureSamplerDesc s;
        s.filter = linear_filter ? TextureFilter::Linear : TextureFilter::Nearest;
        return CreateTexture2D(width, height, rgba8_data, s);
    }

    void DeleteTexture(unsigned int texture_handle) override {
        deleted_textures.push_back(texture_handle);
    }

    // --- 接口要求的其余纯虚函数桩 ---
    void Shutdown() override {}
    void BeginFrame() override {}
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override { (void)desc; return 0; }
    unsigned int GetRenderTargetColorTexture(unsigned int h) const override { (void)h; return 0; }
    unsigned int GetRenderTargetDepthTexture(unsigned int h) const override { (void)h; return 0; }
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int h) const override { (void)h; return {}; }
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int h) const override { (void)h; return {}; }
    unsigned int CreateTextureCube(int w, int h, const unsigned char* const f[6], bool l) override { (void)w; (void)h; (void)f; (void)l; return 0; }
    unsigned int CreateTexture3D(int w, int h, int d, const unsigned char* data, bool l) override { (void)w; (void)h; (void)d; (void)data; (void)l; return 0; }
    unsigned int CreateShaderProgram(const std::string& v, const std::string& f) override { (void)v; (void)f; return 0; }
    void DeleteShaderProgram(unsigned int h) override { (void)h; }
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override { (void)desc; return 0; }
    unsigned int CreateBuffer(size_t s, const void* d, bool dyn, bool idx) override { (void)s; (void)d; (void)dyn; (void)idx; return 0; }
    void UpdateBuffer(unsigned int h, size_t o, size_t s, const void* d, bool idx) override { (void)h; (void)o; (void)s; (void)d; (void)idx; }
    void DeleteBuffer(unsigned int h) override { (void)h; }
    dse::render::VertexArrayHandle CreateVertexArray() override { return {}; }
    void DeleteVertexArray(dse::render::VertexArrayHandle h) override { (void)h; }
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return nullptr; }
    void Submit(std::shared_ptr<CommandBuffer> c) override { (void)c; }
    void EndFrame() override {}
    const RenderStats& LastFrameStats() const override { return stats_; }

    std::vector<CreatedTexture> created;
    std::vector<unsigned int> deleted_textures;

private:
    unsigned int next_texture_handle_ = 700001;
    RenderStats stats_{};
};

// 构造一个 w×h 全 1.0 权重的 splat_data（RGBA 逐顶点 float）。
TerrainComponent MakeSplatTerrain(int w, int h, float fill) {
    TerrainComponent tc;
    tc.resolution_x = w;
    tc.resolution_z = h;
    tc.splat_data.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, fill);
    tc.splat_dirty = true;
    return tc;
}

} // namespace

// 测试 地形泼溅上传：无当
TEST(TerrainSplatUploadTest, WithoutWhen) {
    TerrainSystem sys;  // 未 Init，rhi_ == nullptr
    TerrainComponent tc = MakeSplatTerrain(2, 2, 1.0f);
    sys.UploadSplatWeightMap(tc);
    EXPECT_TRUE(tc.splat_dirty);                 // 无设备不消费脏标志
    EXPECT_EQ(tc.splat_weight_texture, 0u);
}

// 测试 地形泼溅上传：非当不
TEST(TerrainSplatUploadTest, NonWhenNot) {
    TerrainFakeRhiDevice fake;
    TerrainSystem sys;
    sys.Init(&fake);
    TerrainComponent tc = MakeSplatTerrain(2, 2, 1.0f);
    tc.splat_dirty = false;  // 已是干净
    sys.UploadSplatWeightMap(tc);
    EXPECT_TRUE(fake.created.empty());
    EXPECT_EQ(tc.splat_weight_texture, 0u);
}

// 测试 地形泼溅上传：Validdata
TEST(TerrainSplatUploadTest, Validdata) {
    TerrainFakeRhiDevice fake;
    TerrainSystem sys;
    sys.Init(&fake);
    TerrainComponent tc = MakeSplatTerrain(2, 2, 1.0f);

    sys.UploadSplatWeightMap(tc);

    ASSERT_EQ(fake.created.size(), 1u);
    EXPECT_EQ(fake.created[0].width, 2);
    EXPECT_EQ(fake.created[0].height, 2);
    // 线性采样 + ClampToEdge（防权重图边缘出血）。
    EXPECT_EQ(fake.created[0].filter, TextureFilter::Linear);
    EXPECT_EQ(fake.created[0].wrap, TextureWrap::ClampToEdge);
    // float 1.0 → RGBA8 255。
    ASSERT_EQ(fake.created[0].rgba8.size(), 16u);
    for (unsigned char b : fake.created[0].rgba8) EXPECT_EQ(b, 255);
    EXPECT_NE(tc.splat_weight_texture, 0u);
    EXPECT_FALSE(tc.splat_dirty);  // 脏标志已消费
}

// 测试 地形泼溅上传：Floatweight钳制到0 255
TEST(TerrainSplatUploadTest, FloatweightClampedTo0_255) {
    TerrainFakeRhiDevice fake;
    TerrainSystem sys;
    sys.Init(&fake);
    TerrainComponent tc;
    tc.resolution_x = 1;
    tc.resolution_z = 1;
    tc.splat_data = {-0.5f, 0.0f, 0.5f, 2.0f};  // 越界值应被钳制
    tc.splat_dirty = true;

    sys.UploadSplatWeightMap(tc);

    ASSERT_EQ(fake.created.size(), 1u);
    ASSERT_EQ(fake.created[0].rgba8.size(), 4u);
    EXPECT_EQ(fake.created[0].rgba8[0], 0);            // -0.5 → 0
    EXPECT_EQ(fake.created[0].rgba8[1], 0);            //  0.0 → 0
    EXPECT_EQ(fake.created[0].rgba8[2], 128);          //  0.5 → 128 (0.5*255+0.5)
    EXPECT_EQ(fake.created[0].rgba8[3], 255);          //  2.0 → 255
}

// 测试 地形泼溅上传：数据不Whenrollbackrelease
TEST(TerrainSplatUploadTest, DataNotWhenrollbackrelease) {
    TerrainFakeRhiDevice fake;
    TerrainSystem sys;
    sys.Init(&fake);

    // 先正常上传一次，拿到旧纹理句柄。
    TerrainComponent tc = MakeSplatTerrain(2, 2, 1.0f);
    sys.UploadSplatWeightMap(tc);
    const unsigned int old_handle = tc.splat_weight_texture;
    ASSERT_NE(old_handle, 0u);

    // 数据被清空但标脏 → 应释放旧纹理、句柄归 0、不再创建新纹理、脏标志清除。
    tc.splat_data.clear();
    tc.splat_dirty = true;
    sys.UploadSplatWeightMap(tc);

    EXPECT_EQ(tc.splat_weight_texture, 0u);
    EXPECT_FALSE(tc.splat_dirty);
    ASSERT_FALSE(fake.deleted_textures.empty());
    EXPECT_EQ(fake.deleted_textures.back(), old_handle);
    EXPECT_EQ(fake.created.size(), 1u);  // 第二次未新建
}
