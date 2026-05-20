/**
 * @file rhi_device.cpp
 * @brief 娓叉煋纭欢鎺ュ彛(RHI)鎶借薄灞?- OpenGLRhiDevice 鍗忚皟鍣ㄥ疄鐜?
 *
 * OpenGLRhiDevice 浣滀负鍗忚皟鍣ㄦ寔鏈夊洓涓瓙绯荤粺骞跺鎵樿皟鐢細
 * - GLResourceManager锛欸PU 璧勬簮鍒涘缓/閿€姣?鏌ヨ
 * - GLPipelineStateManager锛氭覆鏌撶姸鎬佺紦瀛樹笌搴旂敤
 * - GLShaderManager锛氱潃鑹插櫒缂栬瘧/閾炬帴/Uniform 缂撳瓨
 * - GLDrawExecutor锛氱粯鍒跺懡浠ゆ墽琛?
 */

#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_command_buffer.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"
#include "engine/render/rhi/opengl/gl_loader.h"

// GL 4.3 SSBO / Compute 甯搁噺 鈥?glad/gl.h 浠呭寘鍚?GL 3.3 瀹氫箟
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

// GL 4.2+/4.3 鍑芥暟鎸囬拡锛坓lad 浠呭姞杞?GL 3.3 core锛岄渶鎵嬪姩瑙ｆ瀽锛?
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

// BCn / S3TC / BPTC 鍘嬬缉绾圭悊鏍煎紡甯搁噺
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

namespace dse {
namespace render {

// ============================================================
// HiZImpl 鈥?pimpl 瀹炵幇锛岄伩鍏?unordered_map<uint, HiZTextureInfo> 妯℃澘绗﹀彿娉勬紡
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

// ============================================================
// OpenGLCommandBuffer 鈥?绔嬪嵆杞彂鍒?OpenGLRhiDevice
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

void OpenGLCommandBuffer::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) {
    if (!device_ || items.empty()) return;
    device_->RealSubmitDrawSpriteBatch(items, view_, projection_);
}

void OpenGLCommandBuffer::ClearColor(const glm::vec4& color) {
    if (!device_) return;
    device_->RealClearColor(color);
}

void OpenGLCommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    if (!device_) return;
    device_->RealSubmitDrawSkybox(cubemap_texture_handle, view_, projection_);
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

void OpenGLCommandBuffer::Reset() {
    ResetBase();
}

// ============================================================
// OpenGLRhiDevice 鈥?鍗忚皟鍣紝濮旀墭鍒板瓙绯荤粺
// ============================================================

void OpenGLRhiDevice::EnsureInitialized() {
    if (initialized_) {
        return;
    }

    // 娉ㄥ叆鍑芥暟鎸囬拡鍒?DrawExecutor锛堢敤浜庣紦鍐插尯鎿嶄綔鍥炶皟锛?
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

    // 妫€娴?SSBO 鏀寔锛堥渶瑕?GL 4.3+锛?
    GLint gl_major = 0, gl_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
    supports_ssbo_ = (gl_major > 4) || (gl_major == 4 && gl_minor >= 3);
    shader_mgr_.set_supports_ssbo(supports_ssbo_);
    shader_mgr_.InitBuiltinPBRShader();
    resource_mgr_.ledger().shader_programs_created += 1;

    // 鍒濆鍖?UBO 绠＄悊鍣?
    ubo_mgr_.Init();

    // 鍒濆鍖栧嚑浣曠紦鍐插尯锛?D 绮剧伒 + 3D 缃戞牸 + 鐧借壊绾圭悊锛?
    draw_executor_.InitGeometryBuffers(
        [this]() -> VertexArrayHandle { return CreateVertexArray(); },
        [this](size_t size, const void* data, bool is_dynamic, bool is_index) -> unsigned int { return CreateBuffer(size, data, is_dynamic, is_index); },
        [this](unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) { UpdateBuffer(handle, offset, size, data, is_index); }
    );

    initialized_ = true;
}

void OpenGLRhiDevice::Shutdown() {
    if (!initialized_) return;

    // 娓呯悊娓叉煋鐩爣锛堢敱 ResourceManager 绠＄悊 GL 瀵硅薄鐢熷懡鍛ㄦ湡锛?
    resource_mgr_.DestroyAllRenderTargets();

    // 娓呯悊澶栭儴鍒涘缓鐨勭潃鑹插櫒绋嬪簭锛堥€氳繃 CreateShaderProgram 鍒涘缓銆佺敱 AssetManager 鎸佹湁锛?
    // 鍩轰簬 ledger 宸€兼竻鐞嗭細濡傛灉 created > destroyed锛岃鏄庢湁澶栭儴 shader 鏈鍒犻櫎
    {
        const std::size_t live_external = resource_mgr_.ledger().shader_programs_created
                                        - resource_mgr_.ledger().shader_programs_destroyed;
        // shader_mgr_ 绠＄悊鐨勭潃鑹插櫒鐢?Shutdown 鑷閿€姣侊紝杩欓噷鍙鐞?external 閮ㄥ垎
        if (live_external > 0 && !external_shader_programs_.empty()) {
            for (auto handle : external_shader_programs_) {
                glDeleteProgram(handle);
                resource_mgr_.ledger().shader_programs_destroyed += 1;
            }
            external_shader_programs_.clear();
        } else if (live_external > 0) {
            // external_shader_programs_ 涓虹┖浣?ledger 涓嶅钩琛♀€斺€斿彲鑳?AssetManager 宸叉彁鍓嶉噴鏀?
            // 姝ゆ椂 GL program 宸茬敱 AssetManager::ReleaseGpuResources 閫氳繃 glDeleteProgram 閲婃斁锛?
            // 浣?ledger 璁℃暟鏈洿鏂帮紙AssetManager 璋冪敤 DeleteShaderProgram 鏃?rhi_device 鍙兘宸蹭笉鍙敤锛?
            // 鐩存帴琛ラ綈璐︽湰宸€?
            resource_mgr_.ledger().shader_programs_destroyed += live_external;
        }
    }

    // 娓呯悊 compute shader programs
    for (auto handle : compute_programs_) {
        glDeleteProgram(handle);
    }
    compute_programs_.clear();

    // 娓呯悊 indirect draw buffers
    for (auto& [rhi_handle, gl_buf] : indirect_buffers_) {
        glDeleteBuffers(1, &gl_buf);
    }
    indirect_buffers_.clear();

    // 娓呯悊瀛愮郴缁燂紙閫嗗簭锛?
    draw_executor_.ShutdownGeometryBuffers();
    ubo_mgr_.Shutdown();
    shader_mgr_.Shutdown();
    // Pipeline state 鏃?GL 璧勬簮锛屼粎鏇存柊璐︽湰
    resource_mgr_.ledger().pipeline_states_destroyed += state_mgr_.pipeline_state_count();
    state_mgr_.Shutdown();

    LogResourceLedger();
    initialized_ = false;
}

// --- 甯х敓鍛藉懆鏈?---

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

// --- 缂撳啿鍖?---

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
    if (offset == 0) {
        // Buffer orphaning: 用 glBufferData 替换 glBufferSubData，
        // 驱动可立即分配新 storage 而无需等待 GPU 释放旧数据。
        // GL_STREAM_DRAW 提示驱动：数据每帧写入一次、绘制少量次后丢弃。
        glBufferData(target, static_cast<GLsizeiptr>(size), data, GL_STREAM_DRAW);
    } else {
        glBufferSubData(target, static_cast<GLintptr>(offset),
                        static_cast<GLsizeiptr>(size), data);
    }
}

void OpenGLRhiDevice::DeleteBuffer(unsigned int handle) {
    glDeleteBuffers(1, &handle);
    resource_mgr_.ledger().buffers_destroyed += 1;
}

// --- 椤剁偣鏁扮粍 ---

VertexArrayHandle OpenGLRhiDevice::CreateVertexArray() {
    unsigned int handle = 0;
    glGenVertexArrays(1, &handle);
    resource_mgr_.ledger().vertex_arrays_created += 1;
    return VertexArrayHandle{handle};
}

void OpenGLRhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    unsigned int raw = handle.raw();
    glDeleteVertexArrays(1, &raw);
    resource_mgr_.ledger().vertex_arrays_destroyed += 1;
}

// --- 绾圭悊 ---

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

// --- 娓叉煋鐩爣 ---

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

    RenderTargetResource rt{};
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

// --- 鐫€鑹插櫒 ---

unsigned int OpenGLRhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
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

// --- 绠＄嚎鐘舵€?---

unsigned int OpenGLRhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    unsigned int handle = state_mgr_.CreatePipelineState(desc);
    resource_mgr_.ledger().pipeline_states_created += 1;
    return handle;
}

// --- 鍛戒护缂撳啿 ---

std::shared_ptr<CommandBuffer> OpenGLRhiDevice::CreateCommandBuffer() {
    auto* raw = new OpenGLCommandBuffer();
    raw->SetDevice(this);
    return std::shared_ptr<CommandBuffer>(raw);
}

void OpenGLRhiDevice::Submit(std::shared_ptr<CommandBuffer> /*cmd_buffer*/) {
    // 绔嬪嵆杞彂妯″紡涓嬶紝鍛戒护宸插湪褰曞埗鏃舵墽琛岋紝Submit 浠呬綔涓哄抚缁撴潫鏍囪
}

// --- Real* 鏂规硶锛堢敱 OpenGLCommandBuffer 鐩存帴璋冪敤锛屽鎵樺埌瀛愮郴缁燂級 ---

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

void OpenGLRhiDevice::RealSubmitDrawSpriteBatch(const std::vector<SpriteDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawBatch(items, view, projection, state_mgr_, shader_mgr_, ubo_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawMeshBatch(items, view, projection, state_mgr_, shader_mgr_, resource_mgr_, ubo_mgr_);
}

void OpenGLRhiDevice::RealSubmitDrawSkybox(unsigned int cubemap_texture_handle, const glm::mat4& view, const glm::mat4& projection) {
    draw_executor_.DrawSkybox(cubemap_texture_handle, view, projection, shader_mgr_);
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
        // UBO fallback 鏄犲皠锛歋SBO 缁戝畾鐐?1鈫?锛圥ointLights锛夛紝2鈫?锛圫potLights锛夛紝3/4锛坈luster grid锛塶o-op
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
    // 鍒嗛厤鎵€鏈?mip level 鐨勫瓨鍌?
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
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(offset),
                       static_cast<GLsizeiptr>(size), dst);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// --- Indirect Draw Buffer ---

unsigned int OpenGLRhiDevice::CreateIndirectBuffer(size_t size, const void* data) {
    if (!supports_ssbo_) return 0;  // GL 4.3+ 鍚屾椂鏀寔 indirect draw
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
    auto it = indirect_buffers_.find(handle);
    if (it == indirect_buffers_.end()) return;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, it->second);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void OpenGLRhiDevice::DeleteIndirectBuffer(unsigned int handle) {
    auto it = indirect_buffers_.find(handle);
    if (it == indirect_buffers_.end()) return;
    glDeleteBuffers(1, &it->second);
    indirect_buffers_.erase(it);
}

void OpenGLRhiDevice::MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride) {
    if (draw_count <= 0 || indirect_buffer == 0) return;
    InitComputeProcAddresses();
    if (!pfn_glMultiDrawElementsIndirect) {
        DEBUG_LOG_WARN("glMultiDrawElementsIndirect not available");
        return;
    }

    // 浼樺厛鏌ユ壘 indirect buffer map锛涙壘涓嶅埌鍒欏綋浣?raw GL buffer handle锛堝 SSBO锛?
    unsigned int gl_buf = indirect_buffer;
    auto it = indirect_buffers_.find(indirect_buffer);
    if (it != indirect_buffers_.end()) {
        gl_buf = it->second;
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gl_buf);

    pfn_glMultiDrawElementsIndirect(
        GL_TRIANGLES,
        GL_UNSIGNED_INT,
        nullptr,
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

    // BatchVertex 甯冨眬: stride = 92 bytes (23 floats)
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
    if (b) glDeleteBuffers(1, &b);
    if (i) glDeleteBuffers(1, &i);
}

void OpenGLRhiDevice::BindMegaVAO(VertexArrayHandle vao) {
    if (vao) glBindVertexArray(vao.raw());
}

void OpenGLRhiDevice::UnbindVAO() {
    glBindVertexArray(0);
}

// --- 璧勬簮璐︽湰 ---

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

    // BatchVertex 灞炴€у竷灞€ (stride = 92)
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

    // 鍒涘缓澶氫釜 EBO锛堜竴涓?per LOD level锛?
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

void OpenGLRhiDevice::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                               const glm::vec3& camera_pos,
                                               const glm::vec3& light_dir, const glm::vec3& light_color,
                                               float light_intensity, float ambient_intensity) {
    const unsigned int prog = shader_mgr_.gpu_driven_pbr_shader_handle();
    if (prog == 0) return;

    glUseProgram(prog);

    PerFrameUBO per_frame{};
    per_frame.vp = proj * view;
    per_frame.view = view;
    per_frame.camera_pos = glm::vec4(camera_pos, 0.0f);
    ubo_mgr_.UploadPerFrame(per_frame);

    PerSceneUBO per_scene{};
    per_scene.light_dir_and_enabled     = glm::vec4(light_dir, 1.0f);
    per_scene.light_color_and_ambient   = glm::vec4(light_color, ambient_intensity);
    per_scene.light_params              = glm::vec4(light_intensity, 0.0f, 0.0f, 0.0f);
    ubo_mgr_.UploadPerScene(per_scene);

    ubo_mgr_.BindAll();

    const int loc_skinned = shader_mgr_.gpu_driven_pbr_skinned_loc();
    if (loc_skinned >= 0) glUniform1i(loc_skinned, 0);
    const int loc_morph = shader_mgr_.gpu_driven_pbr_morph_loc();
    if (loc_morph >= 0) glUniform1i(loc_morph, 0);
}

// --- 编辑器场景视图模式 ---

void OpenGLRhiDevice::SetWireframeMode(bool enable) {
    glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL);
}

void OpenGLRhiDevice::SetForceUnlit(bool enable) {
    // 通过 draw executor 的全局状态标志控制 PBR shader 跳过光照计算
    global_render_state_.force_unlit = enable;
}

void OpenGLRhiDevice::SetOverdrawMode(bool enable) {
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
    // 鍚堝苟鐫€鑹插櫒绠＄悊鍣ㄧ殑璁℃暟
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
