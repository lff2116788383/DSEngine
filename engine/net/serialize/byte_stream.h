/**
 * @file byte_stream.h
 * @brief 网络复制用的最小字节级读写器（MVP）。
 *
 * 字节对齐、小端序的 POD 序列化，区别于场景存档的 rapidjson（可读但体积大）。
 * MVP 阶段不做位级量化/压缩（见 NETWORK_LAYER_DESIGN.md §4.10、§8 Phase 进阶）；
 * 当前所有目标平台（x86/ARM 桌面与 Android）均为小端，故直接按内存布局写读。
 */
#ifndef DSE_NET_SERIALIZE_BYTE_STREAM_H
#define DSE_NET_SERIALIZE_BYTE_STREAM_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>

namespace dse::net {

/** 追加式字节写入器。 */
class ByteWriter {
public:
    void WriteU8(uint8_t v)   { buf_.push_back(v); }
    void WriteU16(uint16_t v) { Append(&v, sizeof v); }
    void WriteU32(uint32_t v) { Append(&v, sizeof v); }
    void WriteU64(uint64_t v) { Append(&v, sizeof v); }
    void WriteF32(float v)    { Append(&v, sizeof v); }
    void WriteBytes(const void* p, size_t n) { Append(p, n); }

    const uint8_t* data() const { return buf_.data(); }
    size_t         size() const { return buf_.size(); }
    void           clear()      { buf_.clear(); }

private:
    void Append(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        buf_.insert(buf_.end(), b, b + n);
    }
    std::vector<uint8_t> buf_;
};

/** 只读字节读取器，越界时置 ok=false（调用方应在解析后检查）。 */
class ByteReader {
public:
    ByteReader(const void* p, size_t n)
        : p_(static_cast<const uint8_t*>(p)), n_(n) {}

    bool   ok()        const { return ok_; }
    size_t remaining() const { return n_ - off_; }

    uint8_t  ReadU8()  { uint8_t  v = 0; Take(&v, sizeof v); return v; }
    uint16_t ReadU16() { uint16_t v = 0; Take(&v, sizeof v); return v; }
    uint32_t ReadU32() { uint32_t v = 0; Take(&v, sizeof v); return v; }
    uint64_t ReadU64() { uint64_t v = 0; Take(&v, sizeof v); return v; }
    float    ReadF32() { float    v = 0; Take(&v, sizeof v); return v; }

private:
    void Take(void* out, size_t n) {
        if (off_ + n > n_) { ok_ = false; return; }
        std::memcpy(out, p_ + off_, n);
        off_ += n;
    }
    const uint8_t* p_;
    size_t         n_;
    size_t         off_ = 0;
    bool           ok_  = true;
};

} // namespace dse::net

#endif // DSE_NET_SERIALIZE_BYTE_STREAM_H
