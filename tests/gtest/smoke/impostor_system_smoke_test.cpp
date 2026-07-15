/**
 * @file impostor_system_smoke_test.cpp
 * @brief ImpostorSystem 端到端守护冒烟 — 补全"已接线但要资产的路径"的回归防护。
 *
 * 现有 impostor_system_test.cpp 仅覆盖组件/配置/头部数据的纯数据字段。ImpostorSystem
 * 自身的运行时生命周期（扫描 ECS → 排队烘焙 → GL 线程烘焙 atlas → 下一帧产出绘制批次
 * → RenderOpaque → Shutdown）此前无守护。本冒烟在真实 GL 上下文下跑通该整条链路：
 *
 *   1. 空 World 调用 Update 不崩溃（无 impostor 实体）
 *   2. 带 ImpostorComponent + MeshRendererComponent 几何的实体：首帧无 atlas → 入队烘焙
 *   3. RenderOpaque（持有 GL 上下文）触发 ImpostorBaker 烘焙 atlas → 组件 atlas_loaded_
 *   4. 次帧 Update 距离判定命中 → 产出批次；RenderOpaque 正常绘制
 *   5. Shutdown 清理
 *
 * 无 GL 驱动时（CI 无显卡）自动 SKIP，不误报失败。
 */

#include "rhi_pixel_harness.h"

#include "engine/render/impostor/impostor_system.h"
#include "engine/render/render_scene.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_impostor.h"

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using dse::render::ImpostorSystem;
using dse::render::RenderScenePassContext;
using dse::render::RhiDevice;
using dse::ImpostorComponent;
using dse::ImpostorFrameMode;
using dse::MeshRendererComponent;
// TransformComponent 与 World 位于全局命名空间。

namespace {

// 在给定 RhiDevice 上跑完整条 impostor 生命周期。断言用 EXPECT_*（不 return），
// 以满足 RenderFn 的非 void 返回约束。返回空 readback（本冒烟不比对像素）。
RenderTargetReadback RunImpostorLifecycle(RhiDevice& device) {
    ImpostorSystem sys;
    sys.SetRenderContext(&device, glm::mat4(1.0f), glm::mat4(1.0f),
                         glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                         glm::vec3(0.15f));

    // (1) 空 World：扫描零实体，不崩溃。
    {
        World empty_world;
        sys.Update(empty_world, glm::vec3(0.0f), device);
    }

    // (2) 构建一个带几何的 impostor 实体（相机在原点外 200 单位，位于 transition..cull 之间）。
    World world;
    auto e = world.CreateEntity();

    TransformComponent tf;
    tf.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 200.0f));
    world.registry().emplace<TransformComponent>(e, tf);

    MeshRendererComponent mr;
    mr.temp_vertices = {-1.0f, -1.0f, 0.0f,  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f};
    mr.temp_indices  = {0, 1, 2};
    mr.temp_normals  = {0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f};
    mr.temp_uvs      = {0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 1.0f};
    mr.local_bounds_valid = true;
    mr.local_bounds_min = glm::vec3(-1.0f, -1.0f, 0.0f);
    mr.local_bounds_max = glm::vec3(1.0f, 1.0f, 0.0f);
    world.registry().emplace<MeshRendererComponent>(e, std::move(mr));

    ImpostorComponent imp;
    imp.enabled = true;
    imp.auto_from_lod_group = false;  // 显式距离，避免依赖 LODGroup
    imp.frame_mode = ImpostorFrameMode::Billboard;
    imp.frames_x = 2;  // 小 atlas，降低烘焙成本
    imp.frames_y = 1;
    imp.transition_distance = 100.0f;
    imp.cull_distance = 500.0f;
    world.registry().emplace<ImpostorComponent>(e, imp);

    RenderScenePassContext ctx;
    const glm::mat4 view(1.0f);
    const glm::mat4 proj(1.0f);
    ctx.view = &view;
    ctx.projection = &proj;

    // (3) 首帧：Update 发现无 atlas → 入队；RenderOpaque 在 GL 上下文里烘焙。
    sys.Update(world, glm::vec3(0.0f), device);
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    EXPECT_NE(cmd, nullptr);
    if (cmd) {
        sys.RenderOpaque(*cmd, ctx);
        device.Submit(cmd);
    }
    device.EndFrame();

    const auto& imp_after = world.registry().get<ImpostorComponent>(e);
    EXPECT_TRUE(imp_after.atlas_loaded_)
        << "ImpostorSystem 应在持有 GL 上下文的 RenderOpaque 中烘焙 atlas";
    EXPECT_TRUE(imp_after.atlas_texture_handle_);

    // (4) 次帧：atlas 已就绪 → Update 产出批次，RenderOpaque 正常绘制不崩溃。
    sys.Update(world, glm::vec3(0.0f), device);
    device.BeginFrame();
    auto cmd2 = device.CreateCommandBuffer();
    if (cmd2) {
        sys.RenderOpaque(*cmd2, ctx);
        device.Submit(cmd2);
    }
    device.EndFrame();

    // (5) 清理。
    sys.Shutdown(device);

    return RenderTargetReadback{};
}

}  // namespace

TEST(ImpostorSystemSmoke, GL_BakeAndRenderLifecycle) {
    auto r = dse::test::RunOpenGL([](RhiDevice& d) { return RunImpostorLifecycle(d); });
    if (!r.available) {
        GTEST_SKIP() << "OpenGL 不可用: " << r.skip_reason;
    }
    SUCCEED();
}
