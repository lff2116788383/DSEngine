/**
 * @file dds_parser.h
 * @brief DDS (DirectDraw Surface) 容器解析 —— 提取 BCn 压缩纹理数据用于 GPU 直传。
 *
 * 解析出的压缩块数据按原样交给 RHI（glCompressedTexImage2D / D3D11 BC* /
 * VK_FORMAT_BC*），不在 CPU 端解压为 RGBA，从而省去解压开销并把显存占用降低
 * 约 1/4 ~ 1/3（BCn 4~8 bpp vs RGBA8 32 bpp）。
 *
 * 从 asset_manager.cpp 的匿名命名空间提取为独立可单测单元（见
 * tests/gtest/unit/engine/dds_parser_test.cpp）。
 */

#ifndef DSE_ASSETS_DDS_PARSER_H
#define DSE_ASSETS_DDS_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

#include "engine/render/rhi/rhi_types.h"

namespace dse::assets {

/**
 * @brief 解析 DDS 文件字节流，输出 BCn 压缩格式与各 mip 级别。
 *
 * 支持经典 FourCC（DXT1/DXT3/DXT5、ATI1/BC4U、ATI2/BC5U）与 DX10 扩展头
 * （BC1/2/3/4/5/7，含 sRGB 变体）。
 *
 * @param file_data  DDS 文件完整字节（含 "DDS " 魔数）。
 * @param out_format 解析出的压缩格式。
 * @param out_mips   各 mip 级别；其中 data 指针指向 file_data 内部，
 *                   生命周期与 file_data 绑定（调用方需保证 file_data 存活）。
 * @param out_width  顶层宽。
 * @param out_height 顶层高。
 * @return 成功解析且至少含一个 mip 时返回 true。
 */
bool ParseDds(const std::vector<uint8_t>& file_data,
              CompressedTextureFormat& out_format,
              std::vector<CompressedMipLevel>& out_mips,
              int& out_width, int& out_height);

/// 路径扩展名是否为 .dds（大小写均接受）。
bool HasDdsExtension(const std::string& path);

} // namespace dse::assets

#endif // DSE_ASSETS_DDS_PARSER_H
