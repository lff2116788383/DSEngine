#include "catch/catch.hpp"
#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/render/rhi/rhi_device.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

bool MatrixNear(const glm::mat4& lhs, const glm::mat4& rhs, float epsilon = 0.0001f) {
    const float* a = glm::value_ptr(lhs);
    const float* b = glm::value_ptr(rhs);
    for (int i = 0; i < 16; ++i) {
        if (std::abs(a[i] - b[i]) > epsilon) {
            return false;
        }
    }
    return true;
}

class RecordingUiCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc&) override {}
    void EndRenderPass() override {}
    void SetPipelineState(unsigned int) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetGlobalMat4(const std::string&, const glm::mat4&) override {}
    void SetGlobalMat4Array(const std::string&, const std::vector<glm::mat4>&) override {}
    void SetGlobalFloatArray(const std::string&, const std::vector<float>&) override {}
    void DrawSkybox(unsigned int) override {}
    void DrawPostProcess(unsigned int, const std::string&, const std::vector<float>&) override {}
    void DrawParticles3D(const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&) override {}
    void DrawMeshBatch(const std::vector<MeshDrawItem>&) override {}
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>&) override {}

    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override {
        camera_call_count++;
        last_view = view;
        last_projection = projection;
    }

    void DrawBatch(const std::vector<SpriteDrawItem>& items) override {
        draw_batch_call_count++;
        last_items = items;
    }

    int camera_call_count = 0;
    int draw_batch_call_count = 0;
    glm::mat4 last_view = glm::mat4(1.0f);
    glm::mat4 last_projection = glm::mat4(1.0f);
    std::vector<SpriteDrawItem> last_items;
};

} // namespace

TEST_CASE("Given_VisibleUiElements_When_RenderCalled_Then_ItemsAreSortedAndCameraSet", "[engine][unit][ui_render]") {
    World world;

    auto a = world.CreateEntity();
    auto& ui_a = world.registry().emplace<UIRendererComponent>(a);
    ui_a.visible = true;
    ui_a.texture_handle = 9;
    ui_a.order = 5;
    ui_a.size = glm::vec2(100.0f, 40.0f);
    ui_a.position = glm::vec2(10.0f, 20.0f);
    ui_a.color = glm::vec4(1.0f);

    auto b = world.CreateEntity();
    auto& ui_b = world.registry().emplace<UIRendererComponent>(b);
    ui_b.visible = true;
    ui_b.texture_handle = 3;
    ui_b.order = 8;
    ui_b.size = glm::vec2(60.0f, 30.0f);
    ui_b.position = glm::vec2(30.0f, 40.0f);
    ui_b.color = glm::vec4(0.5f, 0.75f, 1.0f, 1.0f);

    RecordingUiCommandBuffer cmd;
    UIRenderSystem system;
    system.Render(world, cmd, 800, 600);

    REQUIRE(cmd.camera_call_count == 1);
    REQUIRE(cmd.draw_batch_call_count == 1);
    REQUIRE(cmd.last_items.size() == 2);
    REQUIRE(cmd.last_items[0].texture_handle == 3);
    REQUIRE(cmd.last_items[1].texture_handle == 9);
    REQUIRE(MatrixNear(cmd.last_view, glm::mat4(1.0f)));
    REQUIRE(MatrixNear(cmd.last_projection, glm::ortho(0.0f, 800.0f, 0.0f, 600.0f, -1.0f, 1.0f)));
}

TEST_CASE("Given_HoveredAndPressedUi_When_RenderCalled_Then_RenderColorReflectsInteractionState", "[engine][unit][ui_render]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    ui.interactable = true;
    ui.is_hovered = true;
    ui.is_pressed = true;
    ui.color = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f);
    ui.size = glm::vec2(120.0f, 50.0f);

    RecordingUiCommandBuffer cmd;
    UIRenderSystem system;
    system.Render(world, cmd, 1280, 720);

    REQUIRE(cmd.last_items.size() == 1);
    REQUIRE(cmd.last_items.front().color == glm::vec4(0.8f, 0.4f, 0.2f, 0.8f));
}

TEST_CASE("Given_NoVisibleUi_When_RenderCalled_Then_NoDrawBatchOrCameraIsIssued", "[engine][unit][ui_render]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = false;

    RecordingUiCommandBuffer cmd;
    UIRenderSystem system;
    system.Render(world, cmd, 640, 480);

    REQUIRE(cmd.camera_call_count == 0);
    REQUIRE(cmd.draw_batch_call_count == 0);
    REQUIRE(cmd.last_items.empty());
}
