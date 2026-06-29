/**
 * @file dtex.h
 * @brief DSE 自有压缩纹理容器（.dtex）—— 导入期把源图 BCn 编码后落盘，运行期解析为
 *        GPU 直传的压缩块数据（复用 CompressedMipLevel / RHI CreateCompressedTexture2D），
 *        与 .dds 走完全相同的运行时上传路径。
 *
 * 设计与 dds_parser 对齐：解析仅输出指向 file_data 内部的 mip 视图，不做 CPU 解压，
 * 从而省去解压开销并把显存占用降到 RGBA8 的 1/4 ~ 1/6。
 *
 * 编码端见 texture_compressor.{h,cpp}（依赖 stb_dxt）。本文件只承担容器格式定义
 * 与运行期只读解析，运行时不依赖编码器。
 */

#ifndef DSE_ASSETS_DTEX_H
#define DSE_ASSETS_DTEX_H

#include <cstdint>
#include <string>
#include <vector>

#include "engine/render/rhi/rhi_types.h"

namespace dse::assets {

constexpr uint32_t kDtexMagic = 0x58455444u;  // 'D','T','E','X' little-endian
constexpr uint32_t kDtexVersion = 1u;

/// .dtex 文件头（紧凑布局，直接 memcpy 读写）。
#pragma pack(push, 1)
struct DtexHeader {
    uint32_t magic = kDtexMagic;
    uint32_t version = kDtexVersion;
    uint32_t format = 0;     ///< CompressedTextureFormat 的底层值
    uint32_t width = 0;      ///< 顶层宽
    uint32_t height = 0;     ///< 顶层高
    uint32_t mip_count = 0;  ///< mip 级别数（>=1）
    uint32_t flags = 0;      ///< 预留（bit0 可表 sRGB，信息性）
    uint32_t reserved = 0;
};

/// 每个 mip 的描述符表项，紧跟在 DtexHeader 之后，随后才是连续的块数据。
struct DtexMipDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t offset = 0;  ///< 相对文件起始的字节偏移
    uint32_t size = 0;    ///< 该 mip 的压缩块字节数
};
#pragma pack(pop)

/// 给定压缩格式，返回每 4x4 块的字节数（BC1/BC4 = 8，其余 = 16）。
uint32_t DtexBlockBytes(CompressedTextureFormat format);

/**
 * @brief 解析 .dtex 字节流，输出压缩格式与各 mip 视图。
 * @param file_data  完整文件字节（含 DtexHeader 魔数）。
 * @param out_format 解析出的压缩格式。
 * @param out_mips   各 mip；data 指针指向 file_data 内部，生命周期与 file_data 绑定。
 * @param out_width  顶层宽。
 * @param out_height 顶层高。
 * @return 头部合法且至少含一个 mip、且各 mip 字节范围都落在 file_data 内时返回 true。
 */
bool ParseDtex(const std::vector<uint8_t>& file_data,
               CompressedTextureFormat& out_format,
               std::vector<CompressedMipLevel>& out_mips,
               int& out_width, int& out_height);

/// 路径扩展名是否为 .dtex（大小写均接受）。
bool HasDtexExtension(const std::string& path);

} // namespace dse::assets

#endif // DSE_ASSETS_DTEX_H
