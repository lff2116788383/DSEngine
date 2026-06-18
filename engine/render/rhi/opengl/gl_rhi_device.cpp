/**
 * @file rhi_device.cpp
 * @brief 渲染硬件接口(RHI)抽象层 - OpenGLRhiDevice 协调器实现
 *
 * OpenGLRhiDevice 作为协调器持有四个子系统并委托调用：
 * - GLResourceManager：GPU 资源创建/销毁/查询
 * - GLPipelineStateManager：渲染状态缓存与应用
 * - GLShaderManager：着色器编译/链接/Uniform 缓存
 * - GLDrawExecutor：绘制命令执行器
 */

#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_command_buffer.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"
#include "engine/render/rhi/opengl/gl_loader.h"
#include <cstring>

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
#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif
#ifndef GL_COMMAND_BARRIER_BIT
#define GL_COMMAND_BARRIER_BIT 0x00000040
#endif
#ifndef GL_FRAMEBUFFER_BARRIER_BIT
#define GL_FRAMEBUFFER_BARRIER_BIT 0x00000400
#endif
#ifndef GL_TEXTURE_UPDATE_BARRIER_BIT
#define GL_TEXTURE_UPDATE_BARRIER_BIT 0x00000100
#endif

// GL 4.2+/4.3 函数指针（glad 仅加载 GL 3.3 core，需手动解析）
#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall wglGetProcAddress(const char*);
#endif

static void(GLAD_API_PTR* pfn_glDispatchCompute)(GLuint, GLuint, GLuint) = nullptr;
static void(GLAD_API_PTR* pfn_glMemoryBarrier)(GLbitfield) = nullptr;
static void(GLAD_API_PTR* pfn_glBindImageTexture)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum) = nullptr;
static void(GLAD_API_PTR* pfn_glMultiDrawElementsIndirect)(GLenum, GLenum, const void*, GLsizei, GLsizei) = nullptr;

static void InitComputeProcAddresses() {
    static bool done = false;
    if (done) return;
    done = true;
#if defined(__ANDROID__)
    pfn_glDispatchCompute = ::glDispatchCompute;
    pfn_glMemoryBarrier = ::glMemoryBarrier;
    pfn_glBindImageTexture = ::glBindImageTexture;
    pfn_glMultiDrawElementsIndirect = nullptr; // GLES 3.1 不支持 MultiDrawIndirect
#elif defined(_WIN32)
    pfn_glDispatchCompute = reinterpret_cast<decltype(pfn_glDispatchCompute)>(wglGetProcAddress("glDispatchCompute"));
    pfn_glMemoryBarrier = reinterpret_cast<decltype(pfn_glMemoryBarrier)>(wglGetProcAddress("glMemoryBarrier"));
    pfn_glBindImageTexture = reinterpret_cast<decltype(pfn_glBindImageTexture)>(wglGetProcAddress("glBindImageTexture"));
    pfn_glMultiDrawElementsIndirect = reinterpret_cast<decltype(pfn_glMultiDrawElementsIndirect)>(wglGetProcAddress("glMultiDrawElementsIndirect"));
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
#include <cctype>
#include <algorithm>
#include <functional>
#include <string>

namespace dse {
namespace render {

// ============================================================
// HiZImpl -- pimpl 实现，避免 unordered_map<uint, HiZTextureInfo> 模板符号泄漏
// ============================================================

struct OpenGLRhiDevice::HiZImpl {
    struct HiZTextureInfo {
        unsigned int gl_texture = 0;
        int width = 0;
        int height = 0;
        int mip_count = 0;
    };
    std::unordered_map<unsigned int, HiZTextureInfo> textures;
    unsigned int next_handle = 500000;
};

OpenGLRhiDevice::OpenGLRhiDevice() : hiz_impl_(std::make_unique<HiZImpl>()) {}
OpenGLRhiDevice::~OpenGLRhiDevice() = default;

RenderDeviceInfo OpenGLRhiDevice::GetDeviceInfo() const {
    RenderDeviceInfo info;
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (renderer) {
        info.adapter_name = reinterpret_cast<const char*>(renderer);
    }
    // 软件渲染识别：常见软光栅器/通用驱动名（GDI Generic / llvmpipe / softpipe /
    // swrast / Mesa software / Microsoft）即视为软渲。
    std::string lower = info.adapter_name;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* needle : {"gdi generic", "llvmpipe", "softpipe", "swrast", "software", "microsoft"}) {
        if (lower.find(needle) != std::string::npos) { info.is_software = true; break; }
    }
    return info;
}

// ============================================================
// OpenGLCommandBuffer — 立即转发到 OpenGLRhiDevice
// ============================================================

void OpenGLCommandBuffer::SetDevice(OpenGLRhiDevice* device) {
    device_ = device;
    base_device_ = device;
}

void OpenGLCommandBuffer::BeginRenderPass(const RenderPassDesc& render_pass) {
    if (!device_) return;
    device_->RealBeginRenderPass(render_pass);
}

void OpenGLCommandBuffer::EndRenderPass() {
    if (!device_) return;
    device_->RealEndRenderPass();
}

void OpenGLCommandBuffer::SetPipelineState(unsigned int pipeline_state_handle) {
    if (!device_) return;
    device_->RealSetPipelineState(pipeline_state_handle);
}

void OpenGLCommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (!device_ || items.empty()) return;
    DispatchPendingLightArrays();
    device_->RealSubmitDrawMeshBatch(items, view_, projection_);
}

void OpenGLCommandBuffer::ClearColor(const glm::vec4& color) {
    if (!device_) return;
    device_->RealClearColor(color);
}

void OpenGLCommandBuffer::BindShaderProgram(unsigned int program_handle) {
    if (!device_) return;
    device_->RealBindShaderProgram(program_handle);
}

void OpenGLCommandBuffer::BindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                                           const std::vector<VertexAttr>& attrs) {
    if (!device_) return;
    device_->RealBindVertexBuffer(buffer_handle, stride, attrs);
}

void OpenGLCommandBuffer::BindTextureCube(unsigned int slot, unsigned int cubemap_handle) {
    if (!device_) return;
    device_->RealBindTextureCube(slot, cubemap_handle);
}

void OpenGLCommandBuffer::PushConstantsMat4(const glm::mat4& value) {
    if (!device_) return;
    device_->RealPushConstantsMat4(value);
}

void OpenGLCommandBuffer::Draw(uint32_t vertex_count, uint32_t first_vertex) {
    if (!device_) return;
    device_->RealDraw(vertex_count, first_vertex);
}

void OpenGLCommandBuffer::BindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    if (!device_) return;
    device_->RealBindIndexBuffer(buffer_handle, type);
}

void OpenGLCommandBuffer::BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    if (!device_) return;
    device_->RealBindTexture(slot, texture_handle, dim);
}

void OpenGLCommandBuffer::BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                            uint32_t offset, uint32_t size) {
    if (!device_) return;
    device_->RealBindUniformBuffer(slot, buffer_handle, offset, size);
}

void OpenGLCommandBuffer::BindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                            uint32_t offset, uint32_t size) {
    if (!device_) return;
    device_->RealBindStorageBuffer(slot, buffer_handle, offset, size);
}

void OpenGLCommandBuffer::DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    if (!device_) return;
    device_->RealDrawIndexed(index_count, first_index, base_vertex);
}

void OpenGLCommandBuffer::DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                               uint32_t first_index, int32_t base_vertex,
                                               uint32_t first_instance) {
    if (!device_) return;
    device_->RealDrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

void OpenGLCommandBuffer::DrawPostProcess(PostProcessRequest request) {
    if (!device_) return;
    device_->RealSubmitDrawPostProcess(request);
}

void OpenGLCommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || items.empty()) return;
    device_->RealSubmitDrawParticles3D(items, view, projection);
}

void OpenGLCommandBuffer::DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || items.empty()) return;
    device_->RealSubmitDrawHairStrands(items, view, projection);
}

void OpenGLCommandBuffer::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
    glScissor(x, y, width, height);
}

void OpenGLCommandBuffer::ClearDepth(float depth) {
    glEnable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(static_cast<double>(depth));
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

void OpenGLCommandBuffer::Reset() {
    ResetBase();
}

// ============================================================
// OpenGLRhiDevice — 协调器，委托到子系统
// ============================================================

void OpenGLRhiDevice::EnsureInitialized() {
    if (initialized_) {
        return;
    }

    // 注入函数指针到 DrawExecutor（用于缓冲区操作回调）
    draw_executor_.set_create_vao_fn([this]() -> VertexArrayHandle {
        return CreateVertexArray();
    });
    draw_executor_.set_create_buffer_fn([this](size_t size, const void* data, bool is_dynamic, bool is_index) -> unsigned int {
        return CreateBuffer(size, data, is_dynamic, is_index);
    });
    draw_executor_.set_update_buffer_fn([this](unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
        UpdateBuffer(handle, offset, size, data, is_index);
    });
    draw_executor_.set_delete_vao_fn([this](VertexArrayHandle handle) {
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
    // Capability-driven (not platform #ifdef): contexts without SSBO/compute —
    // notably WebGL2 (GLES 3.0) — cannot run the GPU-driven path or the heavy
    // post-process warmup, so they use the dedicated ES3.0-compatible 2D batch
    // program for sprites/UI.
    if (!supports_ssbo_) {
        shader_mgr_.InitSprite2DShader();
#ifdef DSE_ENABLE_3D
        // M5 (best-effort 3D forward on capability-limited contexts, WebGL2
        // included): InitBuiltinPBRShader has a non-SSBO UBO branch — the same
        // path desktop GL<4.3 uses — that lowers to ESSL300. The GPU-driven and
        // compute variants stay gated off inside it (they require SSBO).
        shader_mgr_.InitBuiltinPBRShader();
#endif
    } else {
        shader_mgr_.InitBuiltinPBRShader();
        shader_mgr_.WarmupAllPostProcessShaders();
    }
    resource_mgr_.ledger().shader_programs_created += 1;

    // 鍒濆鍖?UBO 绠＄悊鍣?
    ubo_mgr_.Init();

    // CreateBuffer / UpdateBuffer 内部检查 initialized_，必须在 InitGeometryBuffers 之前置 true
    initialized_ = true;

    // 初始化几何缓冲区：2D 精灵 + 3D 网格 + 白色纹理
    draw_executor_.InitGeometryBuffers(
        [this]() -> VertexArrayHandle { return CreateVertexArray(); },
        [this](size_t size, const void* data, bool is_dynamic, bool is_index) -> unsigned int { return CreateBuffer(size, data, is_dynamic, is_index); },
        [this](unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) { UpdateBuffer(handle, offset, size, data, is_index); }
    );

    gpu_timer_.Init();
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
            // 此时 GL program 已由 AssetManager::ReleaseGpuResources 通过 glDeleteProgram 释放
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

    // 清理 indirect draw buffers
    for (auto& [rhi_handle, gl_buf] : indirect_buffers_) {
        glDeleteBuffers(1, &gl_buf);
    }
    indirect_buffers_.clear();

    // 清理子系统（逆序）
    draw_executor_.ShutdownGeometryBuffers();
    ubo_mgr_.Shutdown();
    shader_mgr_.Shutdown();
    // Pipeline state 无 GL 资源，仅更新账本
    resource_mgr_.ledger().pipeline_states_destroyed += state_mgr_.pipeline_state_count();
    state_mgr_.Shutdown();

    gpu_timer_.Shutdown();

    LogResourceLedger();
    initialized_ = false;
}

// --- 帧生命周期 ---

void OpenGLRhiDevice::BeginFrame() {
    EnsureInitialized();
    gpu_timer_.ResetGpuTimers();
    draw_executor_.BeginFrame();
}

void OpenGLRhiDevice::EndFrame() {
    if (!initialized_) return;
    draw_executor_.EndFrame();
    gpu_timer_.ResolveGpuTimers();
}

const RenderStats& OpenGLRhiDevice::LastFrameStats() const {
    return draw_executor_.last_frame_stats();
}

// --- 缓冲区 ---

unsigned int OpenGLRhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    if (!initialized_) return 0u;
    unsigned int handle = 0;
    glGenBuffers(1, &handle);
    resource_mgr_.ledger().buffers_created += 1;
    unsigned int target = is_index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, handle);
    glBufferData(target, size, data, is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    return handle;
}

void OpenGLRhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    if (!initialized_) return;
    // 使用 GL_COPY_WRITE_BUFFER 避免修改当前 VAO 的 EBO 绑定状态
    glBindBuffer(GL_COPY_WRITE_BUFFER, handle);
    if (offset == 0) {
        // Buffer orphaning: 驱动可立即分配新 storage 而无需等待 GPU 释放旧数据
        glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(size), data, GL_STREAM_DRAW);
    } else {
        glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(offset),
                        static_cast<GLsizeiptr>(size), data);
    }
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void OpenGLRhiDevice::DeleteBuffer(unsigned int handle) {
    glDeleteBuffers(1, &handle);
    resource_mgr_.ledger().buffers_destroyed += 1;
}

// --- 顶点数组 ---

VertexArrayHandle OpenGLRhiDevice::CreateVertexArray() {
    if (!initialized_) return {};
    unsigned int handle = 0;
    glGenVertexArrays(1, &handle);
    resource_mgr_.ledger().vertex_arrays_created += 1;
    return VertexArrayHandle{handle};
}

void OpenGLRhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    if (!initialized_) return;
    unsigned int raw = handle.raw();
    glDeleteVertexArrays(1, &raw);
    resource_mgr_.ledger().vertex_arrays_destroyed += 1;
}

// --- 纹理 ---

unsigned int OpenGLRhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    return CreateTexture2D(width, height, rgba8_data,
                           TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int OpenGLRhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                              const TextureSamplerDesc& sampler) {
    EnsureInitialized();
    const GLint filter = (sampler.filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    const GLint wrap = (sampler.wrap == TextureWrap::ClampToEdge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_mgr_.ledger().textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, texture_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
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
    EnsureInitialized();
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
#if !DSE_GL_ES_RUNTIME
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_texture_handle, 0);
            if (num_color == 0) {
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            }
#else
            // GLES3.1 无 glFramebufferTexture(分层 cubemap) 与 glDrawBuffer：
            // 附 +X 面占位；立方体阴影逐面渲染为移动端后续工作。
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X, depth_texture_handle, 0);
            if (num_color == 0) {
                const GLenum none_buf = GL_NONE;
                glDrawBuffers(1, &none_buf);
                glReadBuffer(GL_NONE);
            }
#endif
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

    RenderTargetResource rt{};
    rt.desc = desc;
    rt.fbo_handle = fbo_handle;
    rt.color_texture_handle = num_color > 0 ? color_handles[0] : 0;
    rt.depth_texture_handle = depth_texture_handle;
    rt.color_texture_handles = std::move(color_handles);
    resource_mgr_.StoreRenderTarget(handle, rt);
    return handle;
}

void OpenGLRhiDevice::DeleteRenderTarget(unsigned int render_target_handle) {
    const auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    if (!rt) return;

    if (rt->fbo_handle != 0) {
        GLuint fbo = rt->fbo_handle;
        glDeleteFramebuffers(1, &fbo);
        resource_mgr_.ledger().framebuffers_destroyed += 1;
    }
    for (unsigned int ch : rt->color_texture_handles) {
        if (ch != 0) {
            glDeleteTextures(1, &ch);
            resource_mgr_.ledger().textures_destroyed += 1;
        }
    }
    if (rt->depth_texture_handle != 0) {
        glDeleteTextures(1, &rt->depth_texture_handle);
        resource_mgr_.ledger().textures_destroyed += 1;
    }
    resource_mgr_.RemoveRenderTarget(render_target_handle);
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
    if (!initialized_) return 0u;
    unsigned int shader_program = GLShaderManager::CompileProgram(vert_src.c_str(), frag_src.c_str());
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
    auto* raw = new OpenGLCommandBuffer();
    raw->SetDevice(this);
    return std::shared_ptr<CommandBuffer>(raw);
}

void OpenGLRhiDevice::Submit(std::shared_ptr<CommandBuffer> /*cmd_buffer*/) {
    // 立即转发模式下，命令已在录制时执行，Submit 仅作为帧结束标记
}

// --- Real* 方法（由 OpenGLCommandBuffer 直接调用，委托到子系统） ---

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

void OpenGLRhiDevice::RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawMeshBatch(items, view, projection, state_mgr_, shader_mgr_, resource_mgr_, ubo_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawPostProcess(const PostProcessRequest& request) {
    draw_executor_.DrawPostProcess(request, shader_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawParticles3D(items, view, projection, shader_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawHairStrands(items, view, projection, shader_mgr_);
}

// --- 通用绘制原语 (A1) ---

void OpenGLRhiDevice::RealBindShaderProgram(unsigned int program_handle) {
    draw_executor_.PrimBindShaderProgram(program_handle);
}

void OpenGLRhiDevice::RealBindVertexBuffer(unsigned int buffer_handle, uint32_t stride, const std::vector<VertexAttr>& attrs) {
    draw_executor_.PrimBindVertexBuffer(buffer_handle, stride, attrs);
}

void OpenGLRhiDevice::RealBindTextureCube(unsigned int slot, unsigned int cubemap_handle) {
    draw_executor_.PrimBindTextureCube(slot, cubemap_handle);
}

void OpenGLRhiDevice::RealPushConstantsMat4(const glm::mat4& value) {
    draw_executor_.PrimPushConstantsMat4(value);
}

void OpenGLRhiDevice::RealDraw(uint32_t vertex_count, uint32_t first_vertex) {
    draw_executor_.PrimDraw(vertex_count, first_vertex);
}

// --- 通用绘制原语 (B0) ---

void OpenGLRhiDevice::RealBindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    draw_executor_.PrimBindIndexBuffer(buffer_handle, type);
}

void OpenGLRhiDevice::RealBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    draw_executor_.PrimBindTexture(slot, texture_handle, dim);
}

void OpenGLRhiDevice::RealBindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) {
    draw_executor_.PrimBindUniformBuffer(slot, buffer_handle, offset, size);
}

void OpenGLRhiDevice::RealBindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) {
    draw_executor_.PrimBindStorageBuffer(slot, buffer_handle, offset, size);
}

void OpenGLRhiDevice::RealDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    draw_executor_.PrimDrawIndexed(index_count, first_index, base_vertex);
}

void OpenGLRhiDevice::RealDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                               uint32_t first_index, int32_t base_vertex,
                                               uint32_t first_instance) {
    draw_executor_.PrimDrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

// --- 内建资源访问器 (A1) ---

unsigned int OpenGLRhiDevice::GetBuiltinProgram(BuiltinProgram program) {
    EnsureInitialized();
    switch (program) {
        case BuiltinProgram::Skybox:
            if (shader_mgr_.skybox_shader_handle() == 0) shader_mgr_.InitSkyboxShader();
            return shader_mgr_.skybox_shader_handle();
        case BuiltinProgram::Sprite2D:
            if (shader_mgr_.sprite2d_shader_handle() == 0) shader_mgr_.InitSprite2DShader();
            return shader_mgr_.sprite2d_shader_handle();
        case BuiltinProgram::SpriteFxSdf:
            if (shader_mgr_.sprite_fx_sdf_shader_handle() == 0) shader_mgr_.InitSpriteFxSdfShader();
            return shader_mgr_.sprite_fx_sdf_shader_handle();
        case BuiltinProgram::SpriteFxVfx:
            if (shader_mgr_.sprite_fx_vfx_shader_handle() == 0) shader_mgr_.InitSpriteFxVfxShader();
            return shader_mgr_.sprite_fx_vfx_shader_handle();
        case BuiltinProgram::ForwardPbr:
            if (shader_mgr_.forward_pbr_shader_handle() == 0) shader_mgr_.InitForwardPbrShader();
            return shader_mgr_.forward_pbr_shader_handle();
        case BuiltinProgram::ForwardPbrSkinned:
            if (shader_mgr_.forward_pbr_skinned_shader_handle() == 0) shader_mgr_.InitForwardPbrSkinnedShader();
            return shader_mgr_.forward_pbr_skinned_shader_handle();
    }
    return 0;
}

unsigned int OpenGLRhiDevice::GetSkyboxCubeVertexBuffer() {
    EnsureInitialized();
    if (skybox_cube_vbo_ == 0) {
        static const float kSkyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
        };
        skybox_cube_vbo_ = CreateBuffer(sizeof(kSkyboxVertices), kSkyboxVertices, false, false);
    }
    return skybox_cube_vbo_;
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

// ============================================================
// RenderGraph 自动屏障（GL: glMemoryBarrier / glTextureBarrier）
// ============================================================

void OpenGLRhiDevice::TransitionRenderTarget(unsigned int rt_handle,
                                              ResourceState from, ResourceState to) {
    (void)rt_handle;
    if (from == to) return;

    InitComputeProcAddresses();

    // UAV (compute storage) → 任何其他状态：需要全量 memory barrier
    if (from == ResourceState::UnorderedAccess && to != ResourceState::UnorderedAccess) {
        if (pfn_glMemoryBarrier) {
            pfn_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                                GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                                GL_TEXTURE_FETCH_BARRIER_BIT |
                                GL_FRAMEBUFFER_BARRIER_BIT);
        }
        return;
    }

    // RenderTarget/DepthWrite → ShaderRead：需要 framebuffer barrier
    // 确保 FBO 写入完成后才能被后续着色器采样
    if ((from == ResourceState::RenderTarget || from == ResourceState::DepthWrite) &&
        to == ResourceState::ShaderRead) {
        if (pfn_glMemoryBarrier) {
            pfn_glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
        }
        return;
    }

    // CopyDest → ShaderRead：确保像素传输完成
    if (from == ResourceState::CopyDest && to == ResourceState::ShaderRead) {
        if (pfn_glMemoryBarrier) {
            pfn_glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
        return;
    }
}

void OpenGLRhiDevice::ComputeMemoryBarrier() {
    if (!supports_ssbo_) return;
    InitComputeProcAddresses();
    if (pfn_glMemoryBarrier) pfn_glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
        GL_TEXTURE_FETCH_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
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

unsigned int OpenGLRhiDevice::CreateComputeWriteTexture2D(int width, int height) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return static_cast<unsigned int>(tex);
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

    unsigned int handle = hiz_impl_->next_handle++;
    hiz_impl_->textures[handle] = {tex, width, height, mip_count};
    DEBUG_LOG_INFO("Hi-Z texture created: handle={} gl_tex={} {}x{} mips={}",
                   handle, tex, width, height, mip_count);
    return handle;
}

void OpenGLRhiDevice::DeleteHiZTexture(unsigned int handle) {
    auto it = hiz_impl_->textures.find(handle);
    if (it == hiz_impl_->textures.end()) return;
    if (it->second.gl_texture) {
        glDeleteTextures(1, &it->second.gl_texture);
    }
    hiz_impl_->textures.erase(it);
}

int OpenGLRhiDevice::GetHiZMipCount(unsigned int handle) const {
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? it->second.mip_count : 0;
}

unsigned int OpenGLRhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? it->second.gl_texture : 0;
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

void OpenGLRhiDevice::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void OpenGLRhiDevice::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform3i(loc, x, y, z);
}

void OpenGLRhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    if (!supports_ssbo_ || shader == 0 || !name) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void OpenGLRhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    if (!supports_ssbo_ || shader == 0 || !name || !data) return;
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, data);
}

// --- SSBO 璇诲洖 ---

void OpenGLRhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    if (!supports_ssbo_ || handle == 0 || !dst || size == 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
#if DSE_GL_ES_RUNTIME
    // GLES 无 glGetBufferSubData：用 glMapBufferRange 读回。
    if (void* mapped = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(offset),
                                        static_cast<GLsizeiptr>(size), GL_MAP_READ_BIT)) {
        std::memcpy(dst, mapped, size);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
#else
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(offset),
                       static_cast<GLsizeiptr>(size), dst);
#endif
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// --- Indirect Draw Buffer ---

unsigned int OpenGLRhiDevice::CreateIndirectBuffer(size_t size, const void* data) {
    if (!initialized_ || !supports_ssbo_) return 0;
    InitComputeProcAddresses();
    unsigned int gl_buf = 0;
    glGenBuffers(1, &gl_buf);
    if (gl_buf == 0) return 0;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gl_buf);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    unsigned int handle = next_indirect_handle_++;
    indirect_buffers_[handle] = gl_buf;
    return handle;
}

void OpenGLRhiDevice::UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) {
    if (!initialized_) return;
    auto it = indirect_buffers_.find(handle);
    if (it == indirect_buffers_.end()) return;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, it->second);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void OpenGLRhiDevice::DeleteIndirectBuffer(unsigned int handle) {
    if (!initialized_) return;
    auto it = indirect_buffers_.find(handle);
    if (it == indirect_buffers_.end()) return;
    glDeleteBuffers(1, &it->second);
    indirect_buffers_.erase(it);
}

void OpenGLRhiDevice::MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset) {
    if (draw_count <= 0 || indirect_buffer == 0) return;
    InitComputeProcAddresses();
    if (!pfn_glMultiDrawElementsIndirect) {
        DEBUG_LOG_WARN("glMultiDrawElementsIndirect not available");
        return;
    }

    // 优先查找 indirect buffer map；找不到则当作 raw GL buffer handle（如 SSBO）
    unsigned int gl_buf = indirect_buffer;
    auto it = indirect_buffers_.find(indirect_buffer);
    if (it != indirect_buffers_.end()) {
        gl_buf = it->second;
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gl_buf);

    pfn_glMultiDrawElementsIndirect(
        GL_TRIANGLES,
        GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(byte_offset),
        static_cast<GLsizei>(draw_count),
        static_cast<GLsizei>(stride)
    );

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    draw_executor_.MutableCurrentStats().indirect_draw_calls += 1;
}

// --- Mega Buffer (GPU Driven) ---

VertexArrayHandle OpenGLRhiDevice::CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                             BufferHandle& out_vbo, BufferHandle& out_ibo) {
    unsigned int vao = 0, vbo = 0, ibo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    if (vao == 0 || vbo == 0 || ibo == 0) {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ibo) glDeleteBuffers(1, &ibo);
        out_vbo = {}; out_ibo = {};
        return {};
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vbo_size_bytes), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(ibo_size_bytes), nullptr, GL_DYNAMIC_DRAW);

    // BatchVertex 布局: stride = 92 bytes (23 floats)
    const GLsizei stride = 92;
    // pos: location 0, vec3, offset 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    // color: location 1, vec4, offset 12
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    // uv: location 2, vec2, offset 28
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(28));
    // normal: location 3, vec3, offset 36
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(36));
    // tangent: location 4, vec3, offset 48
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(48));
    // weights: location 5, vec4, offset 60
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(60));
    // joints: location 6, vec4, offset 76
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(76));

    glBindVertexArray(0);
    out_vbo = BufferHandle{vbo};
    out_ibo = BufferHandle{ibo};
    resource_mgr_.ledger().buffers_created += 2;
    return VertexArrayHandle{vao};
}

void OpenGLRhiDevice::UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) {
    if (!vbo || size == 0 || !data) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo.raw());
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLRhiDevice::UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) {
    if (!ibo || size == 0 || !data) return;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo.raw());
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGLRhiDevice::DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) {
    unsigned int v = vao.raw(), b = vbo.raw(), i = ibo.raw();
    if (v) glDeleteVertexArrays(1, &v);
    if (b) { glDeleteBuffers(1, &b); resource_mgr_.ledger().buffers_destroyed += 1; }
    if (i) { glDeleteBuffers(1, &i); resource_mgr_.ledger().buffers_destroyed += 1; }
}

void OpenGLRhiDevice::BindMegaVAO(VertexArrayHandle vao) {
    if (vao) glBindVertexArray(vao.raw());
}

void OpenGLRhiDevice::UnbindVAO() {
    glBindVertexArray(0);
}

// --- 资源账本 ---

// --- Static Mesh VAO ---

VertexArrayHandle OpenGLRhiDevice::CreateStaticMeshVAO(
    const void* vertex_data, size_t vertex_bytes,
    const std::vector<const void*>& ebo_datas,
    const std::vector<size_t>& ebo_sizes,
    BufferHandle& out_vbo,
    std::vector<BufferHandle>& out_ebos)
{
    if (!vertex_data || vertex_bytes == 0) { out_vbo = {}; out_ebos.clear(); return {}; }
    if (ebo_datas.size() != ebo_sizes.size()) { out_vbo = {}; out_ebos.clear(); return {}; }

    unsigned int vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    if (vao == 0 || vbo == 0) {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        out_vbo = {}; out_ebos.clear(); return {};
    }

    glBindVertexArray(vao);

    // VBO: static vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertex_bytes), vertex_data, GL_STATIC_DRAW);

    // BatchVertex 属性布局 (stride = 92)
    const GLsizei stride = static_cast<GLsizei>(sizeof(BatchVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, normal)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, tangent)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, weights)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(BatchVertex, joints)));

    // 创建多个 EBO（一个 per LOD level）
    out_ebos.resize(ebo_datas.size());
    if (!ebo_datas.empty()) {
        std::vector<unsigned int> raw_ebos(ebo_datas.size(), 0);
        glGenBuffers(static_cast<GLsizei>(ebo_datas.size()), raw_ebos.data());
        for (size_t i = 0; i < ebo_datas.size(); ++i) {
            out_ebos[i] = BufferHandle{raw_ebos[i]};
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, raw_ebos[i]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(ebo_sizes[i]),
                         ebo_datas[i], GL_STATIC_DRAW);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, raw_ebos[0]);
    }

    glBindVertexArray(0);
    out_vbo = BufferHandle{vbo};
    resource_mgr_.ledger().buffers_created += 1 + static_cast<int>(ebo_datas.size());
    return VertexArrayHandle{vao};
}

void OpenGLRhiDevice::DeleteStaticMeshVAO(VertexArrayHandle vao, BufferHandle vbo,
                                           const std::vector<BufferHandle>& ebos) {
    if (!ebos.empty()) {
        std::vector<unsigned int> raw_ebos(ebos.size());
        for (size_t i = 0; i < ebos.size(); ++i) raw_ebos[i] = ebos[i].raw();
        glDeleteBuffers(static_cast<GLsizei>(raw_ebos.size()), raw_ebos.data());
        resource_mgr_.ledger().buffers_destroyed += static_cast<int>(ebos.size());
    }
    unsigned int raw_vbo = vbo.raw(), raw_vao = vao.raw();
    if (raw_vbo) { glDeleteBuffers(1, &raw_vbo); resource_mgr_.ledger().buffers_destroyed += 1; }
    if (raw_vao) { glDeleteVertexArrays(1, &raw_vao); }
}

void OpenGLRhiDevice::BindVAOWithEBO(VertexArrayHandle vao, BufferHandle ebo) {
    glBindVertexArray(vao.raw());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo.raw());
}

bool OpenGLRhiDevice::HasGPUDrivenPBRShader() const {
    return shader_mgr_.gpu_driven_pbr_shader_handle() != 0;
}

void OpenGLRhiDevice::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                               const glm::vec3& camera_pos,
                                               const glm::vec3& light_dir, const glm::vec3& light_color,
                                               float light_intensity, float ambient_intensity,
                                               float shadow_strength) {
    const unsigned int prog = shader_mgr_.gpu_driven_pbr_shader_handle();
    if (prog == 0) return;

    glUseProgram(prog);

    PerFrameUBO per_frame{};
    per_frame.vp = proj * view;
    per_frame.view = view;
    per_frame.camera_pos = glm::vec4(camera_pos, 0.0f);
    ubo_mgr_.UploadPerFrame(per_frame);

    // 从 draw_executor_ 读取 CSMShadowPass 已缓存的 CSM 矩阵和级联分割距离
    const auto& gs = draw_executor_.global_state();

    PerSceneUBO per_scene{};
    per_scene.light_dir_and_enabled   = glm::vec4(light_dir, 1.0f);
    per_scene.light_color_and_ambient = glm::vec4(light_color, ambient_intensity);

    const float receive_shadow = (shadow_strength > 0.0f) ? 1.0f : 0.0f;
    per_scene.light_params = glm::vec4(light_intensity, shadow_strength, receive_shadow, 0.0f);

    per_scene.cascade_splits = glm::vec4(
        gs.cascade_splits[0], gs.cascade_splits[1], gs.cascade_splits[2], 0.0f);

    for (int i = 0; i < 3; ++i) {
        per_scene.light_space_matrices[i] = gs.light_space_matrix[i];
    }

    ubo_mgr_.UploadPerScene(per_scene);
    ubo_mgr_.BindAll();

    const int loc_skinned = shader_mgr_.gpu_driven_pbr_skinned_loc();
    if (loc_skinned >= 0) glUniform1i(loc_skinned, 0);
    const int loc_morph = shader_mgr_.gpu_driven_pbr_morph_loc();
    if (loc_morph >= 0) glUniform1i(loc_morph, 0);

    // CSM shadow map 纹理绑定（per-item 路径由 DrawMeshBatch 绑定，GPU-driven 需在此补齐）
    const auto& slots = shader_mgr_.pbr_texture_slots();
    for (int i = 0; i < 3; ++i) {
        if (gs.shadow_map[i] != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.shadow_base + i);
            glBindTexture(GL_TEXTURE_2D, gs.shadow_map[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (gs.spot_shadow_map[i] != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.spot_shadow_base + i);
            glBindTexture(GL_TEXTURE_2D, gs.spot_shadow_map[i]);
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (gs.point_shadow_map[i] != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.point_shadow_base + i);
            glBindTexture(GL_TEXTURE_CUBE_MAP, gs.point_shadow_map[i]);
        }
    }
}

void OpenGLRhiDevice::SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) {
    const unsigned int prog = shader_mgr_.gpu_driven_shadow_shader_handle();
    if (prog == 0) return;

    glUseProgram(prog);

    PerFrameUBO per_frame{};
    per_frame.vp = light_proj * light_view;
    per_frame.view = light_view;
    per_frame.camera_pos = glm::vec4(0.0f);
    ubo_mgr_.UploadPerFrame(per_frame);
    ubo_mgr_.BindAll();

    const int loc_skinned = shader_mgr_.gpu_driven_shadow_skinned_loc();
    if (loc_skinned >= 0) glUniform1i(loc_skinned, 0);
}

void OpenGLRhiDevice::BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                              unsigned int metallic_roughness,
                                              unsigned int emissive, unsigned int occlusion) {
    const auto& slots = shader_mgr_.pbr_texture_slots();
    const unsigned int white = draw_executor_.white_texture_handle();

    glActiveTexture(GL_TEXTURE0 + slots.albedo);
    glBindTexture(GL_TEXTURE_2D, albedo != 0 ? albedo : white);

    glActiveTexture(GL_TEXTURE0 + slots.normal);
    glBindTexture(GL_TEXTURE_2D, normal != 0 ? normal : white);

    glActiveTexture(GL_TEXTURE0 + slots.metallic_roughness);
    glBindTexture(GL_TEXTURE_2D, metallic_roughness != 0 ? metallic_roughness : white);

    glActiveTexture(GL_TEXTURE0 + slots.emissive);
    glBindTexture(GL_TEXTURE_2D, emissive != 0 ? emissive : white);

    glActiveTexture(GL_TEXTURE0 + slots.occlusion);
    glBindTexture(GL_TEXTURE_2D, occlusion != 0 ? occlusion : white);
}

// --- 编辑器场景视图模式 ---

void OpenGLRhiDevice::SetWireframeMode(bool enable) {
    if (!initialized_) return;
#if !DSE_GL_ES_RUNTIME
    glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL);
#else
    (void)enable;  // GLES 不支持 glPolygonMode 线框模式（编辑器功能，移动端忽略）
#endif
}

void OpenGLRhiDevice::SetForceUnlit(bool enable) {
    global_render_state_.force_unlit = enable;
}

void OpenGLRhiDevice::SetOverdrawMode(bool enable) {
    if (!initialized_) {
        global_render_state_.overdraw_mode = enable;
        return;
    }
    if (enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    global_render_state_.overdraw_mode = enable;
}

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

} // namespace render
} // namespace dse
