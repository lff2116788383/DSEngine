/**
 * @file procedural_generator.cpp
 * @brief 程序化生成实现：噪声函数 + 地形/散布生成器
 */

#include "engine/procedural/procedural_generator.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace dse {
namespace procedural {

// ─── PCG Random ─────────────────────────────────────────────────────────────

PCGRandom::PCGRandom(uint64_t seed) {
    state_ = 0;
    inc_ = (seed << 1u) | 1u;
    Next();
    state_ += seed;
    Next();
}

uint32_t PCGRandom::Next() {
    uint64_t old_state = state_;
    state_ = old_state * 6364136223846793005ULL + inc_;
    uint32_t xorshifted = static_cast<uint32_t>(((old_state >> 18u) ^ old_state) >> 27u);
    uint32_t rot = static_cast<uint32_t>(old_state >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
}

float PCGRandom::NextFloat() {
    return static_cast<float>(Next()) / 4294967296.0f;
}

float PCGRandom::Range(float min_val, float max_val) {
    return min_val + NextFloat() * (max_val - min_val);
}

// ─── Hash helpers ───────────────────────────────────────────────────────────

namespace {

inline uint32_t Hash2D(int x, int y, uint32_t seed) {
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(x) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h ^= static_cast<uint32_t>(y) * 0x1b873593;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return h;
}

inline float GradDot2D(uint32_t hash, float dx, float dy) {
    // 8 gradient directions
    switch (hash & 7) {
        case 0: return  dx + dy;
        case 1: return -dx + dy;
        case 2: return  dx - dy;
        case 3: return -dx - dy;
        case 4: return  dx;
        case 5: return -dx;
        case 6: return  dy;
        case 7: return -dy;
    }
    return 0.0f;
}

inline float Fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

} // anonymous namespace

// ─── Perlin Noise 2D ────────────────────────────────────────────────────────

float PerlinNoise2D(float x, float y, uint32_t seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - ix;
    float fy = y - iy;

    float u = Fade(fx);
    float v = Fade(fy);

    float n00 = GradDot2D(Hash2D(ix, iy, seed), fx, fy);
    float n10 = GradDot2D(Hash2D(ix + 1, iy, seed), fx - 1.0f, fy);
    float n01 = GradDot2D(Hash2D(ix, iy + 1, seed), fx, fy - 1.0f);
    float n11 = GradDot2D(Hash2D(ix + 1, iy + 1, seed), fx - 1.0f, fy - 1.0f);

    return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);
}

// ─── Simplex Noise 2D ───────────────────────────────────────────────────────

float SimplexNoise2D(float x, float y, uint32_t seed) {
    // Skew 2D → simplex grid
    constexpr float F2 = 0.3660254038f; // (sqrt(3)-1)/2
    constexpr float G2 = 0.2113248654f; // (3-sqrt(3))/6

    float s = (x + y) * F2;
    int i = static_cast<int>(std::floor(x + s));
    int j = static_cast<int>(std::floor(y + s));

    float t = (i + j) * G2;
    float x0 = x - (i - t);
    float y0 = y - (j - t);

    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else { i1 = 0; j1 = 1; }

    float x1 = x0 - i1 + G2;
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 >= 0.0f) {
        t0 *= t0;
        n0 = t0 * t0 * GradDot2D(Hash2D(i, j, seed), x0, y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 >= 0.0f) {
        t1 *= t1;
        n1 = t1 * t1 * GradDot2D(Hash2D(i + i1, j + j1, seed), x1, y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 >= 0.0f) {
        t2 *= t2;
        n2 = t2 * t2 * GradDot2D(Hash2D(i + 1, j + 1, seed), x2, y2);
    }

    return 70.0f * (n0 + n1 + n2);
}

// ─── Worley Noise 2D ────────────────────────────────────────────────────────

float WorleyNoise2D(float x, float y, uint32_t seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));

    float min_dist = 1e10f;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = ix + dx;
            int cy = iy + dy;

            // 确定性特征点位置
            uint32_t h = Hash2D(cx, cy, seed);
            float fx = static_cast<float>(cx) + static_cast<float>(h & 0xFFFF) / 65536.0f;
            float fy = static_cast<float>(cy) + static_cast<float>((h >> 16) & 0xFFFF) / 65536.0f;

            float dist = (x - fx) * (x - fx) + (y - fy) * (y - fy);
            min_dist = std::min(min_dist, dist);
        }
    }

    return std::sqrt(min_dist);
}

// ─── FBM ────────────────────────────────────────────────────────────────────

float FBM2D(float x, float y, const FBMParams& params) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = params.frequency;
    float max_value = 0.0f;

    for (int i = 0; i < params.octaves; ++i) {
        value += PerlinNoise2D(x * frequency, y * frequency, params.seed + i) * amplitude;
        max_value += amplitude;
        amplitude *= params.persistence;
        frequency *= params.lacunarity;
    }

    return value / max_value;
}

float DomainWarpedFBM(float x, float y, const FBMParams& params, float warp_strength) {
    // 第一层 warp
    float wx = FBM2D(x, y, params);
    float wy = FBM2D(x + 5.2f, y + 1.3f, params);

    // 第二层（可选，更自然）
    float wx2 = FBM2D(x + warp_strength * wx + 1.7f, y + warp_strength * wy + 9.2f, params);
    float wy2 = FBM2D(x + warp_strength * wx + 8.3f, y + warp_strength * wy + 2.8f, params);

    return FBM2D(x + warp_strength * wx2, y + warp_strength * wy2, params);
}

// ─── Terrain Generation ─────────────────────────────────────────────────────

void GenerateHeightmap(std::vector<float>& out_heights,
                       float world_origin_x, float world_origin_z,
                       int width, int height, float sample_spacing,
                       const TerrainGenConfig& config) {
    out_heights.resize(width * height);

    FBMParams fbm;
    fbm.octaves = config.octaves;
    fbm.frequency = config.base_frequency;
    fbm.lacunarity = config.lacunarity;
    fbm.persistence = config.persistence;
    fbm.seed = config.seed;

    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            float wx = world_origin_x + x * sample_spacing;
            float wz = world_origin_z + z * sample_spacing;

            // Domain warped FBM for natural terrain
            float h = DomainWarpedFBM(wx, wz, fbm, config.warp_strength);

            // Remap [-1,1] → [0,1]
            h = (h + 1.0f) * 0.5f;

            // 高原削顶
            if (h > config.plateau_threshold) {
                float excess = h - config.plateau_threshold;
                h = config.plateau_threshold + excess * 0.3f;
            }

            // 山谷加深
            h = std::pow(h, config.valley_power);

            // 应用高度缩放
            out_heights[z * width + x] = h * config.height_scale;
        }
    }
}

// ─── Scatter Generation ─────────────────────────────────────────────────────

void GenerateScatter(std::vector<ScatterInstance>& out_instances,
                     float world_origin_x, float world_origin_z,
                     float extent_x, float extent_z,
                     const ScatterConfig& config,
                     HeightQueryFunc height_func,
                     NormalQueryFunc normal_func) {
    out_instances.clear();
    if (config.types.empty() || !height_func) return;

    // Poisson disk 简化版：jittered grid
    PCGRandom rng(config.seed ^
                  static_cast<uint64_t>(*reinterpret_cast<const uint32_t*>(&world_origin_x)) ^
                  (static_cast<uint64_t>(*reinterpret_cast<const uint32_t*>(&world_origin_z)) << 32));

    float spacing = config.base_spacing;
    int nx = static_cast<int>(extent_x / spacing);
    int nz = static_cast<int>(extent_z / spacing);

    for (int gz = 0; gz < nz; ++gz) {
        for (int gx = 0; gx < nx; ++gx) {
            // 选择散布类型
            uint32_t type_idx = rng.Next() % static_cast<uint32_t>(config.types.size());
            const auto& scatter_type = config.types[type_idx];

            // 密度检查
            if (rng.NextFloat() > scatter_type.density) continue;

            // Jitter position
            float jx = rng.Range(-0.4f, 0.4f) * spacing;
            float jz = rng.Range(-0.4f, 0.4f) * spacing;
            float wx = world_origin_x + (gx + 0.5f) * spacing + jx;
            float wz = world_origin_z + (gz + 0.5f) * spacing + jz;

            // 获取高度
            float h = height_func(wx, wz);

            // 高度过滤
            if (h < scatter_type.min_height || h > scatter_type.max_height) continue;

            // 坡度过滤
            if (normal_func) {
                glm::vec3 n = normal_func(wx, wz);
                float slope_angle = std::acos(std::clamp(n.y, 0.0f, 1.0f)) * 57.2957795f;
                if (slope_angle > scatter_type.max_slope) continue;
            }

            // 生成实例
            ScatterInstance inst;
            inst.position = glm::vec3(wx, h, wz);
            inst.scale = rng.Range(scatter_type.min_scale, scatter_type.max_scale);
            inst.rotation.y = rng.Range(0.0f, config.random_rotation_y);
            inst.type_index = type_idx;

            // 对齐地面法线
            if (config.align_to_normal && normal_func) {
                glm::vec3 n = normal_func(wx, wz);
                inst.rotation.x = std::atan2(n.z, n.y) * 57.2957795f;
                inst.rotation.z = -std::atan2(n.x, n.y) * 57.2957795f;
            }

            out_instances.push_back(inst);
        }
    }
}

} // namespace procedural
} // namespace dse
