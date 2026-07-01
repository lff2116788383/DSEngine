/**
 * @file net_bitwriter.h
 * @brief 位级网络序列化：NetBitWriter / NetBitReader + 量化工具。
 *
 * 替代字节对齐的 byte_stream.h 用于高频复制，支持：
 *   - 任意位宽整数（1~32 bit）
 *   - 定点位置量化（配置精度与范围）
 *   - 四元数 smallest-three 压缩（9+9+9+2 = 29 bit）
 *   - bool 1 bit
 *
 * 所有目标平台为小端；不做跨端字节序转换。
 */
#ifndef DSE_NET_SERIALIZE_NET_BITWRITER_H
#define DSE_NET_SERIALIZE_NET_BITWRITER_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

namespace dse::net {

// ─── NetBitWriter ───────────────────────────────────────────────────────────

class NetBitWriter {
public:
    void WriteBits(uint32_t value, int bits) {
        for (int i = 0; i < bits; ++i) {
            if (bit_pos_ == 0) buf_.push_back(0);
            if (value & (1u << i)) buf_.back() |= (1u << bit_pos_);
            if (++bit_pos_ == 8) bit_pos_ = 0;
        }
    }

    void WriteBool(bool v)       { WriteBits(v ? 1 : 0, 1); }
    void WriteU8(uint8_t v)      { WriteBits(v, 8); }
    void WriteU16(uint16_t v)    { WriteBits(v, 16); }
    void WriteU32(uint32_t v)    { WriteBits(v & 0xFFFF, 16); WriteBits(v >> 16, 16); }

    void WriteF32(float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, 4);
        WriteU32(bits);
    }

    /// 量化浮点到 N 位无符号整数（[min, max] 映射到 [0, 2^bits-1]）
    void WriteQuantized(float value, float min_val, float max_val, int bits) {
        float clamped = std::clamp(value, min_val, max_val);
        float normalized = (clamped - min_val) / (max_val - min_val);
        uint32_t max_int = (1u << bits) - 1;
        uint32_t quantized = static_cast<uint32_t>(normalized * max_int + 0.5f);
        WriteBits(quantized, bits);
    }

    /// 四元数 smallest-three 压缩（29 bits：2 bit index + 3×9 bit 分量）
    void WriteQuatSmallestThree(float x, float y, float z, float w) {
        float components[4] = {x, y, z, w};
        int largest_idx = 0;
        float largest_val = std::abs(components[0]);
        for (int i = 1; i < 4; ++i) {
            float av = std::abs(components[i]);
            if (av > largest_val) { largest_val = av; largest_idx = i; }
        }
        // 确保最大分量为正（翻转符号不影响旋转）
        float sign = components[largest_idx] < 0.0f ? -1.0f : 1.0f;

        WriteBits(static_cast<uint32_t>(largest_idx), 2);
        int written = 0;
        for (int i = 0; i < 4; ++i) {
            if (i == largest_idx) continue;
            // 其余分量范围 [-0.7071, 0.7071]，量化到 9 bit
            float val = components[i] * sign;
            float normalized = (val + 0.7071068f) / 1.4142136f;
            normalized = std::clamp(normalized, 0.0f, 1.0f);
            uint32_t q = static_cast<uint32_t>(normalized * 511.0f + 0.5f);
            WriteBits(q, 9);
            ++written;
        }
    }

    const uint8_t* data() const { return buf_.data(); }
    size_t size_bytes() const { return buf_.size(); }
    size_t size_bits() const { return buf_.empty() ? 0 : (buf_.size() - 1) * 8 + (bit_pos_ == 0 ? 8 : bit_pos_); }
    void clear() { buf_.clear(); bit_pos_ = 0; }

private:
    std::vector<uint8_t> buf_;
    int bit_pos_ = 0;  // 当前字节内已用的位数 (0..7)
};

// ─── NetBitReader ───────────────────────────────────────────────────────────

class NetBitReader {
public:
    NetBitReader(const void* data, size_t size_bytes)
        : data_(static_cast<const uint8_t*>(data)), total_bits_(size_bytes * 8) {}

    uint32_t ReadBits(int bits) {
        uint32_t value = 0;
        for (int i = 0; i < bits; ++i) {
            if (bit_pos_ >= total_bits_) { ok_ = false; return 0; }
            size_t byte_idx = bit_pos_ / 8;
            int bit_idx = bit_pos_ % 8;
            if (data_[byte_idx] & (1u << bit_idx)) value |= (1u << i);
            ++bit_pos_;
        }
        return value;
    }

    bool     ReadBool()  { return ReadBits(1) != 0; }
    uint8_t  ReadU8()    { return static_cast<uint8_t>(ReadBits(8)); }
    uint16_t ReadU16()   { return static_cast<uint16_t>(ReadBits(16)); }
    uint32_t ReadU32()   { uint32_t lo = ReadBits(16); uint32_t hi = ReadBits(16); return lo | (hi << 16); }

    float ReadF32() {
        uint32_t bits = ReadU32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    }

    float ReadQuantized(float min_val, float max_val, int bits) {
        uint32_t max_int = (1u << bits) - 1;
        uint32_t quantized = ReadBits(bits);
        float normalized = static_cast<float>(quantized) / static_cast<float>(max_int);
        return min_val + normalized * (max_val - min_val);
    }

    void ReadQuatSmallestThree(float& x, float& y, float& z, float& w) {
        int largest_idx = static_cast<int>(ReadBits(2));
        float components[4] = {0, 0, 0, 0};
        float sum_sq = 0.0f;
        int read_idx = 0;
        for (int i = 0; i < 4; ++i) {
            if (i == largest_idx) continue;
            uint32_t q = ReadBits(9);
            float normalized = static_cast<float>(q) / 511.0f;
            float val = normalized * 1.4142136f - 0.7071068f;
            components[i] = val;
            sum_sq += val * val;
            ++read_idx;
        }
        components[largest_idx] = std::sqrt(std::max(0.0f, 1.0f - sum_sq));
        x = components[0]; y = components[1]; z = components[2]; w = components[3];
    }

    bool ok() const { return ok_; }
    size_t bits_read() const { return bit_pos_; }
    size_t bits_remaining() const { return ok_ ? (total_bits_ - bit_pos_) : 0; }

private:
    const uint8_t* data_;
    size_t total_bits_;
    size_t bit_pos_ = 0;
    bool ok_ = true;
};

} // namespace dse::net

#endif // DSE_NET_SERIALIZE_NET_BITWRITER_H
