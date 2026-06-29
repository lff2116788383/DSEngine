/**
 * @file texture_compressor.h
 * @brief 导入期纹理 BCn 编码器：源 RGBA8 → BCn 压缩块 + mip 链 → .dtex 容器字节。
 *
 * MVP 后端基于已在仓的 stb_dxt（公有领域，零新依赖），支持 BC1/BC3/BC4/BC5
 * （含 sRGB 变体——sRGB 仅是格式 tag，块编码相同）。BC7/ASTC 需另接编码器，
 * 不在本 MVP 范围（EncodeTextureToDtex 对不支持的格式返回 false）。
 *
 * 产物与 dtex.h 的 ParseDtex / RHI CreateCompressedTexture2D 直接对接。
 */

#ifndef DSE_ASSETS_TEXTURE_COMPRESSOR_H
#define DSE_ASSETS_TEXTURE_COMPRESSOR_H

#include <cstdint>
#include <string>
#include <vector>

#include "engine/render/rhi/rhi_types.h"

namespace dse::assets {

/// stb_dxt MVP 支持的格式判定（BC1/BC3/BC4/BC5，含 sRGB）。
bool IsBCnEncodeSupported(CompressedTextureFormat format);

/**
 * @brief 把 RGBA8 像素编码为 BCn 并打包成 .dtex 字节流（含可选 mip 链）。
 *
 * @param rgba           顶层像素，行主序，每像素 4 字节（R,G,B,A）。
 * @param width          顶层宽（>0）。
 * @param height         顶层高（>0）。
 * @param format         目标压缩格式（须 IsBCnEncodeSupported）。
 * @param generate_mips  true 时用盒式滤波生成到 1x1 的完整 mip 链；false 仅 mip0。
 * @param high_quality   true 时启用 stb_dxt 高质量精修（约慢 30~40%）。
 * @param out_bytes      输出完整 .dtex 文件字节（含 header + mip 描述表 + 块数据）。
 * @return 成功返回 true；输入非法或格式不支持返回 false。
 */
bool EncodeTextureToDtex(const uint8_t* rgba, int width, int height,
                         CompressedTextureFormat format, bool generate_mips,
                         bool high_quality, std::vector<uint8_t>& out_bytes);

/**
 * @brief 仅编码单个 mip 级别为 BCn 块（不打包容器）。供测试/底层复用。
 * @return 成功返回 true，out_blocks 为紧凑块数据。
 */
bool EncodeBCnLevel(const uint8_t* rgba, int width, int height,
                    CompressedTextureFormat format, bool high_quality,
                    std::vector<uint8_t>& out_blocks);

} // namespace dse::assets

#endif // DSE_ASSETS_TEXTURE_COMPRESSOR_H
