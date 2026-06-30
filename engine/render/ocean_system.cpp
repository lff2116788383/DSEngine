/**
 * @file ocean_system.cpp
 * @brief 大规模海洋系统实现 — FFT 波谱模拟 + Tile LOD
 */

#include "engine/render/ocean_system.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace dse {
namespace render {

static constexpr float PI = 3.14159265358979323846f;
static constexpr float TWO_PI = 2.0f * PI;

void OceanSystem::Init(const OceanConfig& config) {
    config_ = config;
    int N = config_.fft_resolution;

    // Setup LOD levels
    lod_levels_.resize(config_.lod_levels);
    for (int i = 0; i < config_.lod_levels; ++i) {
        auto& lod = lod_levels_[i];
        lod.grid_size = N >> i;
        lod.tile_size = config_.tile_size * static_cast<float>(1 << i);
        lod.max_distance = config_.tile_size * config_.tile_count * static_cast<float>(1 << i);
    }

    // Allocate tile data
    tile_data_.heights.resize(N * N, 0.0f);
    tile_data_.normals.resize(N * N, glm::vec3(0, 1, 0));
    tile_data_.displacement.resize(N * N, glm::vec2(0.0f));
    tile_data_.foam.resize(N * N, 0.0f);

    // Allocate spectrum data
    h0_.resize(N * N);
    h0_conj_.resize(N * N);
    omega_.resize(N * N);
    ht_.resize(N * N);
    dx_spectrum_.resize(N * N);
    dz_spectrum_.resize(N * N);

    InitSpectrum();
    initialized_ = true;
}

void OceanSystem::Shutdown() {
    tile_data_ = {};
    h0_.clear(); h0_conj_.clear(); omega_.clear();
    ht_.clear(); dx_spectrum_.clear(); dz_spectrum_.clear();
    lod_levels_.clear();
    initialized_ = false;
}

void OceanSystem::InitSpectrum() {
    int N = config_.fft_resolution;
    float L = config_.tile_size;

    std::mt19937 rng(42);
    std::normal_distribution<float> gauss(0.0f, 1.0f);

    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = z * N + x;

            // Wave vector
            float kx = TWO_PI * (x - N / 2) / L;
            float kz = TWO_PI * (z - N / 2) / L;
            glm::vec2 k(kx, kz);
            float k_len = glm::length(k);

            // Angular frequency
            omega_[idx] = std::sqrt(config_.gravity * k_len);

            // Phillips spectrum
            float spectrum_val = 0.0f;
            if (k_len > 0.0001f) {
                spectrum_val = (config_.spectrum == OceanSpectrumType::JONSWAP)
                    ? JONSWAPSpectrum(k) : PhillipsSpectrum(k);
            }

            // h0(k) = 1/sqrt(2) * (ξr + iξi) * sqrt(Ph(k))
            float scale = std::sqrt(spectrum_val * 0.5f);
            h0_[idx] = std::complex<float>(gauss(rng) * scale, gauss(rng) * scale);

            // h0*(-k)
            int mx = (N - x) % N;
            int mz = (N - z) % N;
            int conj_idx = mz * N + mx;
            h0_conj_[idx] = std::conj(h0_[conj_idx < N * N ? conj_idx : 0]);
        }
    }

    // Fix conjugate symmetry
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = z * N + x;
            int mx = (N - x) % N;
            int mz = (N - z) % N;
            int conj_idx = mz * N + mx;
            h0_conj_[idx] = std::conj(h0_[conj_idx]);
        }
    }
}

float OceanSystem::PhillipsSpectrum(const glm::vec2& k) const {
    float k_len = glm::length(k);
    if (k_len < 0.0001f) return 0.0f;

    float k2 = k_len * k_len;
    float k4 = k2 * k2;

    float wind_speed = config_.wind_speed;
    float L_param = wind_speed * wind_speed / config_.gravity;  // Largest wave
    float L2 = L_param * L_param;

    glm::vec2 k_norm = k / k_len;
    glm::vec2 w_norm = glm::normalize(config_.wind_direction);
    float k_dot_w = glm::dot(k_norm, w_norm);

    // Phillips spectrum
    float Ph = config_.amplitude * std::exp(-1.0f / (k2 * L2)) / k4;
    Ph *= k_dot_w * k_dot_w;  // Directional spread

    // Suppress waves against wind
    if (k_dot_w < 0.0f) Ph *= 0.07f;

    // Suppress very small wavelengths
    float l = L_param * 0.001f;
    Ph *= std::exp(-k2 * l * l);

    return Ph;
}

float OceanSystem::JONSWAPSpectrum(const glm::vec2& k) const {
    float base = PhillipsSpectrum(k);
    // JONSWAP adds peak enhancement
    float k_len = glm::length(k);
    if (k_len < 0.0001f) return 0.0f;

    float omega = std::sqrt(config_.gravity * k_len);
    float omega_peak = 0.87f * config_.gravity / config_.wind_speed;

    float sigma = (omega <= omega_peak) ? 0.07f : 0.09f;
    float r = std::exp(-((omega - omega_peak) * (omega - omega_peak)) /
                       (2.0f * sigma * sigma * omega_peak * omega_peak));

    float gamma = 3.3f;  // JONSWAP peak enhancement factor
    return base * std::pow(gamma, r);
}

void OceanSystem::ComputeFFT(float time) {
    int N = config_.fft_resolution;

    // Compute h(k,t) from h0(k)
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = z * N + x;
            float w = omega_[idx];
            float cos_wt = std::cos(w * time);
            float sin_wt = std::sin(w * time);
            std::complex<float> exp_iwt(cos_wt, sin_wt);
            std::complex<float> exp_niwt(cos_wt, -sin_wt);

            ht_[idx] = h0_[idx] * exp_iwt + h0_conj_[idx] * exp_niwt;

            // Displacement spectra
            float kx = TWO_PI * (x - N / 2) / config_.tile_size;
            float kz = TWO_PI * (z - N / 2) / config_.tile_size;
            float k_len = std::sqrt(kx * kx + kz * kz);
            if (k_len > 0.0001f) {
                dx_spectrum_[idx] = std::complex<float>(0, -kx / k_len) * ht_[idx];
                dz_spectrum_[idx] = std::complex<float>(0, -kz / k_len) * ht_[idx];
            } else {
                dx_spectrum_[idx] = {0, 0};
                dz_spectrum_[idx] = {0, 0};
            }
        }
    }

    // IDFT to get spatial domain (simplified CPU fallback)
    DFT2D(ht_, N, true);
    DFT2D(dx_spectrum_, N, true);
    DFT2D(dz_spectrum_, N, true);

    // Store results
    float max_h = 0.0f;
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = z * N + x;
            float sign = ((x + z) % 2 == 0) ? 1.0f : -1.0f;

            tile_data_.heights[idx] = ht_[idx].real() * sign;
            tile_data_.displacement[idx] = glm::vec2(
                dx_spectrum_[idx].real() * sign * config_.choppiness,
                dz_spectrum_[idx].real() * sign * config_.choppiness
            );

            max_h = std::max(max_h, std::abs(tile_data_.heights[idx]));
        }
    }

    // Compute normals from heights
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            int idx = z * N + x;
            float hL = tile_data_.heights[z * N + ((x - 1 + N) % N)];
            float hR = tile_data_.heights[z * N + ((x + 1) % N)];
            float hD = tile_data_.heights[((z - 1 + N) % N) * N + x];
            float hU = tile_data_.heights[((z + 1) % N) * N + x];

            float cell = config_.tile_size / N;
            glm::vec3 n(-((hR - hL) / (2.0f * cell)), 1.0f, -((hU - hD) / (2.0f * cell)));
            tile_data_.normals[idx] = glm::normalize(n);
        }
    }

    tile_data_.jacobian_min = max_h;
}

void OceanSystem::DFT2D(std::vector<std::complex<float>>& data, int N, bool inverse) {
    // Simplified row/column DFT (O(N²) per row; for real usage, use FFT)
    // For test/demo purposes, we use a downsampled approach
    std::vector<std::complex<float>> temp(N);
    float sign = inverse ? 1.0f : -1.0f;
    float scale = inverse ? 1.0f / N : 1.0f;

    // Transform rows
    for (int row = 0; row < N; ++row) {
        for (int k = 0; k < N; ++k) {
            std::complex<float> sum(0, 0);
            for (int n = 0; n < N; ++n) {
                float angle = sign * TWO_PI * k * n / N;
                sum += data[row * N + n] * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            temp[k] = sum * scale;
        }
        for (int k = 0; k < N; ++k) data[row * N + k] = temp[k];
    }

    // Transform columns
    for (int col = 0; col < N; ++col) {
        for (int k = 0; k < N; ++k) {
            std::complex<float> sum(0, 0);
            for (int n = 0; n < N; ++n) {
                float angle = sign * TWO_PI * k * n / N;
                sum += data[n * N + col] * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            temp[k] = sum * scale;
        }
        for (int k = 0; k < N; ++k) data[k * N + col] = temp[k];
    }
}

void OceanSystem::UpdateFoam(float dt) {
    int N = config_.fft_resolution;
    for (int i = 0; i < N * N; ++i) {
        // Foam generated where Jacobian is negative (wave breaking)
        float disp_x = tile_data_.displacement[i].x;
        float disp_z = tile_data_.displacement[i].y;
        float jacobian = 1.0f + disp_x + disp_z; // Simplified Jacobian

        if (jacobian < config_.foam_threshold) {
            tile_data_.foam[i] = std::min(1.0f, tile_data_.foam[i] + 0.5f);
        } else {
            tile_data_.foam[i] *= config_.foam_decay;
        }
    }
}

void OceanSystem::Update(float time, const glm::vec3& camera_pos) {
    if (!initialized_) return;

    float dt = time - last_time_;
    last_time_ = time;

    ComputeFFT(time);
    UpdateFoam(dt);

    // Calculate visible tiles based on camera distance
    visible_tile_count_ = 0;
    for (int lod = 0; lod < static_cast<int>(lod_levels_.size()); ++lod) {
        int count = config_.tile_count >> lod;
        visible_tile_count_ += count * count;
    }
}

float OceanSystem::GetHeightAt(float world_x, float world_z) const {
    if (!initialized_) return 0.0f;
    int N = config_.fft_resolution;
    float cell = config_.tile_size / N;

    // Wrap into tile space
    float lx = std::fmod(world_x - origin_offset_.x, config_.tile_size);
    float lz = std::fmod(world_z - origin_offset_.z, config_.tile_size);
    if (lx < 0) lx += config_.tile_size;
    if (lz < 0) lz += config_.tile_size;

    // Bilinear interpolation
    float fx = lx / cell;
    float fz = lz / cell;
    int ix = static_cast<int>(fx) % N;
    int iz = static_cast<int>(fz) % N;
    float tx = fx - std::floor(fx);
    float tz = fz - std::floor(fz);

    float h00 = tile_data_.heights[iz * N + ix];
    float h10 = tile_data_.heights[iz * N + (ix + 1) % N];
    float h01 = tile_data_.heights[((iz + 1) % N) * N + ix];
    float h11 = tile_data_.heights[((iz + 1) % N) * N + (ix + 1) % N];

    float h0 = h00 * (1 - tx) + h10 * tx;
    float h1 = h01 * (1 - tx) + h11 * tx;
    return h0 * (1 - tz) + h1 * tz;
}

glm::vec3 OceanSystem::GetNormalAt(float world_x, float world_z) const {
    if (!initialized_) return glm::vec3(0, 1, 0);
    int N = config_.fft_resolution;
    float cell = config_.tile_size / N;

    float lx = std::fmod(world_x - origin_offset_.x, config_.tile_size);
    float lz = std::fmod(world_z - origin_offset_.z, config_.tile_size);
    if (lx < 0) lx += config_.tile_size;
    if (lz < 0) lz += config_.tile_size;

    int ix = static_cast<int>(lx / cell) % N;
    int iz = static_cast<int>(lz / cell) % N;
    return tile_data_.normals[iz * N + ix];
}

float OceanSystem::GetFoamAt(float world_x, float world_z) const {
    if (!initialized_) return 0.0f;
    int N = config_.fft_resolution;
    float cell = config_.tile_size / N;

    float lx = std::fmod(world_x - origin_offset_.x, config_.tile_size);
    float lz = std::fmod(world_z - origin_offset_.z, config_.tile_size);
    if (lx < 0) lx += config_.tile_size;
    if (lz < 0) lz += config_.tile_size;

    int ix = static_cast<int>(lx / cell) % N;
    int iz = static_cast<int>(lz / cell) % N;
    return tile_data_.foam[iz * N + ix];
}

void OceanSystem::SetWind(float speed, float dir_x, float dir_z) {
    config_.wind_speed = speed;
    config_.wind_direction = glm::normalize(glm::vec2(dir_x, dir_z));
    InitSpectrum();
}

void OceanSystem::RebaseOrigin(const glm::vec3& offset) {
    origin_offset_ += offset;
}

OceanSystem::OceanStats OceanSystem::GetStats() const {
    return {
        static_cast<uint32_t>(config_.tile_count * config_.tile_count),
        visible_tile_count_,
        static_cast<uint32_t>(config_.fft_resolution),
        tile_data_.jacobian_min
    };
}

} // namespace render
} // namespace dse
