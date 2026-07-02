/**
 * @file texture_compressor.h
 * @brief 导入期纹理编码器：源 RGBA8 → BCn/ASTC 压缩块 + mip 链 → .dtex 容器字节。
 *
 * 编码后端：
 * - BC1/BC3/BC4/BC5: stb_dxt（公有领域，已在仓）
 * - BC7: 内建 mode-6 编码器（高质量 RGBA，无外部依赖）
 * - ASTC 4x4/6x6/8x8: 内建简化编码器（void-extent fallback + luminance-alpha 模式）
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

/// 判定是否支持该压缩格式的编码（BC1-BC7 + ASTC 4x4）。
bool IsTextureEncodeSupported(CompressedTextureFormat format);

/**
 * @brief 把 RGBA8 像素编码为压缩纹理并打包成 .dtex 字节流（含可选 mip 链）。
 *
 * @param rgba           顶层像素，行主序，每像素 4 字节（R,G,B,A）。
 * @param width          顶层宽（>0）。
 * @param height         顶层高（>0）。
 * @param format         目标压缩格式（须 IsTextureEncodeSupported）。
 * @param generate_mips  true 时用盒式滤波生成到 1x1 的完整 mip 链；false 仅 mip0。
 * @param high_quality   true 时启用高质量精修（BC1-5: stb_dxt HQ; BC7: 额外搜索迭代）。
 * @param out_bytes      输出完整 .dtex 文件字节（含 header + mip 描述表 + 块数据）。
 * @return 成功返回 true；输入非法或格式不支持返回 false。
 */
bool EncodeTextureToDtex(const uint8_t* rgba, int width, int height,
                         CompressedTextureFormat format, bool generate_mips,
                         bool high_quality, std::vector<uint8_t>& out_bytes);

/**
 * @brief 仅编码单个 mip 级别为压缩块（不打包容器）。供测试/底层复用。
 * @return 成功返回 true，out_blocks 为紧凑块数据。
 */
bool EncodeBCnLevel(const uint8_t* rgba, int width, int height,
                    CompressedTextureFormat format, bool high_quality,
                    std::vector<uint8_t>& out_blocks);

// Legacy alias
inline bool IsBCnEncodeSupported(CompressedTextureFormat format) { return IsTextureEncodeSupported(format); }

} // namespace dse::assets

#endif // DSE_ASSETS_TEXTURE_COMPRESSOR_H
