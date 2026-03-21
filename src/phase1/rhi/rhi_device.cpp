#include "phase1/rhi/rhi_device.h"
#include "render_device/render_task_producer.h"
#include "utils/debug.h"
#include "utils/screen.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_INDICES = MAX_SPRITES * 6;

struct BatchVertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec2 uv;
};

void OpenGLRhiDevice::EnsureInitialized() {
    if (initialized_) {
        return;
    }
    const char* vertex_shader = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_vp;
out vec4 v_color;
out vec2 v_uv;
void main() {
    gl_Position = u_vp * vec4(a_pos, 1.0);
    v_color = a_color;
    v_uv = a_uv;
}
)";
    const char* fragment_shader = R"(
#version 330 core
in vec4 v_color;
in vec2 v_uv;
uniform sampler2D u_texture;
uniform vec3 u_tint;
out vec4 fragColor;
void main() {
    vec4 tex = texture(u_texture, v_uv);
    fragColor = vec4(v_color.rgb * u_tint.rgb, v_color.a) * tex;
}
)";
    RenderTaskProducer::ProduceRenderTaskCompileShader(vertex_shader, fragment_shader, shader_handle_);

    // Pre-allocate large buffers
    std::vector<BatchVertex> vertices(MAX_VERTICES);
    std::vector<unsigned short> indices(MAX_INDICES);
    
    for (size_t i = 0, j = 0; i < MAX_SPRITES; ++i) {
        indices[j++] = i * 4 + 0;
        indices[j++] = i * 4 + 1;
        indices[j++] = i * 4 + 2;
        indices[j++] = i * 4 + 2;
        indices[j++] = i * 4 + 3;
        indices[j++] = i * 4 + 0;
    }

    RenderTaskProducer::ProduceRenderTaskCreateVAO(
        shader_handle_,
        vao_handle_,
        vbo_handle_,
        vertices.size() * sizeof(BatchVertex),
        sizeof(BatchVertex),
        vertices.data(),
        indices.size() * sizeof(unsigned short),
        indices.data()
    );

    unsigned char white_texture[] = {255, 255, 255, 255};
    RenderTaskProducer::ProduceRenderTaskCreateTexImage2D(
        white_texture_handle_,
        1,
        1,
        GL_RGBA,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        sizeof(white_texture),
        white_texture
    );
    initialized_ = true;
}

void OpenGLCommandBuffer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    view_ = view;
    projection_ = projection;
}

void OpenGLCommandBuffer::DrawSpriteBatch(const std::vector<Phase1SpriteDrawItem>& items) {
    draw_batch_cmds_.push_back({items});
}

void OpenGLCommandBuffer::ClearColor(const glm::vec4& color) {
    clear_cmds_.push_back({color});
}

void OpenGLCommandBuffer::Execute(OpenGLRhiDevice* device) {
    if (!device) {
        return;
    }
    for (const auto& cmd : clear_cmds_) {
        device->RealClearColor(cmd.color);
    }
    for (const auto& cmd : draw_batch_cmds_) {
        device->RealSubmitSpriteBatch(cmd.items, view_, projection_);
    }
    clear_cmds_.clear();
    draw_batch_cmds_.clear();
}

void OpenGLRhiDevice::BeginFrame() {
    EnsureInitialized();
    current_frame_stats_ = {};
}

std::shared_ptr<CommandBuffer> OpenGLRhiDevice::CreateCommandBuffer() {
    return std::make_shared<OpenGLCommandBuffer>();
}

void OpenGLRhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    auto gl_cmd = std::dynamic_pointer_cast<OpenGLCommandBuffer>(cmd_buffer);
    if (gl_cmd) {
        // Mock execution
        // std::cout << "[RHI] Executing Command Buffer" << std::endl;
        
        // Execute clears first (simplification for Phase 1)
        // Wait, actually I should just implement real execution in Execute
        // and have OpenGLCommandBuffer call the real rendering tasks.
        // For now, let's keep the existing render tasks working.
        
        // Actually, since Phase1 uses RenderTaskProducer, we just need to queue those.
        // Let's modify OpenGLCommandBuffer to do it.
        gl_cmd->Execute(this);
    }
}

void OpenGLRhiDevice::RealClearColor(const glm::vec4& color) {
    RenderTaskProducer::ProduceRenderTaskSetClearFlagAndClearColorBuffer(GL_COLOR_BUFFER_BIT, color.r, color.g, color.b, color.a);
}

void OpenGLRhiDevice::RealSubmitSpriteBatch(const std::vector<Phase1SpriteDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) {
        return;
    }
    current_frame_stats_.sprite_count += static_cast<int>(items.size());

    glm::mat4 vp = projection * view;
    
    RenderTaskProducer::ProduceRenderTaskUseShaderProgram(shader_handle_);
    RenderTaskProducer::ProduceRenderTaskSetEnableState(GL_BLEND, true);
    RenderTaskProducer::ProduceRenderTaskSetBlenderFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    RenderTaskProducer::ProduceRenderTaskSetUniform1i(shader_handle_, "u_texture", 0);
    RenderTaskProducer::ProduceRenderTaskSetUniform3f(shader_handle_, "u_tint", glm::vec3(1.0f, 1.0f, 1.0f));
    RenderTaskProducer::ProduceRenderTaskSetUniformMatrix4fv(shader_handle_, "u_vp", false, vp);

    std::vector<BatchVertex> batch_vertices;
    batch_vertices.reserve(MAX_VERTICES);

    unsigned int current_texture = items[0].texture_handle == 0 ? white_texture_handle_ : items[0].texture_handle;
    
    auto flush_batch = [&]() {
        if (batch_vertices.empty()) {
            return;
        }
        int batch_sprites = static_cast<int>(batch_vertices.size() / 4);
        current_frame_stats_.draw_calls += 1;
        current_frame_stats_.max_batch_sprites = std::max(current_frame_stats_.max_batch_sprites, batch_sprites);
        
        RenderTaskProducer::ProduceRenderTaskUpdateVBOSubData(
            vbo_handle_,
            batch_vertices.size() * sizeof(BatchVertex),
            batch_vertices.data()
        );
        
        RenderTaskProducer::ProduceRenderTaskActiveAndBindTexture(GL_TEXTURE0, current_texture);
        RenderTaskProducer::ProduceRenderTaskBindVAOAndDrawElements(vao_handle_, (batch_vertices.size() / 4) * 6);
        
        batch_vertices.clear();
    };

    const glm::vec4 quad_positions[4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f}
    };
    
    const glm::vec2 quad_uvs[4] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    for (const auto& item : items) {
        unsigned int tex = item.texture_handle == 0 ? white_texture_handle_ : item.texture_handle;
        if (tex != current_texture || batch_vertices.size() + 4 > MAX_VERTICES) {
            flush_batch();
            current_texture = tex;
        }

        // Apply UV rect if provided, else use default quad UVs
        // Assuming item.uv is a rect (x, y, w, h)
        // If uv.z and uv.w are 0, we fallback to default [0,1]
        glm::vec2 uvs[4];
        if (item.uv.z > 0.0f && item.uv.w > 0.0f) {
            uvs[0] = {item.uv.x, item.uv.y};
            uvs[1] = {item.uv.x + item.uv.z, item.uv.y};
            uvs[2] = {item.uv.x + item.uv.z, item.uv.y + item.uv.w};
            uvs[3] = {item.uv.x, item.uv.y + item.uv.w};
        } else {
            for (int i = 0; i < 4; ++i) uvs[i] = quad_uvs[i];
        }

        for (int i = 0; i < 4; ++i) {
            BatchVertex v;
            glm::vec4 world_pos = item.model * quad_positions[i];
            v.pos = glm::vec3(world_pos.x, world_pos.y, world_pos.z);
            v.color = item.color;
            v.uv = uvs[i];
            batch_vertices.push_back(v);
        }
    }
    
    flush_batch();
}

void OpenGLRhiDevice::EndFrame() {
    last_frame_stats_ = current_frame_stats_;
    RenderTaskProducer::ProduceRenderTaskEndFrame();
}

const Phase1RenderStats& OpenGLRhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}
