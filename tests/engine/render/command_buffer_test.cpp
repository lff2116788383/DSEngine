#include "catch/catch.hpp"

#include "engine/render/rhi/rhi_device.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

bool MatrixEquals(const glm::mat4& lhs, const glm::mat4& rhs, float epsilon = 0.0001f) {
    const float* left = glm::value_ptr(lhs);
    const float* right = glm::value_ptr(rhs);
    for (int i = 0; i < 16; ++i) {
        if (std::abs(left[i] - right[i]) > epsilon) {
            return false;
        }
    }
    return true;
}

class RecordingCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override {
        begin_render_pass_calls++;
        last_render_pass = render_pass;
    }

    void EndRenderPass() override {
        end_render_pass_calls++;
    }

    void SetPipelineState(unsigned int pipeline_state_handle) override {
        last_pipeline_state = pipeline_state_handle;
    }

    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override {
        camera_set_calls++;
        last_view = view;
        last_projection = projection;
    }

    void DrawBatch(const std::vector<DrawBatchItem>& items) override {
        draw_batch_calls++;
        last_draw_batch = items;
    }

    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override {
        draw_mesh_batch_calls++;
        last_mesh_batch = items;
    }

    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override {
        draw_sprite_batch_calls++;
        last_sprite_batch = items;
    }

    void ClearColor(const glm::vec4& color) override {
        clear_calls++;
        last_clear_color = color;
    }

    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override {
        set_global_mat4_calls++;
        last_mat4_name = name;
        last_mat4_value = value;
    }

    void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) override {
        set_global_mat4_array_calls++;
        last_mat4_array_name = name;
        last_mat4_array = values;
    }

    void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) override {
        set_global_float_array_calls++;
        last_float_array_name = name;
        last_float_array = values;
    }

    void DrawSkybox(unsigned int cubemap_texture_handle) override {
        draw_skybox_calls++;
        last_skybox = cubemap_texture_handle;
    }

    void DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) override {
        draw_post_process_calls++;
        last_post_process_source = source_texture;
        last_post_process_effect = effect_name;
        last_post_process_params = params;
    }

    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override {
        draw_particles_calls++;
        last_particles = items;
        last_particles_view = view;
        last_particles_projection = projection;
    }

    int begin_render_pass_calls = 0;
    int end_render_pass_calls = 0;
    int camera_set_calls = 0;
    int draw_batch_calls = 0;
    int draw_mesh_batch_calls = 0;
    int draw_sprite_batch_calls = 0;
    int clear_calls = 0;
    int set_global_mat4_calls = 0;
    int set_global_mat4_array_calls = 0;
    int set_global_float_array_calls = 0;
    int draw_skybox_calls = 0;
    int draw_post_process_calls = 0;
    int draw_particles_calls = 0;

    RenderPassDesc last_render_pass{};
    unsigned int last_pipeline_state = 0;
    glm::mat4 last_view = glm::mat4(1.0f);
    glm::mat4 last_projection = glm::mat4(1.0f);
    glm::vec4 last_clear_color = glm::vec4(0.0f);
    std::string last_mat4_name;
    glm::mat4 last_mat4_value = glm::mat4(1.0f);
    std::string last_mat4_array_name;
    std::vector<glm::mat4> last_mat4_array;
    std::string last_float_array_name;
    std::vector<float> last_float_array;
    unsigned int last_skybox = 0;
    unsigned int last_post_process_source = 0;
    std::string last_post_process_effect;
    std::vector<float> last_post_process_params;
    std::vector<DrawBatchItem> last_draw_batch;
    std::vector<MeshDrawItem> last_mesh_batch;
    std::vector<SpriteDrawItem> last_sprite_batch;
    std::vector<Particle3DDrawItem> last_particles;
    glm::mat4 last_particles_view = glm::mat4(1.0f);
    glm::mat4 last_particles_projection = glm::mat4(1.0f);
};

} // namespace

TEST_CASE("Given_CommandBufferContract_When_RecordingCommands_Then_LastSubmittedStateIsPreserved", "[engine][render]") {
    RecordingCommandBuffer cmd;

    RenderPassDesc render_pass{};
    render_pass.render_target = 99;
    render_pass.clear_color = glm::vec4(0.1f, 0.2f, 0.3f, 1.0f);
    render_pass.clear_color_enabled = true;

    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 1.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.1f, 10.0f);

    DrawBatchItem draw_item{};
    draw_item.texture_handle = 42;
    draw_item.material_instance_id = 77;
    draw_item.shader_variant_key = 5;
    draw_item.blend_mode = 2;
    draw_item.order_in_layer = 3;

    MeshDrawItem mesh_item{};
    mesh_item.texture_handle = 123;
    mesh_item.vertices.push_back(BatchVertex{});

    SpriteDrawItem sprite_item{};
    sprite_item.texture_handle = 88;
    sprite_item.order_in_layer = 9;

    Particle3DDrawItem particle_item{};
    particle_item.texture_handle = 7;
    particle_item.particle_count = 2;

    cmd.BeginRenderPass(render_pass);
    cmd.SetPipelineState(17);
    cmd.SetCamera(view, projection);
    cmd.DrawBatch({draw_item});
    cmd.DrawMeshBatch({mesh_item});
    cmd.DrawSpriteBatch({sprite_item});
    cmd.ClearColor(glm::vec4(0.9f, 0.8f, 0.7f, 1.0f));
    cmd.SetGlobalMat4("u_view_proj", projection * view);
    cmd.SetGlobalMat4Array("u_cascades", {view, projection});
    cmd.SetGlobalFloatArray("u_splits", {0.1f, 0.3f, 0.6f});
    cmd.DrawSkybox(501);
    cmd.DrawPostProcess(600, "bloom_extract", {1.2f});
    cmd.DrawParticles3D({particle_item}, view, projection);
    cmd.EndRenderPass();

    REQUIRE(cmd.begin_render_pass_calls == 1);
    REQUIRE(cmd.end_render_pass_calls == 1);
    REQUIRE(cmd.last_render_pass.render_target == 99);
    REQUIRE(cmd.last_render_pass.clear_color_enabled);
    REQUIRE(cmd.last_pipeline_state == 17);
    REQUIRE(cmd.camera_set_calls == 1);
    REQUIRE(MatrixEquals(cmd.last_view, view));
    REQUIRE(MatrixEquals(cmd.last_projection, projection));

    REQUIRE(cmd.draw_batch_calls == 1);
    REQUIRE(cmd.last_draw_batch.size() == 1);
    REQUIRE(cmd.last_draw_batch.front().texture_handle == 42);
    REQUIRE(cmd.last_draw_batch.front().blend_mode == 2);

    REQUIRE(cmd.draw_mesh_batch_calls == 1);
    REQUIRE(cmd.last_mesh_batch.size() == 1);
    REQUIRE(cmd.last_mesh_batch.front().texture_handle == 123);
    REQUIRE(cmd.last_mesh_batch.front().vertices.size() == 1);

    REQUIRE(cmd.draw_sprite_batch_calls == 1);
    REQUIRE(cmd.last_sprite_batch.size() == 1);
    REQUIRE(cmd.last_sprite_batch.front().texture_handle == 88);

    REQUIRE(cmd.clear_calls == 1);
    REQUIRE(cmd.last_clear_color == glm::vec4(0.9f, 0.8f, 0.7f, 1.0f));

    REQUIRE(cmd.set_global_mat4_calls == 1);
    REQUIRE(cmd.last_mat4_name == "u_view_proj");
    REQUIRE(cmd.set_global_mat4_array_calls == 1);
    REQUIRE(cmd.last_mat4_array_name == "u_cascades");
    REQUIRE(cmd.last_mat4_array.size() == 2);
    REQUIRE(cmd.set_global_float_array_calls == 1);
    REQUIRE(cmd.last_float_array_name == "u_splits");
    REQUIRE(cmd.last_float_array == std::vector<float>({0.1f, 0.3f, 0.6f}));

    REQUIRE(cmd.draw_skybox_calls == 1);
    REQUIRE(cmd.last_skybox == 501);
    REQUIRE(cmd.draw_post_process_calls == 1);
    REQUIRE(cmd.last_post_process_source == 600);
    REQUIRE(cmd.last_post_process_effect == "bloom_extract");
    REQUIRE(cmd.last_post_process_params == std::vector<float>({1.2f}));

    REQUIRE(cmd.draw_particles_calls == 1);
    REQUIRE(cmd.last_particles.size() == 1);
    REQUIRE(cmd.last_particles.front().texture_handle == 7);
    REQUIRE(MatrixEquals(cmd.last_particles_view, view));
    REQUIRE(MatrixEquals(cmd.last_particles_projection, projection));
}

TEST_CASE("Given_OpenGLCommandBuffer_When_DrawMeshOrParticlesInputIsEmpty_Then_RecordingRemainsStable", "[engine][render][rhi]") {
    OpenGLCommandBuffer cmd;
    cmd.DrawMeshBatch({});
    cmd.DrawParticles3D({}, glm::mat4(1.0f), glm::mat4(1.0f));
    SUCCEED();
}

TEST_CASE("Given_OpenGLCommandBuffer_When_OnlyUnrecognizedGlobalUniformsAreRecorded_Then_RecordingSucceedsWithoutDraws", "[engine][render][rhi]") {
    OpenGLCommandBuffer cmd;
    cmd.SetGlobalMat4("u_view_proj", glm::mat4(2.0f));
    cmd.SetGlobalMat4Array("u_unused", {glm::mat4(3.0f), glm::mat4(4.0f)});
    cmd.SetGlobalFloatArray("u_unused_splits", {0.25f, 0.5f, 0.75f});
    SUCCEED();
}

TEST_CASE("Given_OpenGLCommandBuffer_When_CommandsAreRecordedInterleaved_Then_RecordingAcceptsMixedCommandTypes", "[engine][render][rhi]") {
    OpenGLCommandBuffer cmd;

    RenderPassDesc first_pass{};
    first_pass.render_target = 1;
    RenderPassDesc second_pass{};
    second_pass.render_target = 2;

    DrawBatchItem draw_item{};
    draw_item.texture_handle = 101;

    MeshDrawItem mesh_item{};
    mesh_item.texture_handle = 202;
    mesh_item.vertices.push_back(BatchVertex{});
    mesh_item.indices.push_back(0);

    Particle3DDrawItem particle_item{};
    particle_item.texture_handle = 303;
    particle_item.particle_count = 1;

    const glm::mat4 particle_view = glm::lookAt(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 particle_projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 50.0f);

    cmd.BeginRenderPass(first_pass);
    cmd.DrawBatch({draw_item});
    cmd.EndRenderPass();
    cmd.BeginRenderPass(second_pass);
    cmd.DrawMeshBatch({mesh_item});
    cmd.DrawParticles3D({particle_item}, particle_view, particle_projection);
    cmd.EndRenderPass();
    SUCCEED();
}

TEST_CASE("Given_OpenGLCommandBuffer_When_SpecialGlobalUniformsAreRecordedBeforeAndAfterDraws_Then_RecordingStillSucceeds", "[engine][render][rhi]") {
    OpenGLCommandBuffer cmd;

    DrawBatchItem draw_item{};
    draw_item.texture_handle = 444;

    MeshDrawItem mesh_item{};
    mesh_item.texture_handle = 555;
    mesh_item.vertices.push_back(BatchVertex{});
    mesh_item.indices.push_back(0);

    cmd.SetGlobalMat4("u_light_space_matrix", glm::mat4(1.0f));
    cmd.DrawBatch({draw_item});
    cmd.SetGlobalMat4Array("u_light_space_matrices", {glm::mat4(2.0f), glm::mat4(3.0f)});
    cmd.DrawMeshBatch({mesh_item});
    cmd.SetGlobalFloatArray("u_cascade_splits", {0.2f, 0.4f, 0.8f});
    SUCCEED();
}

TEST_CASE("Given_OpenGLCommandBuffer_When_MultiplePipelineStatesAndClearsAreInterleaved_Then_RecordingStillSucceeds", "[engine][render][rhi]") {
    OpenGLCommandBuffer cmd;
    cmd.SetPipelineState(11);
    cmd.ClearColor(glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    cmd.SetPipelineState(22);
    cmd.ClearColor(glm::vec4(0.6f, 0.5f, 0.4f, 1.0f));
    SUCCEED();
}

TEST_CASE("Given_OpenGLCommandBuffer_When_CameraChangesBetweenSpriteAndSkyboxCommands_Then_RecordingPreservesSnapshotsWithoutCrashing", "[engine][render][rhi]") {
    OpenGLCommandBuffer cmd;

    const glm::mat4 first_view = glm::lookAt(glm::vec3(0.0f, 0.0f, 8.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 first_projection = glm::ortho(-3.0f, 3.0f, -2.0f, 2.0f, 0.1f, 20.0f);
    const glm::mat4 second_view = glm::lookAt(glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 second_projection = glm::perspective(glm::radians(55.0f), 1.2f, 0.1f, 100.0f);

    SpriteDrawItem sprite_item{};
    sprite_item.texture_handle = 909;

    cmd.SetCamera(first_view, first_projection);
    cmd.DrawSpriteBatch({sprite_item});
    cmd.SetCamera(second_view, second_projection);
    cmd.DrawSkybox(707);
    SUCCEED();
}
