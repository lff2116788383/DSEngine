#include "catch/catch.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>

#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/time.h"

namespace {

class RecordingCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc&) override {}
    void EndRenderPass() override {}
    void SetPipelineState(unsigned int) override {}

    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override {
        camera_set = true;
        last_view = view;
        last_projection = projection;
    }

    void DrawBatch(const std::vector<DrawBatchItem>& items) override {
        draw_batch_calls++;
        last_batch = items;
    }

    void DrawMeshBatch(const std::vector<MeshDrawItem>&) override {}
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>&) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetGlobalMat4(const std::string&, const glm::mat4&) override {}
    void SetGlobalMat4Array(const std::string&, const std::vector<glm::mat4>&) override {}
    void SetGlobalFloatArray(const std::string&, const std::vector<float>&) override {}
    void DrawSkybox(unsigned int) override {}
    void DrawPostProcess(unsigned int, const std::string&, const std::vector<float>&) override {}
    void DrawParticles3D(const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&) override {}

    bool camera_set = false;
    int draw_batch_calls = 0;
    glm::mat4 last_view = glm::mat4(1.0f);
    glm::mat4 last_projection = glm::mat4(1.0f);
    std::vector<DrawBatchItem> last_batch;
};

} // namespace

TEST_CASE("Given_MixedSpriteEntities_When_Render_Then_InvisibleSpritesAreSkippedAndBatchIsSorted", "[engine][unit][rendering][2d][sprite]") {
    World world;
    SpriteRenderSystem system;
    RecordingCommandBuffer cmd;

    auto hidden = world.CreateEntity();
    auto& hidden_tf = world.registry().emplace<TransformComponent>(hidden);
    hidden_tf.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(99.0f, 99.0f, 0.0f));
    auto& hidden_sprite = world.registry().emplace<SpriteRendererComponent>(hidden);
    hidden_sprite.visible = false;
    hidden_sprite.texture_handle = 999;

    auto late = world.CreateEntity();
    auto& late_tf = world.registry().emplace<TransformComponent>(late);
    late_tf.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(30.0f, 0.0f, 0.0f));
    auto& late_sprite = world.registry().emplace<SpriteRendererComponent>(late);
    late_sprite.sorting_layer = 2;
    late_sprite.order_in_layer = 9;
    late_sprite.shader_variant = "variant_z";
    late_sprite.material_instance_id = 5;
    late_sprite.texture_handle = 8;
    late_sprite.blend_mode = SpriteBlendMode::Additive;
    late_sprite.color = glm::vec4(0.2f, 0.4f, 0.6f, 1.0f);

    auto first = world.CreateEntity();
    auto& first_tf = world.registry().emplace<TransformComponent>(first);
    first_tf.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
    auto& first_sprite = world.registry().emplace<SpriteRendererComponent>(first);
    first_sprite.sorting_layer = 0;
    first_sprite.order_in_layer = 10;
    first_sprite.shader_variant = "shared_variant";
    first_sprite.material_instance_id = 3;
    first_sprite.texture_handle = 4;
    first_sprite.blend_mode = SpriteBlendMode::Alpha;
    first_sprite.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    auto second = world.CreateEntity();
    auto& second_tf = world.registry().emplace<TransformComponent>(second);
    second_tf.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, 0.0f));
    auto& second_sprite = world.registry().emplace<SpriteRendererComponent>(second);
    second_sprite.sorting_layer = 0;
    second_sprite.order_in_layer = 1;
    second_sprite.shader_variant = "shared_variant";
    second_sprite.material_instance_id = 1;
    second_sprite.texture_handle = 2;
    second_sprite.blend_mode = SpriteBlendMode::Alpha;
    second_sprite.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

    system.Render(world, cmd);

    REQUIRE(cmd.draw_batch_calls == 1);
    REQUIRE(cmd.last_batch.size() == 3);

    REQUIRE(cmd.last_batch[0].sorting_layer == 0);
    REQUIRE(cmd.last_batch[0].material_instance_id == 1);
    REQUIRE(cmd.last_batch[0].texture_handle == 2);
    REQUIRE(cmd.last_batch[0].order_in_layer == 1);
    REQUIRE(cmd.last_batch[0].color == second_sprite.color);

    REQUIRE(cmd.last_batch[1].sorting_layer == 0);
    REQUIRE(cmd.last_batch[1].material_instance_id == 3);
    REQUIRE(cmd.last_batch[1].texture_handle == 4);
    REQUIRE(cmd.last_batch[1].order_in_layer == 10);
    REQUIRE(cmd.last_batch[1].color == first_sprite.color);

    REQUIRE(cmd.last_batch[2].sorting_layer == 2);
    REQUIRE(cmd.last_batch[2].material_instance_id == 5);
    REQUIRE(cmd.last_batch[2].texture_handle == 8);
    REQUIRE(cmd.last_batch[2].order_in_layer == 9);
    REQUIRE(cmd.last_batch[2].color == late_sprite.color);
}

TEST_CASE("Given_SpriteWithStaticUVOffset_When_Render_Then_SubmittedUVReflectsCurrentOffset", "[engine][unit][rendering][2d][sprite]") {
    World world;
    SpriteRenderSystem system;
    RecordingCommandBuffer cmd;

    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    auto& sprite = world.registry().emplace<SpriteRendererComponent>(entity);
    sprite.uv = glm::vec4(0.1f, 0.2f, 0.3f, 0.4f);
    sprite.uv_offset = glm::vec2(1.0f, 2.0f);
    sprite.uv_scroll_speed = glm::vec2(0.0f, 0.0f);

    system.Render(world, cmd);

    REQUIRE(cmd.last_batch.size() == 1);
    REQUIRE(sprite.uv_offset.x == Approx(1.0f));
    REQUIRE(sprite.uv_offset.y == Approx(2.0f));
    REQUIRE(cmd.last_batch[0].uv.x == Approx(1.1f));
    REQUIRE(cmd.last_batch[0].uv.y == Approx(2.2f));
    REQUIRE(cmd.last_batch[0].uv.z == Approx(0.3f));
    REQUIRE(cmd.last_batch[0].uv.w == Approx(0.4f));
}

TEST_CASE("Given_VisibleUIElements_When_Render_Then_RuntimeModelAndTintAndCameraAreUpdated", "[engine][unit][rendering][2d][ui]") {
    World world;
    UIRenderSystem system;
    RecordingCommandBuffer cmd;

    auto hovered = world.CreateEntity();
    auto& hovered_ui = world.registry().emplace<UIRendererComponent>(hovered);
    hovered_ui.texture_handle = 5;
    hovered_ui.position = glm::vec2(10.0f, 20.0f);
    hovered_ui.size = glm::vec2(100.0f, 40.0f);
    hovered_ui.anchor_min = glm::vec2(0.5f, 0.5f);
    hovered_ui.pivot = glm::vec2(0.5f, 0.5f);
    hovered_ui.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    hovered_ui.is_hovered = true;
    hovered_ui.interactable = true;
    hovered_ui.order = 2;

    auto pressed = world.CreateEntity();
    auto& pressed_ui = world.registry().emplace<UIRendererComponent>(pressed);
    pressed_ui.texture_handle = 3;
    pressed_ui.position = glm::vec2(-20.0f, -30.0f);
    pressed_ui.size = glm::vec2(80.0f, 20.0f);
    pressed_ui.anchor_min = glm::vec2(0.0f, 0.0f);
    pressed_ui.pivot = glm::vec2(0.0f, 0.0f);
    pressed_ui.color = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f);
    pressed_ui.is_pressed = true;
    pressed_ui.interactable = true;
    pressed_ui.order = 1;

    auto hidden = world.CreateEntity();
    auto& hidden_ui = world.registry().emplace<UIRendererComponent>(hidden);
    hidden_ui.visible = false;
    hidden_ui.texture_handle = 99;

    system.Render(world, cmd, 800, 600);

    REQUIRE(cmd.camera_set);
    REQUIRE(cmd.draw_batch_calls == 1);
    REQUIRE(cmd.last_batch.size() == 2);

    const auto& pressed_item = cmd.last_batch[0];
    const auto& hovered_item = cmd.last_batch[1];

    REQUIRE(pressed_item.texture_handle == 3);
    REQUIRE(pressed_item.order_in_layer == 1);
    REQUIRE(pressed_item.color.r == Approx(0.8f));
    REQUIRE(pressed_item.color.g == Approx(0.4f));
    REQUIRE(pressed_item.color.b == Approx(0.2f));
    REQUIRE(pressed_ui.runtime_model[3][0] == Approx(-20.0f));
    REQUIRE(pressed_ui.runtime_model[3][1] == Approx(-30.0f));

    REQUIRE(hovered_item.texture_handle == 5);
    REQUIRE(hovered_item.order_in_layer == 2);
    REQUIRE(hovered_item.color.r == Approx(0.6f));
    REQUIRE(hovered_item.color.g == Approx(0.6f));
    REQUIRE(hovered_item.color.b == Approx(0.6f));
    REQUIRE(hovered_ui.runtime_model[3][0] == Approx(360.0f));
    REQUIRE(hovered_ui.runtime_model[3][1] == Approx(300.0f));

    REQUIRE(cmd.last_view[0][0] == Approx(1.0f));
    REQUIRE(cmd.last_view[1][1] == Approx(1.0f));
    REQUIRE(cmd.last_projection[0][0] == Approx(2.0f / 800.0f));
    REQUIRE(cmd.last_projection[1][1] == Approx(2.0f / 600.0f));
}

TEST_CASE("Given_NoVisibleUI_When_Render_Then_NoCameraOrDrawCommandsAreSubmitted", "[engine][unit][rendering][2d][ui]") {
    World world;
    UIRenderSystem system;
    RecordingCommandBuffer cmd;

    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = false;

    system.Render(world, cmd, 1280, 720);

    REQUIRE_FALSE(cmd.camera_set);
    REQUIRE(cmd.draw_batch_calls == 0);
    REQUIRE(cmd.last_batch.empty());
}
