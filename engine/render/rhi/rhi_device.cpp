/**
 * @file rhi_device.cpp
 * @brief 渲染硬件接口(RHI)抽象层 - OpenGLRhiDevice 协调器实现
 *
 * OpenGLRhiDevice 作为协调器持有四个子系统并委托调用：
 * - GLResourceManager：GPU 资源创建/销毁/查询
 * - GLPipelineStateManager：渲染状态缓存与应用
 * - GLShaderManager：着色器编译/链接/Uniform 缓存
 * - GLDrawExecutor：绘制命令执行
 */

#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"
#include <glad/gl.h>

// GL 4.3 SSBO / Compute 常量 — glad/gl.h 仅包含 GL 3.3 定义
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif
#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif
#ifndef GL_TEXTURE_FETCH_BARRIER_BIT
#define GL_TEXTURE_FETCH_BARRIER_BIT 0x00000008
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_READ_WRITE
#define GL_READ_WRITE 0x88BA
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif

// GL 4.2+/4.3 函数指针（glad 仅加载 GL 3.3 core，需手动解析）
#ifdef _WIN32
extern "C" __declspec(dllimport) void* __stdcall wglGetProcAddress(const char*);
#endif

static void(GLAD_API_PTR* pfn_glDispatchCompute)(GLuint, GLuint, GLuint) = nullptr;
static void(GLAD_API_PTR* pfn_glMemoryBarrier)(GLbitfield) = nullptr;
static void(GLAD_API_PTR* pfn_glBindImageTexture)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum) = nullptr;

static void InitComputeProcAddresses() {
    static bool done = false;
    if (done) return;
    done = true;
#ifdef _WIN32
    pfn_glDispatchCompute = reinterpret_cast<decltype(pfn_glDispatchCompute)>(wglGetProcAddress("glDispatchCompute"));
    pfn_glMemoryBarrier = reinterpret_cast<decltype(pfn_glMemoryBarrier)>(wglGetProcAddress("glMemoryBarrier"));
    pfn_glBindImageTexture = reinterpret_cast<decltype(pfn_glBindImageTexture)>(wglGetProcAddress("glBindImageTexture"));
#endif
}

// BCn / S3TC / BPTC 压缩纹理格式常量
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT            0x83F0
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT           0x8C4C
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT           0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT           0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT     0x8C4F
#endif
#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1                    0x8DBB
#endif
#ifndef GL_COMPRESSED_RG_RGTC2
#define GL_COMPRESSED_RG_RGTC2                     0x8DBD
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
#define GL_COMPRESSED_RGBA_BPTC_UNORM_ARB          0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB    0x8E8D
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>
#include <algorithm>
#include <functional>
#include <string>

// ============================================================
// OpenGLCommandBuffer — 命令记录与回放（无状态，纯委托到 device）
// ============================================================

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

void OpenGLCommandBuffer::SetGlobalMat4(const std::string& name, const glm::mat4& value) {
    set_global_mat4_cmds_.push_back({next_cmd_order_++, name, value});
}

void OpenGLCommandBuffer::SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) {
    set_global_mat4_array_cmds_.push_back({next_cmd_order_++, name, values});
}

void OpenGLCommandBuffer::SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) {
    set_global_float_array_cmds_.push_back({next_cmd_order_++, name, values});
}

void OpenGLCommandBuffer::DrawBatch(const std::vector<DrawBatchItem>& items) {
    DrawBatchCmd cmd;
    cmd.order = next_cmd_order_++;
    cmd.items = items;
    cmd.view = view_;
    cmd.projection = projection_;
    draw_batch_cmds_.push_back(std::move(cmd));
}

void OpenGLCommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (items.empty()) return;
    draw_mesh_batch_cmds_.push_back({next_cmd_order_++, items, view_, projection_});
}

void OpenGLCommandBuffer::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) {
    DrawBatch(items);
}

void OpenGLCommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    draw_skybox_cmds_.push_back({next_cmd_order_++, cubemap_texture_handle, view_, projection_});
}

void OpenGLCommandBuffer::ClearColor(const glm::vec4& color) {
    clear_cmds_.push_back({next_cmd_order_++, color});
}

void OpenGLCommandBuffer::DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) {
    draw_post_process_cmds_.push_back({next_cmd_order_++, source_texture, effect_name, params});
}

void OpenGLCommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) return;
    draw_particles3d_cmds_.push_back({next_cmd_order_++, items, view, projection});
}

void OpenGLCommandBuffer::DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) {
    defer_shadow_map_cmds_.push_back({next_cmd_order_++, index, texture_handle, 0});
}

void OpenGLCommandBuffer::DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) {
    defer_shadow_map_cmds_.push_back({next_cmd_order_++, index, texture_handle, 1});
}

void OpenGLCommandBuffer::DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) {
    defer_shadow_map_cmds_.push_back({next_cmd_order_++, index, texture_handle, 2});
}

void OpenGLCommandBuffer::AppendFrom(OpenGLCommandBuffer& other) {
    const uint64_t offset = next_cmd_order_;
    auto rebase = [offset](uint64_t order) { return order + offset; };

    for (auto& c : other.begin_render_pass_cmds_)   { c.order = rebase(c.order); begin_render_pass_cmds_.push_back(std::move(c)); }
    for (auto& c : other.end_render_pass_cmds_)     { c.order = rebase(c.order); end_render_pass_cmds_.push_back(std::move(c)); }
    for (auto& c : other.set_pipeline_state_cmds_)   { c.order = rebase(c.order); set_pipeline_state_cmds_.push_back(std::move(c)); }
    for (auto& c : other.set_global_mat4_cmds_)      { c.order = rebase(c.order); set_global_mat4_cmds_.push_back(std::move(c)); }
    for (auto& c : other.set_global_mat4_array_cmds_){ c.order = rebase(c.order); set_global_mat4_array_cmds_.push_back(std::move(c)); }
    for (auto& c : other.set_global_float_array_cmds_){ c.order = rebase(c.order); set_global_float_array_cmds_.push_back(std::move(c)); }
    for (auto& c : other.clear_cmds_)               { c.order = rebase(c.order); clear_cmds_.push_back(std::move(c)); }
    for (auto& c : other.draw_batch_cmds_)           { c.order = rebase(c.order); draw_batch_cmds_.push_back(std::move(c)); }
    for (auto& c : other.draw_mesh_batch_cmds_)      { c.order = rebase(c.order); draw_mesh_batch_cmds_.push_back(std::move(c)); }
    for (auto& c : other.draw_skybox_cmds_)          { c.order = rebase(c.order); draw_skybox_cmds_.push_back(std::move(c)); }
    for (auto& c : other.draw_post_process_cmds_)    { c.order = rebase(c.order); draw_post_process_cmds_.push_back(std::move(c)); }
    for (auto& c : other.draw_particles3d_cmds_)     { c.order = rebase(c.order); draw_particles3d_cmds_.push_back(std::move(c)); }
    for (auto& c : other.defer_shadow_map_cmds_)     { c.order = rebase(c.order); defer_shadow_map_cmds_.push_back(std::move(c)); }

    next_cmd_order_ = offset + other.next_cmd_order_;
    other.Reset();
}

void OpenGLCommandBuffer::Reset() {
    begin_render_pass_cmds_.clear();
    end_render_pass_cmds_.clear();
    set_pipeline_state_cmds_.clear();
    set_global_mat4_cmds_.clear();
    set_global_mat4_array_cmds_.clear();
    set_global_float_array_cmds_.clear();
    clear_cmds_.clear();
    draw_batch_cmds_.clear();
    draw_mesh_batch_cmds_.clear();
    draw_skybox_cmds_.clear();
    draw_post_process_cmds_.clear();
    draw_particles3d_cmds_.clear();
    defer_shadow_map_cmds_.clear();
    next_cmd_order_ = 0;
}

void OpenGLCommandBuffer::Execute(OpenGLRhiDevice* device) {
    if (!device) {
        return;
    }

    // 收集所有命令并按提交顺序排序
    std::vector<CommandRef> commands;
    commands.reserve(begin_render_pass_cmds_.size() + set_pipeline_state_cmds_.size() + clear_cmds_.size() + draw_batch_cmds_.size() + draw_mesh_batch_cmds_.size() + end_render_pass_cmds_.size());
    for (size_t i = 0; i < begin_render_pass_cmds_.size(); ++i) {
        commands.push_back({begin_render_pass_cmds_[i].order, 0, i});
    }
    for (size_t i = 0; i < set_pipeline_state_cmds_.size(); ++i) {
        commands.push_back({set_pipeline_state_cmds_[i].order, 1, i});
    }
    for (size_t i = 0; i < set_global_mat4_cmds_.size(); ++i) {
        commands.push_back({set_global_mat4_cmds_[i].order, 8, i});
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
    for (size_t i = 0; i < draw_mesh_batch_cmds_.size(); ++i) {
        commands.push_back({draw_mesh_batch_cmds_[i].order, 5, i});
    }
    for (size_t i = 0; i < draw_skybox_cmds_.size(); ++i) {
        commands.push_back({draw_skybox_cmds_[i].order, 7, i});
    }
    for (size_t i = 0; i < set_global_mat4_array_cmds_.size(); ++i) {
        commands.push_back({set_global_mat4_array_cmds_[i].order, 9, i});
    }
    for (size_t i = 0; i < set_global_float_array_cmds_.size(); ++i) {
        commands.push_back({set_global_float_array_cmds_[i].order, 10, i});
    }
    for (size_t i = 0; i < draw_post_process_cmds_.size(); ++i) {
        commands.push_back({draw_post_process_cmds_[i].order, 11, i});
    }
    for (size_t i = 0; i < draw_particles3d_cmds_.size(); ++i) {
        commands.push_back({draw_particles3d_cmds_[i].order, 12, i});
    }
    for (size_t i = 0; i < defer_shadow_map_cmds_.size(); ++i) {
        commands.push_back({defer_shadow_map_cmds_[i].order, 13, i});
    }
    std::sort(commands.begin(), commands.end(), [](const CommandRef& a, const CommandRef& b) {
        return a.order < b.order;
    });

    // 按顺序回放到 device
    for (const auto& cmd : commands) {
        if (cmd.type == 0) {
            device->RealBeginRenderPass(begin_render_pass_cmds_[cmd.index].render_pass);
        } else if (cmd.type == 1) {
            device->RealSetPipelineState(set_pipeline_state_cmds_[cmd.index].pipeline_state_handle);
        } else if (cmd.type == 8) {
            const auto& mat_cmd = set_global_mat4_cmds_[cmd.index];
            if (mat_cmd.name == "u_spot_light_space_matrix") {
                device->SetGlobalSpotLightSpaceMatrix(mat_cmd.value);
            } else if (mat_cmd.name == "u_light_space_matrix") {
                device->SetGlobalLightSpaceMatrix(0, mat_cmd.value);
            }
        } else if (cmd.type == 9) {
            const auto& mat_cmd = set_global_mat4_array_cmds_[cmd.index];
            if (mat_cmd.name == "u_light_space_matrices") {
                for(size_t j=0; j<3 && j<mat_cmd.values.size(); ++j) {
                    device->SetGlobalLightSpaceMatrix(static_cast<unsigned int>(j), mat_cmd.values[j]);
                }
            } else if (mat_cmd.name == "u_spot_light_space_matrices") {
                for(size_t j=0; j<4 && j<mat_cmd.values.size(); ++j) {
                    device->SetGlobalSpotLightSpaceMatrix(static_cast<unsigned int>(j), mat_cmd.values[j]);
                }
            }
        } else if (cmd.type == 10) {
            const auto& mat_cmd = set_global_float_array_cmds_[cmd.index];
            if (mat_cmd.name == "u_cascade_splits") {
                for(size_t j=0; j<3 && j<mat_cmd.values.size(); ++j) {
                    device->SetGlobalCascadeSplit(static_cast<unsigned int>(j), mat_cmd.values[j]);
                }
            }
        } else if (cmd.type == 2) {
            device->RealClearColor(clear_cmds_[cmd.index].color);
        } else if (cmd.type == 3) {
            device->RealSubmitDrawBatch(draw_batch_cmds_[cmd.index].items, draw_batch_cmds_[cmd.index].view, draw_batch_cmds_[cmd.index].projection);
        } else if (cmd.type == 4) {
            device->RealEndRenderPass();
        } else if (cmd.type == 5) {
            device->RealSubmitDrawMeshBatch(draw_mesh_batch_cmds_[cmd.index].items, draw_mesh_batch_cmds_[cmd.index].view, draw_mesh_batch_cmds_[cmd.index].projection);
        } else if (cmd.type == 7) {
            device->RealSubmitDrawSkybox(draw_skybox_cmds_[cmd.index].cubemap_texture_handle, draw_skybox_cmds_[cmd.index].view, draw_skybox_cmds_[cmd.index].projection);
        } else if (cmd.type == 11) {
            device->RealSubmitDrawPostProcess(draw_post_process_cmds_[cmd.index].source_texture, draw_post_process_cmds_[cmd.index].effect_name, draw_post_process_cmds_[cmd.index].params);
        } else if (cmd.type == 12) {
            device->RealSubmitDrawParticles3D(draw_particles3d_cmds_[cmd.index].items, draw_particles3d_cmds_[cmd.index].view, draw_particles3d_cmds_[cmd.index].projection);
        } else if (cmd.type == 13) {
            const auto& sc = defer_shadow_map_cmds_[cmd.index];
            if (sc.shadow_type == 0) {
                device->SetGlobalShadowMap(sc.index, sc.texture_handle);
            } else if (sc.shadow_type == 1) {
                device->SetGlobalSpotShadowMap(sc.index, sc.texture_handle);
            } else if (sc.shadow_type == 2) {
                device->SetGlobalPointShadowMap(sc.index, sc.texture_handle);
            }
        }
    }
    Reset();
}

// ============================================================
// OpenGLRhiDevice — 协调器，委托到子系统
// ============================================================

void OpenGLRhiDevice::EnsureInitialized() {
    if (initialized_) {
        return;
    }

    // 注入函数指针到 DrawExecutor（用于缓冲区操作回调）
    draw_executor_.set_create_vao_fn([this]() -> unsigned int {
        return CreateVertexArray();
    });
    draw_executor_.set_create_buffer_fn([this](size_t size, const void* data, bool is_dynamic, bool is_index) -> unsigned int {
        return CreateBuffer(size, data, is_dynamic, is_index);
    });
    draw_executor_.set_update_buffer_fn([this](unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
        UpdateBuffer(handle, offset, size, data, is_index);
    });
    draw_executor_.set_delete_vao_fn([this](unsigned int handle) {
        DeleteVertexArray(handle);
    });
    draw_executor_.set_delete_buffer_fn([this](unsigned int handle) {
        DeleteBuffer(handle);
    });
    draw_executor_.set_delete_texture_fn([this](unsigned int handle) {
        DeleteTexture(handle);
    });
    draw_executor_.set_create_texture_fn([this](int w, int h, const unsigned char* data, bool linear) -> unsigned int {
        return CreateTexture2D(w, h, data, linear);
    });

    // 检测 SSBO 支持（需要 GL 4.3+）
    GLint gl_major = 0, gl_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
    supports_ssbo_ = (gl_major > 4) || (gl_major == 4 && gl_minor >= 3);
    shader_mgr_.set_supports_ssbo(supports_ssbo_);

    // 初始化内置 PBR 着色器
    shader_mgr_.InitBuiltinPBRShader();
    resource_mgr_.ledger().shader_programs_created += 1;

    // 初始化 UBO 管理器
    ubo_mgr_.Init();

    // 初始化几何缓冲区（2D 精灵 + 3D 网格 + 白色纹理）
    draw_executor_.InitGeometryBuffers(
        [this]() -> unsigned int { return CreateVertexArray(); },
        [this](size_t size, const void* data, bool is_dynamic, bool is_index) -> unsigned int { return CreateBuffer(size, data, is_dynamic, is_index); },
        [this](unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) { UpdateBuffer(handle, offset, size, data, is_index); }
    );

    initialized_ = true;
}

void OpenGLRhiDevice::Shutdown() {
    if (!initialized_) return;

    // 清理渲染目标（由 ResourceManager 管理 GL 对象生命周期）
    resource_mgr_.DestroyAllRenderTargets();

    // 清理外部创建的着色器程序（通过 CreateShaderProgram 创建、由 AssetManager 持有）
    // 基于 ledger 差值清理：如果 created > destroyed，说明有外部 shader 未被删除
    {
        const std::size_t live_external = resource_mgr_.ledger().shader_programs_created
                                        - resource_mgr_.ledger().shader_programs_destroyed;
        // shader_mgr_ 管理的着色器由 Shutdown 自行销毁，这里只处理 external 部分
        if (live_external > 0 && !external_shader_programs_.empty()) {
            for (auto handle : external_shader_programs_) {
                glDeleteProgram(handle);
                resource_mgr_.ledger().shader_programs_destroyed += 1;
            }
            external_shader_programs_.clear();
        } else if (live_external > 0) {
            // external_shader_programs_ 为空但 ledger 不平衡——可能 AssetManager 已提前释放
            // 此时 GL program 已由 AssetManager::ReleaseGpuResources 通过 glDeleteProgram 释放，
            // 但 ledger 计数未更新（AssetManager 调用 DeleteShaderProgram 时 rhi_device 可能已不可用）
            // 直接补齐账本差值
            resource_mgr_.ledger().shader_programs_destroyed += live_external;
        }
    }

    // 清理 compute shader programs
    for (auto handle : compute_programs_) {
        glDeleteProgram(handle);
    }
    compute_programs_.clear();

    // 清理子系统（逆序）
    draw_executor_.ShutdownGeometryBuffers();
    ubo_mgr_.Shutdown();
    shader_mgr_.Shutdown();
    // Pipeline state 无 GL 资源，仅更新账本
    resource_mgr_.ledger().pipeline_states_destroyed += state_mgr_.pipeline_state_count();
    state_mgr_.Shutdown();

    LogResourceLedger();
    initialized_ = false;
}

// --- 帧生命周期 ---

void OpenGLRhiDevice::BeginFrame() {
    EnsureInitialized();
    draw_executor_.BeginFrame();
}

void OpenGLRhiDevice::EndFrame() {
    draw_executor_.EndFrame();
}

const RenderStats& OpenGLRhiDevice::LastFrameStats() const {
    return draw_executor_.last_frame_stats();
}

// --- 缓冲区 ---

unsigned int OpenGLRhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    unsigned int handle = 0;
    glGenBuffers(1, &handle);
    resource_mgr_.ledger().buffers_created += 1;
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
    resource_mgr_.ledger().buffers_destroyed += 1;
}

// --- 顶点数组 ---

unsigned int OpenGLRhiDevice::CreateVertexArray() {
    unsigned int handle = 0;
    glGenVertexArrays(1, &handle);
    resource_mgr_.ledger().vertex_arrays_created += 1;
    return handle;
}

void OpenGLRhiDevice::DeleteVertexArray(unsigned int handle) {
    glDeleteVertexArrays(1, &handle);
    resource_mgr_.ledger().vertex_arrays_destroyed += 1;
}

// --- 纹理 ---

unsigned int OpenGLRhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_mgr_.ledger().textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, texture_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_handle;
}

unsigned int OpenGLRhiDevice::CreateCompressedTexture2D(CompressedTextureFormat format,
                                                         const std::vector<CompressedMipLevel>& mips,
                                                         bool linear_filter) {
    if (mips.empty()) return 0;

    GLenum gl_format = 0;
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM: gl_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
        case CompressedTextureFormat::BC1_SRGB:  gl_format = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT; break;
        case CompressedTextureFormat::BC2_UNORM: gl_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
        case CompressedTextureFormat::BC3_UNORM: gl_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
        case CompressedTextureFormat::BC3_SRGB:  gl_format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT; break;
        case CompressedTextureFormat::BC4_UNORM: gl_format = GL_COMPRESSED_RED_RGTC1; break;
        case CompressedTextureFormat::BC5_UNORM: gl_format = GL_COMPRESSED_RG_RGTC2; break;
        case CompressedTextureFormat::BC7_UNORM: gl_format = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB; break;
        case CompressedTextureFormat::BC7_SRGB:  gl_format = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB; break;
        default: return 0;
    }

    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_mgr_.ledger().textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, texture_handle);

    bool has_mips = mips.size() > 1;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, has_mips ? (linear_filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) : (linear_filter ? GL_LINEAR : GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(mips.size() - 1));

    for (size_t i = 0; i < mips.size(); ++i) {
        glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i), gl_format,
                               mips[i].width, mips[i].height, 0,
                               static_cast<GLsizei>(mips[i].size), mips[i].data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_handle;
}

unsigned int OpenGLRhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    if (width <= 0 || height <= 0) {
        return 0;
    }
    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_mgr_.ledger().textures_created += 1;
    if (texture_handle == 0) {
        return 0;
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_handle);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    for (int face = 0; face < 6; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_faces[face]);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return texture_handle;
}

unsigned int OpenGLRhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    if (width <= 0 || height <= 0 || depth <= 0) return 0;
    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_mgr_.ledger().textures_created += 1;
    if (texture_handle == 0) return 0;
    glBindTexture(GL_TEXTURE_3D, texture_handle);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_data);
    glBindTexture(GL_TEXTURE_3D, 0);
    return texture_handle;
}

void OpenGLRhiDevice::DeleteTexture(unsigned int texture_handle) {
    if (texture_handle == 0) {
        return;
    }
    glDeleteTextures(1, &texture_handle);
    resource_mgr_.ledger().textures_destroyed += 1;
}

// --- 渲染目标 ---

unsigned int OpenGLRhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    unsigned int handle = resource_mgr_.AllocateRenderTargetHandle();
    unsigned int depth_texture_handle = 0;
    unsigned int fbo_handle = 0;

    const int num_color = desc.has_color ? (std::max)(1, desc.color_attachment_count) : 0;
    std::vector<unsigned int> color_handles(static_cast<size_t>(num_color), 0);

    auto cleanup_failed_rt = [&]() {
        if (fbo_handle != 0) {
            glDeleteFramebuffers(1, &fbo_handle);
            resource_mgr_.ledger().framebuffers_destroyed += 1;
            fbo_handle = 0;
        }
        if (depth_texture_handle != 0) {
            glDeleteTextures(1, &depth_texture_handle);
            resource_mgr_.ledger().textures_destroyed += 1;
            depth_texture_handle = 0;
        }
        for (auto& ch : color_handles) {
            if (ch != 0) {
                glDeleteTextures(1, &ch);
                resource_mgr_.ledger().textures_destroyed += 1;
                ch = 0;
            }
        }
    };

    for (int ci = 0; ci < num_color; ++ci) {
        glGenTextures(1, &color_handles[ci]);
        resource_mgr_.ledger().textures_created += 1;
        if (color_handles[ci] == 0) {
            DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: glGenTextures returned 0 for color attachment {} ({}x{})",
                ci, desc.width, desc.height);
            cleanup_failed_rt();
            return 0;
        }
    }

    if (desc.has_depth) {
        glGenTextures(1, &depth_texture_handle);
        resource_mgr_.ledger().textures_created += 1;
        if (depth_texture_handle == 0) {
            DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: glGenTextures returned 0 for depth attachment ({}x{})",
                desc.width, desc.height);
            cleanup_failed_rt();
            return 0;
        }
    }

    glGenFramebuffers(1, &fbo_handle);
    resource_mgr_.ledger().framebuffers_created += 1;
    if (fbo_handle == 0) {
        DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: glGenFramebuffers returned 0 ({}x{})",
            desc.width, desc.height);
        cleanup_failed_rt();
        return 0;
    }

    for (int ci = 0; ci < num_color; ++ci) {
        glBindTexture(GL_TEXTURE_2D, color_handles[ci]);
        GLint internal_format = GL_RGBA16F;
        GLenum type = GL_FLOAT;
        if (desc.generate_mipmaps) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, desc.width, desc.height, 0, GL_RGBA, type, nullptr);
        if (desc.generate_mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    if (desc.has_depth) {
        if (desc.cube_map) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, depth_texture_handle);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24, desc.width, desc.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            }
        } else {
            glBindTexture(GL_TEXTURE_2D, depth_texture_handle);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, desc.width, desc.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle);
    for (int ci = 0; ci < num_color; ++ci) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + ci, GL_TEXTURE_2D, color_handles[ci], 0);
    }
    if (num_color > 1) {
        std::vector<GLenum> draw_bufs(static_cast<size_t>(num_color));
        for (int ci = 0; ci < num_color; ++ci)
            draw_bufs[ci] = GL_COLOR_ATTACHMENT0 + ci;
        glDrawBuffers(num_color, draw_bufs.data());
    }
    if (desc.has_depth) {
        if (desc.cube_map) {
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_texture_handle, 0);
            if (num_color == 0) {
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            }
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_handle, 0);
        }
    }

    const GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
        DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: framebuffer incomplete, status=0x{:x} ({}x{}, color_count={}, depth={}, cube={})",
            static_cast<unsigned int>(framebuffer_status), desc.width, desc.height, num_color, desc.has_depth, desc.cube_map);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        cleanup_failed_rt();
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    dse::render::RenderTargetResource rt{};
    rt.desc = desc;
    rt.fbo_handle = fbo_handle;
    rt.color_texture_handle = num_color > 0 ? color_handles[0] : 0;
    rt.depth_texture_handle = depth_texture_handle;
    rt.color_texture_handles = std::move(color_handles);
    resource_mgr_.StoreRenderTarget(handle, rt);
    return handle;
}

unsigned int OpenGLRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    return rt ? rt->color_texture_handle : 0;
}

unsigned int OpenGLRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
    auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    if (!rt) return 0;
    if (index < 0 || index >= static_cast<int>(rt->color_texture_handles.size())) return 0;
    return rt->color_texture_handles[index];
}

unsigned int OpenGLRhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    return rt ? rt->depth_texture_handle : 0;
}

std::vector<unsigned char> OpenGLRhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    return ReadRenderTargetColorRgba8WithSize(render_target_handle).pixels;
}

RenderTargetReadback OpenGLRhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    if (!rt || !rt->desc.has_color || rt->desc.width <= 0 || rt->desc.height <= 0) {
        return {};
    }

    RenderTargetReadback readback;
    readback.width = rt->desc.width;
    readback.height = rt->desc.height;
    readback.pixels.resize(static_cast<std::size_t>(rt->desc.width) * static_cast<std::size_t>(rt->desc.height) * 4u, 0u);

    GLint previous_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo_handle);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, rt->desc.width, rt->desc.height, GL_RGBA, GL_UNSIGNED_BYTE, readback.pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));

    return readback;
}

// --- 着色器 ---

unsigned int OpenGLRhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    unsigned int shader_program = dse::render::GLShaderManager::CompileProgram(vert_src.c_str(), frag_src.c_str());
    resource_mgr_.ledger().shader_programs_created += 1;
    external_shader_programs_.insert(shader_program);
    return shader_program;
}

void OpenGLRhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    shader_mgr_.DeleteProgram(program_handle);
    resource_mgr_.ledger().shader_programs_destroyed += 1;
    external_shader_programs_.erase(program_handle);
}

// --- 管线状态 ---

unsigned int OpenGLRhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    unsigned int handle = state_mgr_.CreatePipelineState(desc);
    resource_mgr_.ledger().pipeline_states_created += 1;
    return handle;
}

// --- 命令缓冲 ---

std::shared_ptr<CommandBuffer> OpenGLRhiDevice::CreateCommandBuffer() {
    return std::make_shared<OpenGLCommandBuffer>();
}

void OpenGLRhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    auto gl_cmd = std::dynamic_pointer_cast<OpenGLCommandBuffer>(cmd_buffer);
    if (gl_cmd) {
        gl_cmd->Execute(this);
    }
}

// --- Real* 方法（由 OpenGLCommandBuffer::Execute 回调，委托到子系统） ---

void OpenGLRhiDevice::RealClearColor(const glm::vec4& color) {
    draw_executor_.ClearColor(color);
}

void OpenGLRhiDevice::RealBeginRenderPass(const RenderPassDesc& render_pass) {
    draw_executor_.BeginRenderPass(render_pass, resource_mgr_);
}

void OpenGLRhiDevice::RealEndRenderPass() {
    draw_executor_.EndRenderPass(resource_mgr_);
}

void OpenGLRhiDevice::RealSetPipelineState(unsigned int pipeline_state_handle) {
    state_mgr_.ApplyState(pipeline_state_handle);
}

void OpenGLRhiDevice::RealSubmitDrawBatch(const std::vector<DrawBatchItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    // DrawBatchItem 是 SpriteDrawItem 的别名，直接传递
    draw_executor_.DrawBatch(items, view, projection, state_mgr_, shader_mgr_, ubo_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawMeshBatch(items, view, projection, state_mgr_, shader_mgr_, resource_mgr_, ubo_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawSkybox(unsigned int cubemap_texture_handle, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawSkybox(cubemap_texture_handle, view, projection, shader_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) {
    draw_executor_.DrawPostProcess(source_texture, effect_name, params, shader_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawParticles3D(items, view, projection, shader_mgr_);
}

// --- SSBO (Shader Storage Buffer Object) ---

unsigned int OpenGLRhiDevice::CreateSSBO(size_t size, const void* data) {
    unsigned int handle = 0;
    glGenBuffers(1, &handle);
    if (handle == 0) return 0;
    resource_mgr_.ledger().buffers_created += 1;
    if (!supports_ssbo_) {
        glBindBuffer(GL_UNIFORM_BUFFER, handle);
        glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    } else {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
        glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    return handle;
}

void OpenGLRhiDevice::UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
    if (handle == 0) return;
    if (!supports_ssbo_) {
        glBindBuffer(GL_UNIFORM_BUFFER, handle);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    } else {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void OpenGLRhiDevice::BindSSBO(unsigned int handle, unsigned int binding_point) {
    if (!supports_ssbo_) {
        // UBO fallback 映射：SSBO 绑定点 1→3（PointLights），2→4（SpotLights），3/4（cluster grid）no-op
        if (binding_point == 1u) {
            glBindBufferBase(GL_UNIFORM_BUFFER, 3u, handle);
        } else if (binding_point == 2u) {
            glBindBufferBase(GL_UNIFORM_BUFFER, 4u, handle);
        }
        return;
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, handle);
}

void OpenGLRhiDevice::DeleteSSBO(unsigned int handle) {
    if (handle == 0) return;
    glDeleteBuffers(1, &handle);
    resource_mgr_.ledger().buffers_destroyed += 1;
}

// --- Compute Shader (GL 4.3+) ---

unsigned int OpenGLRhiDevice::CreateComputeShader(const std::string& source) {
    if (!supports_ssbo_) {
        DEBUG_LOG_WARN("CreateComputeShader: GL 4.3+ required for compute shaders");
        return 0;
    }
    if (source.empty()) return 0;

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    if (shader == 0) return 0;

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        DEBUG_LOG_ERROR("Compute shader compile error: {}", info_log);
        glDeleteShader(shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    GLint link_ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
    if (!link_ok) {
        char info_log[1024];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        DEBUG_LOG_ERROR("Compute shader link error: {}", info_log);
        glDeleteProgram(program);
        return 0;
    }

    compute_programs_.insert(program);
    return program;
}

void OpenGLRhiDevice::DeleteComputeShader(unsigned int handle) {
    if (handle == 0) return;
    auto it = compute_programs_.find(handle);
    if (it != compute_programs_.end()) {
        glDeleteProgram(handle);
        compute_programs_.erase(it);
    }
}

void OpenGLRhiDevice::DispatchCompute(unsigned int shader_handle,
                                       unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!supports_ssbo_ || shader_handle == 0) return;
    InitComputeProcAddresses();
    glUseProgram(shader_handle);
    if (pfn_glDispatchCompute) pfn_glDispatchCompute(groups_x, groups_y, groups_z);
    glUseProgram(0);
}

void OpenGLRhiDevice::ComputeMemoryBarrier() {
    if (!supports_ssbo_) return;
    InitComputeProcAddresses();
    if (pfn_glMemoryBarrier) pfn_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void OpenGLRhiDevice::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
    if (!supports_ssbo_ || texture_handle == 0) return;
    InitComputeProcAddresses();
    if (pfn_glBindImageTexture) {
        GLenum access = read_only ? GL_READ_ONLY : GL_READ_WRITE;
        pfn_glBindImageTexture(binding, texture_handle, 0, GL_FALSE, 0, access, GL_RGBA32F);
    }
}

void OpenGLRhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                                  int mip_level, bool read_only, bool r32f) {
    if (!supports_ssbo_ || texture_handle == 0) return;
    InitComputeProcAddresses();
    if (pfn_glBindImageTexture) {
        GLenum access = read_only ? GL_READ_ONLY : GL_READ_WRITE;
        GLenum format = r32f ? GL_R32F : GL_RGBA32F;
        pfn_glBindImageTexture(binding, texture_handle, mip_level, GL_FALSE, 0, access, format);
    }
}

void OpenGLRhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    if (!supports_ssbo_) return;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture_handle);
}

// --- Hi-Z Occlusion Culling ---

unsigned int OpenGLRhiDevice::CreateHiZTexture(int width, int height) {
    if (!supports_ssbo_ || width <= 0 || height <= 0) return 0;

    int mip_count = 1;
    {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = std::max(1, w / 2);
            h = std::max(1, h / 2);
            ++mip_count;
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (tex == 0) return 0;

    glBindTexture(GL_TEXTURE_2D, tex);
    // 分配所有 mip level 的存储
    for (int i = 0; i < mip_count; ++i) {
        int mip_w = std::max(1, width >> i);
        int mip_h = std::max(1, height >> i);
        glTexImage2D(GL_TEXTURE_2D, i, GL_R32F, mip_w, mip_h, 0, GL_RED, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip_count - 1);
    glBindTexture(GL_TEXTURE_2D, 0);

    unsigned int handle = next_hiz_handle_++;
    hiz_textures_[handle] = {tex, width, height, mip_count};
    DEBUG_LOG_INFO("Hi-Z texture created: handle={} gl_tex={} {}x{} mips={}",
                   handle, tex, width, height, mip_count);
    return handle;
}

void OpenGLRhiDevice::DeleteHiZTexture(unsigned int handle) {
    auto it = hiz_textures_.find(handle);
    if (it == hiz_textures_.end()) return;
    if (it->second.gl_texture) {
        glDeleteTextures(1, &it->second.gl_texture);
    }
    hiz_textures_.erase(it);
}

int OpenGLRhiDevice::GetHiZMipCount(unsigned int handle) const {
    auto it = hiz_textures_.find(handle);
    return it != hiz_textures_.end() ? it->second.mip_count : 0;
}

unsigned int OpenGLRhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    auto it = hiz_textures_.find(handle);
    return it != hiz_textures_.end() ? it->second.gl_texture : 0;
}

// --- Compute Uniform ---

void OpenGLRhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform1i(loc, value);
}

void OpenGLRhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform1f(loc, value);
}

void OpenGLRhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform2i(loc, x, y);
}

void OpenGLRhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform2f(loc, x, y);
}

void OpenGLRhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    if (!supports_ssbo_ || shader == 0 || !name || !data) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, data);
}

// --- SSBO 读回 ---

void OpenGLRhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    if (!supports_ssbo_ || handle == 0 || !dst || size == 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(offset),
                       static_cast<GLsizeiptr>(size), dst);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// --- 资源账本 ---

void OpenGLRhiDevice::LogResourceLedger() const {
    resource_mgr_.LogResourceLedger();
    // 合并着色器管理器的计数
    const auto& shader_ledger = resource_mgr_.ledger();
    const std::size_t live_programs = (shader_ledger.shader_programs_created + shader_mgr_.programs_created())
                                     - (shader_ledger.shader_programs_destroyed + shader_mgr_.programs_destroyed());
    if (live_programs != 0) {
        DEBUG_LOG_WARN("RHI shader manager additional: programs_created={}, programs_destroyed={}",
                       shader_mgr_.programs_created(), shader_mgr_.programs_destroyed());
    }
}
