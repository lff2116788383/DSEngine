#ifndef DSE_LUT_LOADER_H
#define DSE_LUT_LOADER_H

#include <string>
#include <vector>

namespace dse {
namespace assets {

struct LutData {
    int size = 0;              // LUT 边长 (e.g. 32 for a 32x32x32 LUT)
    std::vector<unsigned char> rgba8;  // width*height*depth*4 bytes, row-major, slice-major
};

/// 从 .cube 文件加载 3D LUT 数据
/// 返回 true 表示成功，data 填入 LUT 像素数据（RGBA8 格式）
bool LoadCubeLut(const std::string& path, LutData& data);

/// 生成默认 identity LUT（不改变任何颜色）
LutData GenerateIdentityLut(int size = 32);

} // namespace assets
} // namespace dse

#endif // DSE_LUT_LOADER_H
