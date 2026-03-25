#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <functional>
#include <string>

constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_INDICES = MAX_SPRITES * 6;
constexpr unsigned int GL_SRC_ALPHA_CONST = 0x0302;
constexpr unsigned int GL_ONE_MINUS_SRC_ALPHA_CONST = 0x0303;

struct BatchVertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec2 uv;
};

namespace {
unsigned int CompileShaderProgram(const char* vertex_shader_source, const char* fragment_shader_source) {
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    int vertex_compiled = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    if (vertex_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(vertex_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL vertex shader compile failed: {}", log_buffer);
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    int fragment_compiled = 0;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
    if (fragment_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(fragment_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL fragment shader compile failed: {}", log_buffer);
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char log_buffer[1024];
        glGetProgramInfoLog(program, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL shader link failed: {}", log_buffer);
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}
}

unsigned int OpenGLRhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    unsigned int handle = 0;
    glGenBuffers(1, &handle);
    resource_ledger_.buffers_created += 1;
    unsigned int target = is_index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, handle);
    glBufferData(target, size, data, is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    return handle;
}

void OpenGLRhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    unsigned int target = is_index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, handle);
    glBufferSubData(target, offset, size, data);
}

void OpenGLRhiDevice::DeleteBuffer(unsigned int handle) {
    glDeleteBuffers(1, &handle);
    resource_ledger_.buffers_destroyed += 1;
}

unsigned int OpenGLRhiDevice::CreateVertexArray() {
    unsigned int handle = 0;
    glGenVertexArrays(1, &handle);
    resource_ledger_.vertex_arrays_created += 1;
    return handle;
}

void OpenGLRhiDevice::DeleteVertexArray(unsigned int handle) {
    glDeleteVertexArrays(1, &handle);
    resource_ledger_.vertex_arrays_destroyed += 1;
}

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
    shader_handle_ = CompileShaderProgram(vertex_shader, fragment_shader);
    resource_ledger_.shader_programs_created += 1;
    uniform_texture_loc_ = glGetUniformLocation(shader_handle_, "u_texture");
    uniform_tint_loc_ = glGetUniformLocation(shader_handle_, "u_tint");
    uniform_vp_loc_ = glGetUniformLocation(shader_handle_, "u_vp");

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

    vao_handle_ = CreateVertexArray();
    glBindVertexArray(vao_handle_);
    vbo_handle_ = CreateBuffer(vertices.size() * sizeof(BatchVertex), vertices.data(), true, false);
    ebo_handle_ = CreateBuffer(indices.size() * sizeof(unsigned short), indices.data(), false, true);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_handle_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<void*>(offsetof(BatchVertex, uv)));
    glBindVertexArray(0);

    unsigned char white_texture[] = {255, 255, 255, 255};
    glGenTextures(1, &white_texture_handle_);
    resource_ledger_.textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, white_texture_handle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_texture);
    glBindTexture(GL_TEXTURE_2D, 0);
    initialized_ = true;
}

void OpenGLRhiDevice::Shutdown() {
    for (auto& [handle, target] : render_targets_) {
        (void)handle;
        if (target.fbo_handle != 0) {
            glDeleteFramebuffers(1, &target.fbo_handle);
            resource_ledger_.framebuffers_destroyed += 1;
            target.fbo_handle = 0;
        }
        if (target.color_texture_handle != 0) {
            glDeleteTextures(1, &target.color_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            target.color_texture_handle = 0;
        }
        if (target.depth_texture_handle != 0) {
            glDeleteTextures(1, &target.depth_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            target.depth_texture_handle = 0;
        }
        resource_ledger_.render_targets_destroyed += 1;
    }
    render_targets_.clear();
    resource_ledger_.pipeline_states_destroyed += pipeline_states_.size();
    pipeline_states_.clear();
    active_pipeline_state_ = 0;
    active_render_target_ = 0;

    if (white_texture_handle_ != 0) {
        glDeleteTextures(1, &white_texture_handle_);
        resource_ledger_.textures_destroyed += 1;
        white_texture_handle_ = 0;
    }
    if (vbo_handle_ != 0) {
        DeleteBuffer(vbo_handle_);
        vbo_handle_ = 0;
    }
    if (ebo_handle_ != 0) {
        DeleteBuffer(ebo_handle_);
        ebo_handle_ = 0;
    }
    if (vao_handle_ != 0) {
        DeleteVertexArray(vao_handle_);
        vao_handle_ = 0;
    }
    if (shader_handle_ != 0) {
        glDeleteProgram(shader_handle_);
        resource_ledger_.shader_programs_destroyed += 1;
        shader_handle_ = 0;
    }
    uniform_texture_loc_ = -1;
    uniform_tint_loc_ = -1;
    uniform_vp_loc_ = -1;
    LogResourceLedger();
    initialized_ = false;
}

void OpenGLCommandBuffer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    view_ = view;
    projection_ = projection;
}

void OpenGLCommandBuffer::BeginRenderPass(const RenderPassDesc& render_pass) {
    begin_render_pass_cmds_.push_back({next_cmd_order_++, render_pass});
}

void OpenGLCommandBuffer::EndRenderPass() {
    end_render_pass_cmds_.push_back({next_cmd_order_++});
}

void OpenGLCommandBuffer::SetPipelineState(unsigned int pipeline_state_handle) {
    set_pipeline_state_cmds_.push_back({next_cmd_order_++, pipeline_state_handle});
}

void OpenGLCommandBuffer::DrawBatch(const std::vector<DrawBatchItem>& items) {
    DrawBatchCmd cmd;
    cmd.order = next_cmd_order_++;
    cmd.items = items;
    cmd.view = view_;
    cmd.projection = projection_;
    draw_batch_cmds_.push_back(std::move(cmd));
}

void OpenGLCommandBuffer::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) {
    DrawBatch(items);
}

void OpenGLCommandBuffer::ClearColor(const glm::vec4& color) {
    clear_cmds_.push_back({next_cmd_order_++, color});
}

void OpenGLCommandBuffer::Execute(OpenGLRhiDevice* device) {
    if (!device) {
        return;
    }
    std::vector<CommandRef> commands;
    commands.reserve(begin_render_pass_cmds_.size() + set_pipeline_state_cmds_.size() + clear_cmds_.size() + draw_batch_cmds_.size() + end_render_pass_cmds_.size());
    for (size_t i = 0; i < begin_render_pass_cmds_.size(); ++i) {
        commands.push_back({begin_render_pass_cmds_[i].order, 0, i});
    }
    for (size_t i = 0; i < set_pipeline_state_cmds_.size(); ++i) {
        commands.push_back({set_pipeline_state_cmds_[i].order, 1, i});
    }
    for (size_t i = 0; i < clear_cmds_.size(); ++i) {
        commands.push_back({clear_cmds_[i].order, 2, i});
    }
    for (size_t i = 0; i < draw_batch_cmds_.size(); ++i) {
        commands.push_back({draw_batch_cmds_[i].order, 3, i});
    }
    for (size_t i = 0; i < end_render_pass_cmds_.size(); ++i) {
        commands.push_back({end_render_pass_cmds_[i].order, 4, i});
    }
    std::sort(commands.begin(), commands.end(), [](const CommandRef& a, const CommandRef& b) {
        return a.order < b.order;
    });

    for (const auto& cmd : commands) {
        if (cmd.type == 0) {
            device->RealBeginRenderPass(begin_render_pass_cmds_[cmd.index].render_pass);
        } else if (cmd.type == 1) {
            device->RealSetPipelineState(set_pipeline_state_cmds_[cmd.index].pipeline_state_handle);
        } else if (cmd.type == 2) {
            device->RealClearColor(clear_cmds_[cmd.index].color);
        } else if (cmd.type == 3) {
            device->RealSubmitDrawBatch(draw_batch_cmds_[cmd.index].items, draw_batch_cmds_[cmd.index].view, draw_batch_cmds_[cmd.index].projection);
        } else {
            device->RealEndRenderPass();
        }
    }
    begin_render_pass_cmds_.clear();
    end_render_pass_cmds_.clear();
    set_pipeline_state_cmds_.clear();
    clear_cmds_.clear();
    draw_batch_cmds_.clear();
    next_cmd_order_ = 0;
}

void OpenGLRhiDevice::BeginFrame() {
    EnsureInitialized();
    current_frame_stats_ = {};
}

unsigned int OpenGLRhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    unsigned int handle = ++next_render_target_handle_;
    unsigned int color_texture_handle = 0;
    glGenTextures(1, &color_texture_handle);
    resource_ledger_.textures_created += 1;
    unsigned int depth_texture_handle = 0;
    if (desc.has_depth) {
        glGenTextures(1, &depth_texture_handle);
        resource_ledger_.textures_created += 1;
    }
    unsigned int fbo_handle = 0;
    glGenFramebuffers(1, &fbo_handle);
    resource_ledger_.framebuffers_created += 1;
    glBindTexture(GL_TEXTURE_2D, color_texture_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, desc.width, desc.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (desc.has_depth) {
        glBindTexture(GL_TEXTURE_2D, depth_texture_handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, desc.width, desc.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_handle, 0);
    if (desc.has_depth) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_handle, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    render_targets_[handle] = {desc, fbo_handle, color_texture_handle, depth_texture_handle};
    resource_ledger_.render_targets_created += 1;
    return handle;
}

unsigned int OpenGLRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    auto it = render_targets_.find(render_target_handle);
    if (it == render_targets_.end()) {
        return 0;
    }
    return it->second.color_texture_handle;
}

unsigned int OpenGLRhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_ledger_.textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, texture_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_handle;
}

unsigned int OpenGLRhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    unsigned int shader_program = CompileShaderProgram(vert_src.c_str(), frag_src.c_str());
    resource_ledger_.shader_programs_created += 1;
    return shader_program;
}

unsigned int OpenGLRhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    unsigned int handle = ++next_pipeline_state_handle_;
    pipeline_states_[handle] = desc;
    resource_ledger_.pipeline_states_created += 1;
    return handle;
}

std::shared_ptr<CommandBuffer> OpenGLRhiDevice::CreateCommandBuffer() {
    return std::make_shared<OpenGLCommandBuffer>();
}

void OpenGLRhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    auto gl_cmd = std::dynamic_pointer_cast<OpenGLCommandBuffer>(cmd_buffer);
    if (gl_cmd) {
        gl_cmd->Execute(this);
    }
}

void OpenGLRhiDevice::RealClearColor(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRhiDevice::RealBeginRenderPass(const RenderPassDesc& render_pass) {
    if (render_pass.render_target == 0) {
        if (active_render_target_ != 0) {
            auto active_it = render_targets_.find(active_render_target_);
            if (active_it != render_targets_.end()) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
        active_render_target_ = 0;
        glViewport(0, 0, Screen::width(), Screen::height());
    } else {
        auto it = render_targets_.find(render_pass.render_target);
        if (it != render_targets_.end()) {
            if (active_render_target_ != 0 && active_render_target_ != render_pass.render_target) {
                auto active_it = render_targets_.find(active_render_target_);
                if (active_it != render_targets_.end()) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, it->second.fbo_handle);
            glViewport(0, 0, it->second.desc.width, it->second.desc.height);
            active_render_target_ = render_pass.render_target;
        }
    }
    current_frame_stats_.render_passes += 1;
    if (render_pass.clear_color_enabled) {
        RealClearColor(render_pass.clear_color);
    }
}

void OpenGLRhiDevice::RealEndRenderPass() {
}

void OpenGLRhiDevice::RealSetPipelineState(unsigned int pipeline_state_handle) {
    active_pipeline_state_ = pipeline_state_handle;
    auto it = pipeline_states_.find(pipeline_state_handle);
    if (it == pipeline_states_.end()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA_CONST, GL_ONE_MINUS_SRC_ALPHA_CONST);
        return;
    }
    if (it->second.blend_enabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    if (it->second.blend_enabled) {
        glBlendFunc(it->second.blend_src, it->second.blend_dst);
    }
}

void OpenGLRhiDevice::RealSubmitDrawBatch(const std::vector<DrawBatchItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) {
        return;
    }
    current_frame_stats_.sprite_count += static_cast<int>(items.size());

    glm::mat4 vp = projection * view;
    
    glUseProgram(shader_handle_);
    if (active_pipeline_state_ != 0) {
        RealSetPipelineState(active_pipeline_state_);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform1i(uniform_texture_loc_, 0);
    glUniform3f(uniform_tint_loc_, 1.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(uniform_vp_loc_, 1, GL_FALSE, &vp[0][0]);

    std::vector<BatchVertex> batch_vertices;
    batch_vertices.reserve(MAX_VERTICES);

    unsigned int current_texture = items[0].texture_handle == 0 ? white_texture_handle_ : items[0].texture_handle;
    unsigned int current_material_instance = items[0].material_instance_id;
    unsigned int current_shader_variant = items[0].shader_variant_key;
    unsigned int current_blend_mode = items[0].blend_mode;
    const unsigned int additive_variant_key = static_cast<unsigned int>(std::hash<std::string>{}("SPRITE_ADDITIVE"));
    auto apply_blend = [&](unsigned int blend_mode, unsigned int shader_variant_key) {
        glEnable(GL_BLEND);
        if (blend_mode == 1 || shader_variant_key == additive_variant_key) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            return;
        }
        if (blend_mode == 2) {
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            return;
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    };
    apply_blend(current_blend_mode, current_shader_variant);
    
    auto flush_batch = [&]() {
        if (batch_vertices.empty()) {
            return;
        }
        int batch_sprites = static_cast<int>(batch_vertices.size() / 4);
        current_frame_stats_.draw_calls += 1;
        current_frame_stats_.max_batch_sprites = std::max(current_frame_stats_.max_batch_sprites, batch_sprites);
        
        UpdateBuffer(vbo_handle_, 0, batch_vertices.size() * sizeof(BatchVertex), batch_vertices.data(), false);
        
        apply_blend(current_blend_mode, current_shader_variant);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_texture);
        glBindVertexArray(vao_handle_);
        glDrawElements(GL_TRIANGLES, static_cast<int>((batch_vertices.size() / 4) * 6), GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);
        
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
        if (tex != current_texture ||
            item.material_instance_id != current_material_instance ||
            item.shader_variant_key != current_shader_variant ||
            item.blend_mode != current_blend_mode ||
            batch_vertices.size() + 4 > MAX_VERTICES) {
            flush_batch();
            current_texture = tex;
            current_material_instance = item.material_instance_id;
            current_shader_variant = item.shader_variant_key;
            current_blend_mode = item.blend_mode;
        }

        // Apply UV rect if provided, else use default quad UVs
        // Assuming item.uv is a rect (x, y, w, h)
        // If uv.z and uv.w are 0, we fallback to default [0,1]
        glm::vec2 uvs[4];
        if (item.uv.z > 0.0f && item.uv.w > 0.0f) {
            const bool use_max_uv = item.uv.z > item.uv.x && item.uv.w > item.uv.y;
            const float u1 = use_max_uv ? item.uv.z : (item.uv.x + item.uv.z);
            const float v1 = use_max_uv ? item.uv.w : (item.uv.y + item.uv.w);
            uvs[0] = {item.uv.x, item.uv.y};
            uvs[1] = {u1, item.uv.y};
            uvs[2] = {u1, v1};
            uvs[3] = {item.uv.x, v1};
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
    glFlush();
}

const RenderStats& OpenGLRhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

void OpenGLRhiDevice::LogResourceLedger() const {
    const std::size_t live_textures = resource_ledger_.textures_created - resource_ledger_.textures_destroyed;
    const std::size_t live_fbos = resource_ledger_.framebuffers_created - resource_ledger_.framebuffers_destroyed;
    const std::size_t live_programs = resource_ledger_.shader_programs_created - resource_ledger_.shader_programs_destroyed;
    const std::size_t live_vaos = resource_ledger_.vertex_arrays_created - resource_ledger_.vertex_arrays_destroyed;
    const std::size_t live_buffers = resource_ledger_.buffers_created - resource_ledger_.buffers_destroyed;
    const std::size_t live_targets = resource_ledger_.render_targets_created - resource_ledger_.render_targets_destroyed;
    const std::size_t live_pipelines = resource_ledger_.pipeline_states_created - resource_ledger_.pipeline_states_destroyed;
    DEBUG_LOG_INFO("RHI resource ledger: textures={}/{}, framebuffers={}/{}, shader_programs={}/{}, vaos={}/{}, buffers={}/{}, render_targets={}/{}, pipeline_states={}/{}",
                   resource_ledger_.textures_destroyed, resource_ledger_.textures_created,
                   resource_ledger_.framebuffers_destroyed, resource_ledger_.framebuffers_created,
                   resource_ledger_.shader_programs_destroyed, resource_ledger_.shader_programs_created,
                   resource_ledger_.vertex_arrays_destroyed, resource_ledger_.vertex_arrays_created,
                   resource_ledger_.buffers_destroyed, resource_ledger_.buffers_created,
                   resource_ledger_.render_targets_destroyed, resource_ledger_.render_targets_created,
                   resource_ledger_.pipeline_states_destroyed, resource_ledger_.pipeline_states_created);
    if (live_textures != 0 || live_fbos != 0 || live_programs != 0 || live_vaos != 0 || live_buffers != 0 || live_targets != 0 || live_pipelines != 0) {
        DEBUG_LOG_WARN("RHI resource ledger leak suspect: live_textures={}, live_fbos={}, live_programs={}, live_vaos={}, live_buffers={}, live_targets={}, live_pipelines={}",
                       live_textures, live_fbos, live_programs, live_vaos, live_buffers, live_targets, live_pipelines);
    }
}
