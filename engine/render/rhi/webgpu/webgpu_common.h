/**
 * @file webgpu_common.h
 * @brief WebGPU 后端跨 manager 共享的类型与小工具。
 *
 * 拆分 WebGPU 单体 device 为独立 manager 类（Context/Resource/Shader/PipelineState/DrawExecutor）后，
 * 这里集中放置「被两个及以上 manager 引用」的内容：
 *   - 资源表条目结构（BufferEntry/TextureEntry/RenderTargetEntry/ShaderEntry/ComputeShaderEntry）；
 *   - 命令录制用的逻辑绑定结构（BindingInfo）；
 *   - 枚举/格式映射 + 资源小工具（标 inline，跨 TU 共享单一定义）。
 *
 * 仅在 Emscripten + DSE_ENABLE_WEBGPU 下编入（与各 webgpu 实现文件一致）。
 */

#ifndef DSE_WEBGPU_COMMON_H
#define DSE_WEBGPU_COMMON_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/rhi_device.h"

#include <webgpu/webgpu.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace dse {
namespace render {

// ============================================================
// 资源表条目（句柄 → WebGPU 原生对象）
// ============================================================

/// 缓冲条目：顶点/索引/uniform 等。usage 记录创建时的 WGPU usage 位，供更新/绑定校验。
struct BufferEntry {
    WGPUBuffer buffer = nullptr;
    uint64_t   size   = 0;        ///< 已对齐到 4 字节的实际分配大小
    uint64_t   logical_size = 0;  ///< 调用方请求的逻辑大小
    WGPUBufferUsageFlags usage = 0;
    bool is_index = false;
};

/// 纹理条目：持有原生纹理 + 默认采样视图 + 采样器。RT 的颜色/深度附件也登记于此，
/// 以便 BindTexture(slot, handle, dim) 统一查表绑定。
struct TextureEntry {
    WGPUTexture     texture = nullptr;
    WGPUTextureView view    = nullptr;   ///< 默认（全 mip / 全层）采样视图
    WGPUSampler     sampler = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
    WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
    uint32_t width = 0, height = 0, depth = 1, mip_levels = 1, array_layers = 1;
    int  msaa_samples = 1;
    bool owns_texture = true;            ///< RT 持有的附件由 RT 释放时统一释放
};

/// 渲染目标条目：颜色/深度附件均以纹理句柄形式登记于 textures_。
struct RenderTargetEntry {
    std::vector<unsigned int> color_textures;  ///< textures_ 中的句柄
    unsigned int depth_texture = 0;            ///< textures_ 中的句柄，0=无深度
    uint32_t width = 0, height = 0;
    int  msaa_samples = 1;
    bool is_cube = false;
};

/// 着色器程序条目：引擎 GLSL 源暂存于此（WebGPU 无离线 GLSL→WGSL，故不转译，module 留空，
/// 其绘制在录制期被优雅跳过）；内建程序以 WGSL 源创建并编译出 module（vs/fs 同模块，入口
/// 名见 vs_entry/fs_entry）。仅内建 WGSL 程序携带 module。
struct ShaderEntry {
    std::string vert_src;
    std::string frag_src;
    WGPUShaderModule module = nullptr;  ///< 仅 WGSL 程序非空
    std::string vs_entry = "vs_main";
    std::string fs_entry = "fs_main";
    bool has_fragment = true;           ///< 仅深度 pass 等可无片元入口
    /// WGSL 实际声明的绑定集合（key = (group<<16)|binding），CreateShaderProgram 解析填充。
    /// explicit pipeline-layout/BindGroup 仅纳入此集合内的绑定：引擎可能绑定多于着色器
    /// 所需的资源（如 ForwardShaded 绑 20 纹理槽），全量纳入会超 per-stage 采样上限并使
    /// layout 与着色器用量不符；按此集合过滤即对齐 WebGPU 自动布局语义。
    std::set<uint32_t> wgsl_bindings;
};

/// compute shader 条目：WGSL module + 入口名 + 实际声明的 @group/@binding 集合
/// （key=(group<<16)|binding，供 explicit pipeline-layout/BindGroup 过滤）。
struct ComputeShaderEntry {
    WGPUShaderModule module = nullptr;
    std::string entry = "cs_main";
    std::set<uint32_t> wgsl_bindings;
};

/// 单个逻辑绑定（用于在 BGL 条目与 BindGroup 条目间共享同一遍历顺序，杜绝二者发散）。
struct BindingInfo {
    uint32_t binding = 0;
    enum class Kind { Uniform, Storage, Texture, Sampler, StorageTexture } kind = Kind::Uniform;
    WGPUShaderStageFlags visibility = 0;
    WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
    WGPUTextureSampleType sample_type = WGPUTextureSampleType_Float;
    WGPUTextureFormat tex_format = WGPUTextureFormat_RGBA8Unorm;  ///< storage texture 格式
    bool sampler_nonfiltering = false;  ///< 该采样器须为 NonFiltering（深度纹理配非过滤采样器，如点光 cube 阴影）
    // BindGroup 端实际资源（BGL 端忽略）：
    WGPUBuffer buffer = nullptr; uint64_t buf_offset = 0; uint64_t buf_size = 0;
    WGPUSampler sampler = nullptr;
    WGPUTextureView view = nullptr;
};

// ============================================================
// 枚举/格式映射 + 资源小工具（inline：跨多个 webgpu TU 共享单一定义）
// ============================================================

inline constexpr uint64_t AlignUp4(uint64_t n) { return (n + 3u) & ~static_cast<uint64_t>(3u); }

/// 解析 WGSL 源中实际声明的 `@group(N) @binding(M)`，填入 out（key=(group<<16)|binding）。
/// 供 explicit pipeline-layout/BindGroup 过滤：仅纳入着色器真正使用的绑定，避免引擎多绑资源
/// 超 per-stage 上限 / 与着色器用量不符。render（vs/fs）与 compute 程序共用此解析。
inline void ParseWgslBindings(const std::string& src, std::set<uint32_t>& out) {
    for (size_t pos = src.find("@group("); pos != std::string::npos;
         pos = src.find("@group(", pos + 1)) {
        const size_t g0 = pos + 7;
        const size_t g1 = src.find(')', g0);
        if (g1 == std::string::npos) break;
        const size_t bpos = src.find("@binding(", g1);
        if (bpos == std::string::npos) break;
        // @binding 须紧随同一声明（其间只允许空白），否则视为不同声明。
        if (src.find_first_not_of(" \t\r\n", g1 + 1) != bpos) continue;
        const size_t b0 = bpos + 9;
        const size_t b1 = src.find(')', b0);
        if (b1 == std::string::npos) break;
        const uint32_t group = static_cast<uint32_t>(std::strtoul(src.c_str() + g0, nullptr, 10));
        const uint32_t binding = static_cast<uint32_t>(std::strtoul(src.c_str() + b0, nullptr, 10));
        out.insert((group << 16) | binding);
    }
}

inline WGPUVertexFormat ToVertexFormat(uint32_t components) {
    switch (components) {
        case 1:  return WGPUVertexFormat_Float32;
        case 2:  return WGPUVertexFormat_Float32x2;
        case 3:  return WGPUVertexFormat_Float32x3;
        default: return WGPUVertexFormat_Float32x4;
    }
}

inline WGPUTextureViewDimension ToViewDimension(TextureDim dim) {
    switch (dim) {
        case TextureDim::TexCube:    return WGPUTextureViewDimension_Cube;
        case TextureDim::Tex2DArray: return WGPUTextureViewDimension_2DArray;
        case TextureDim::Tex3D:      return WGPUTextureViewDimension_3D;
        case TextureDim::Tex2D:
        default:                     return WGPUTextureViewDimension_2D;
    }
}

inline WGPUPrimitiveTopology ToTopology(PrimitiveTopology t) {
    switch (t) {
        case PrimitiveTopology::LineStrip: return WGPUPrimitiveTopology_LineStrip;
        case PrimitiveTopology::LineList:  return WGPUPrimitiveTopology_LineList;
        case PrimitiveTopology::PointList: return WGPUPrimitiveTopology_PointList;
        case PrimitiveTopology::TriangleList:
        default:                           return WGPUPrimitiveTopology_TriangleList;
    }
}

inline WGPUCullMode ToCullMode(CullFace c) {
    switch (c) {
        case CullFace::Front: return WGPUCullMode_Front;
        case CullFace::Back:  return WGPUCullMode_Back;
        case CullFace::None:
        case CullFace::FrontAndBack:  // WebGPU 无 FrontAndBack；退化为 None（双面不剔除）
        default:              return WGPUCullMode_None;
    }
}

inline WGPUCompareFunction ToCompareFunc(CompareFunc f) {
    switch (f) {
        case CompareFunc::Never:        return WGPUCompareFunction_Never;
        case CompareFunc::Less:         return WGPUCompareFunction_Less;
        case CompareFunc::Equal:        return WGPUCompareFunction_Equal;
        case CompareFunc::LessEqual:    return WGPUCompareFunction_LessEqual;
        case CompareFunc::Greater:      return WGPUCompareFunction_Greater;
        case CompareFunc::NotEqual:     return WGPUCompareFunction_NotEqual;
        case CompareFunc::GreaterEqual: return WGPUCompareFunction_GreaterEqual;
        case CompareFunc::Always:
        default:                        return WGPUCompareFunction_Always;
    }
}

inline WGPUBlendFactor ToBlendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero:             return WGPUBlendFactor_Zero;
        case BlendFactor::One:              return WGPUBlendFactor_One;
        case BlendFactor::SrcAlpha:         return WGPUBlendFactor_SrcAlpha;
        case BlendFactor::OneMinusSrcAlpha: return WGPUBlendFactor_OneMinusSrcAlpha;
        case BlendFactor::DstAlpha:         return WGPUBlendFactor_DstAlpha;
        case BlendFactor::OneMinusDstAlpha: return WGPUBlendFactor_OneMinusDstAlpha;
        case BlendFactor::SrcColor:         return WGPUBlendFactor_Src;
        case BlendFactor::OneMinusSrcColor: return WGPUBlendFactor_OneMinusSrc;
        case BlendFactor::DstColor:         return WGPUBlendFactor_Dst;
        case BlendFactor::OneMinusDstColor: return WGPUBlendFactor_OneMinusDst;
        default:                            return WGPUBlendFactor_One;
    }
}

inline bool IsDepthFormat(WGPUTextureFormat f) {
    return f == WGPUTextureFormat_Depth32Float || f == WGPUTextureFormat_Depth24Plus ||
           f == WGPUTextureFormat_Depth24PlusStencil8 || f == WGPUTextureFormat_Depth16Unorm ||
           f == WGPUTextureFormat_Depth32FloatStencil8;
}

inline WGPUAddressMode ToAddressMode(TextureWrap w) {
    return w == TextureWrap::ClampToEdge ? WGPUAddressMode_ClampToEdge : WGPUAddressMode_Repeat;
}
inline WGPUFilterMode ToFilterMode(TextureFilter f) {
    return f == TextureFilter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
}

/// 全 mip 链层数（2D 维度，向下取整 log2(max(w,h))+1）。
inline uint32_t FullMipCount(uint32_t w, uint32_t h) {
    uint32_t m = (w > h ? w : h);
    uint32_t levels = 1;
    while (m > 1) { m >>= 1; ++levels; }
    return levels;
}

/// 向 mipLevel=0..1 的 2D 纹理写入一层 RGBA8 数据（origin.z 指定 cube 面 / 3D 切片）。
inline void WriteTextureLayerRGBA8(WGPUQueue queue, WGPUTexture tex, uint32_t mip_level,
                                   uint32_t width, uint32_t height, uint32_t z,
                                   const unsigned char* rgba8) {
    if (!rgba8) return;
    WGPUImageCopyTexture dst{};
    dst.texture = tex;
    dst.mipLevel = mip_level;
    dst.origin = WGPUOrigin3D{0, 0, z};
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout{};
    layout.offset = 0;
    layout.bytesPerRow = width * 4u;
    layout.rowsPerImage = height;
    WGPUExtent3D extent{width, height, 1u};
    wgpuQueueWriteTexture(queue, &dst, rgba8, static_cast<size_t>(width) * height * 4u, &layout, &extent);
}

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_COMMON_H
