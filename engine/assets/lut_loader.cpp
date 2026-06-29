#include "lut_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace dse {
namespace assets {

bool LoadCubeLut(const std::string& path, LutData& data) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    int lut_size = 0;
    std::vector<float> rgb_floats;

    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') continue;

        // 去除前导空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // 解析 LUT_3D_SIZE
        if (line.rfind("LUT_3D_SIZE", 0) == 0) {
            std::istringstream iss(line.substr(11));
            iss >> lut_size;
            // .cube 规范上限 256；夹住防止下方 lut_size^3*3 的 int 乘法溢出 (UB) 与超大分配。
            if (lut_size < 0 || lut_size > 256) return false;
            continue;
        }

        // 跳过其他关键字行 (TITLE, DOMAIN_MIN, DOMAIN_MAX 等)
        if (std::isalpha(static_cast<unsigned char>(line[0]))) continue;

        // 解析 RGB 浮点值
        float r, g, b;
        std::istringstream iss(line);
        if (iss >> r >> g >> b) {
            rgb_floats.push_back(r);
            rgb_floats.push_back(g);
            rgb_floats.push_back(b);
        }
    }

    if (lut_size <= 0 || static_cast<int>(rgb_floats.size()) != lut_size * lut_size * lut_size * 3) {
        return false;
    }

    data.size = lut_size;
    data.rgba8.resize(lut_size * lut_size * lut_size * 4);

    for (int i = 0; i < lut_size * lut_size * lut_size; ++i) {
        float r = std::clamp(rgb_floats[i * 3 + 0], 0.0f, 1.0f);
        float g = std::clamp(rgb_floats[i * 3 + 1], 0.0f, 1.0f);
        float b = std::clamp(rgb_floats[i * 3 + 2], 0.0f, 1.0f);
        data.rgba8[i * 4 + 0] = static_cast<unsigned char>(r * 255.0f + 0.5f);
        data.rgba8[i * 4 + 1] = static_cast<unsigned char>(g * 255.0f + 0.5f);
        data.rgba8[i * 4 + 2] = static_cast<unsigned char>(b * 255.0f + 0.5f);
        data.rgba8[i * 4 + 3] = 255;
    }

    return true;
}

LutData GenerateIdentityLut(int size) {
    LutData data;
    data.size = size;
    data.rgba8.resize(size * size * size * 4);

    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                int idx = (b * size * size + g * size + r) * 4;
                data.rgba8[idx + 0] = static_cast<unsigned char>(static_cast<float>(r) / (size - 1) * 255.0f + 0.5f);
                data.rgba8[idx + 1] = static_cast<unsigned char>(static_cast<float>(g) / (size - 1) * 255.0f + 0.5f);
                data.rgba8[idx + 2] = static_cast<unsigned char>(static_cast<float>(b) / (size - 1) * 255.0f + 0.5f);
                data.rgba8[idx + 3] = 255;
            }
        }
    }

    return data;
}

} // namespace assets
} // namespace dse
