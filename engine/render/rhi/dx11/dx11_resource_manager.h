/**
 * @file dx11_resource_manager.h
 * @brief D3D11 GPU 资源管理器 — 纹理/Buffer/RenderTarget 生命周期管理
 *
 * 对标 VulkanResourceManager / GLResourceManager，使用 ComPtr 管理 COM 资源。
 * 内部维护 unsigned int handle → COM 指针映射表。
 */

#ifndef DSE_RENDER_DX11_RESOURCE_MANAGER_H
#define DSE_RENDER_DX11_RESOURCE_MANAGER_H

#include <d3d11.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include "engine/render/rhi/rhi_types.h"

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

class DX11Context;

/// D3D11 纹理资源封装
struct DX11Texture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;  ///< compute write 用（仅 CreateComputeWriteTexture2D 创建时有效）
    ComPtr<ID3D11SamplerState> sampler;
    int width = 0;
    int height = 0;
    int depth = 1;
    bool is_cube = false;
    bool is_3d = false;
};

/// D3D11 缓冲资源封装
struct DX11Buffer {
    ComPtr<ID3D11Buffer> buffer;
    size_t size = 0;
    bool is_dynamic = false;
    bool is_index = false;
};

/// D3D11 渲染目标资源封装
struct DX11RenderTarget {
    int width = 0;
    int height = 0;
    bool has_color = true;
    bool has_depth = false;
    bool generate_mipmaps = false;
    bool is_msaa = false;                          ///< 是否为 MSAA 渲染目标
    int msaa_samples = 1;                          ///< MSAA 采样数
    int color_attachment_count = 1;                ///< MRT 颜色附件数量

    ComPtr<ID3D11Texture2D> color_texture;         ///< 颜色纹理（MSAA 时为 MSAA 纹理），兼容 = color_textures[0]
    ComPtr<ID3D11RenderTargetView> color_rtv;
    ComPtr<ID3D11ShaderResourceView> color_srv;    ///< MSAA 时指向 resolve 纹理
    ComPtr<ID3D11UnorderedAccessView> color_uav;   ///< UAV（allow_uav=true 时有效）
    unsigned int color_texture_handle = 0;         ///< MSAA 时指向 resolve 纹理句柄

    ComPtr<ID3D11Texture2D> color_resolve_texture; ///< MSAA resolve 目标（1x）

    std::vector<ComPtr<ID3D11Texture2D>> color_textures_mrt;       ///< MRT: 所有颜色纹理
    std::vector<ComPtr<ID3D11RenderTargetView>> color_rtvs_mrt;    ///< MRT: 所有 RTV
    std::vector<ComPtr<ID3D11ShaderResourceView>> color_srvs_mrt;  ///< MRT: 所有 SRV
    std::vector<unsigned int> color_texture_handles_mrt;           ///< MRT: 所有纹理句柄

    ComPtr<ID3D11Texture2D> depth_texture;
    ComPtr<ID3D11DepthStencilView> depth_dsv;
    ComPtr<ID3D11ShaderResourceView> depth_srv;
    unsigned int depth_texture_handle = 0;
};

/// D3D11 SSBO 资源封装（StructuredBuffer + SRV）
struct DX11SSBO {
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11ShaderResourceView> srv;
    size_t size = 0;
};

/// D3D11 顶点数组模拟（InputLayout + Buffer 绑定组合）
struct DX11VertexArray {
    unsigned int placeholder = 0;
};

/// D3D11 Indirect Draw Buffer 封装（D3D11_BIND_INDIRECT_ARGS_BUFFER）
struct DX11IndirectBuffer {
    ComPtr<ID3D11Buffer> buffer;
    size_t size = 0;
};

/**
 * @class DX11ResourceManager
 * @brief D3D11 GPU 资源管理器
 */
class DX11ResourceManager {
public:
    DX11ResourceManager() = default;
    ~DX11ResourceManager() = default;

    /// 初始化
    bool Init(DX11Context* context);

    /// 销毁所有资源
    void Shutdown();

    // --- 纹理 ---
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter);
    unsigned int CreateComputeWriteTexture2D(int width, int height);
    unsigned int CreateCompressedTexture2D(CompressedTextureFormat format,
                                           const std::vector<CompressedMipLevel>& mips,
                                           bool linear_filter);
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter);
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter);
    void DeleteTexture(unsigned int handle);
    const DX11Texture* GetTexture(unsigned int handle) const;

    // --- 缓冲区 ---
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index);
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index);
    void DeleteBuffer(unsigned int handle);
    const DX11Buffer* GetBuffer(unsigned int handle) const;

    // --- SSBO (StructuredBuffer + SRV) ---
    unsigned int CreateSSBO(size_t size, const void* data);
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data);
    void BindSSBO(unsigned int handle, unsigned int binding_point);
    void DeleteSSBO(unsigned int handle);
    const DX11SSBO* GetSSBO(unsigned int handle) const;
    void set_ssbo_register_base(unsigned int base) { ssbo_register_base_ = base; }

    // --- 渲染目标 ---
    unsigned int CreateRenderTarget(int width, int height, bool has_color, bool has_depth,
                                     bool generate_mipmaps, bool cube_map,
                                     int msaa_samples = 1, bool allow_uav = false,
                                     int color_attachment_count = 1);
    void DeleteRenderTarget(unsigned int handle);
    const DX11RenderTarget* GetRenderTarget(unsigned int handle) const;
    unsigned int GetRenderTargetColorTextureHandle(unsigned int handle) const;
    unsigned int GetRenderTargetDepthTextureHandle(unsigned int handle) const;

    // --- 渲染目标回读 ---
    struct ReadbackResult {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> pixels;
    };
    ReadbackResult ReadRenderTargetColor(unsigned int handle) const;

    // --- 顶点数组（D3D11 无 VAO，占位） ---
    unsigned int CreateVertexArray();
    void DeleteVertexArray(unsigned int handle);

    // --- Indirect Draw Buffer ---
    unsigned int CreateIndirectBuffer(size_t size, const void* data);
    /// 注意：底层使用 MAP_WRITE_DISCARD，调用者应保证 offset==0 且 size 覆盖整个缓冲
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data);
    void DeleteIndirectBuffer(unsigned int handle);
    const DX11IndirectBuffer* GetIndirectBuffer(unsigned int handle) const;

    // --- 异步纹理上传 ---
    unsigned int CreateTexture2DAsync(int width, int height);
    void FlushPendingUploads();
    void QueueTextureUpload(unsigned int handle, int width, int height, const unsigned char* rgba8_data);

private:
    DX11Context* context_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* dc_ = nullptr;

    std::unordered_map<unsigned int, DX11Texture> textures_;
    std::unordered_map<unsigned int, DX11Buffer> buffers_;
    std::unordered_map<unsigned int, DX11SSBO> ssbos_;
    std::unordered_map<unsigned int, DX11RenderTarget> render_targets_;
    std::unordered_map<unsigned int, DX11VertexArray> vertex_arrays_;
    std::unordered_map<unsigned int, DX11IndirectBuffer> indirect_buffers_;

    unsigned int next_texture_handle_ = 800000;
    unsigned int next_buffer_handle_ = 810000;
    unsigned int next_ssbo_handle_ = 815000;
    unsigned int next_render_target_handle_ = 820000;
    unsigned int next_vao_handle_ = 830000;
    unsigned int next_indirect_handle_ = 840000;

    bool initialized_ = false;
    unsigned int ssbo_register_base_ = 16; ///< SSBO t-register 起始偏移（由 reflection 计算填充）

    /// 异步纹理上传条目
    struct PendingUpload {
        unsigned int handle;
        ComPtr<ID3D11Texture2D> staging;
        int width;
        int height;
    };
    std::queue<PendingUpload> pending_uploads_;
    std::mutex pending_uploads_mutex_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_RESOURCE_MANAGER_H
