/**
 * @file tilemap_serializer.cpp
 * @brief .dtilemap 二进制序列化/反序列化实现
 */

#include "engine/ecs/tilemap.h"

#include <cstring>
#include <fstream>

namespace {

// ── 二进制写入辅助 ────────────────────────────────────────────────────────

class ByteWriter {
public:
    void Write(const void* data, size_t len) {
        const auto* p = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), p, p + len);
    }

    void WriteString(const std::string& s) {
        int32_t len = static_cast<int32_t>(s.size());
        Write(&len, sizeof(len));
        if (len > 0) Write(s.data(), static_cast<size_t>(len));
    }

    void WriteU8(uint8_t v) { Write(&v, sizeof(v)); }
    void WriteI32(int32_t v) { Write(&v, sizeof(v)); }
    void WriteF32(float v) { Write(&v, sizeof(v)); }

    const std::vector<uint8_t>& Data() const { return buffer_; }

private:
    std::vector<uint8_t> buffer_;
};

class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size)
        : data_(data), size_(size), pos_(0) {}

    bool Read(void* out, size_t len) {
        if (pos_ + len > size_) return false;
        std::memcpy(out, data_ + pos_, len);
        pos_ += len;
        return true;
    }

    bool ReadString(std::string& out) {
        int32_t len = 0;
        if (!Read(&len, sizeof(len))) return false;
        if (len < 0 || pos_ + static_cast<size_t>(len) > size_) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), static_cast<size_t>(len));
        pos_ += static_cast<size_t>(len);
        return true;
    }

    bool ReadU8(uint8_t& out) { return Read(&out, sizeof(out)); }
    bool ReadI32(int32_t& out) { return Read(&out, sizeof(out)); }
    bool ReadF32(float& out) { return Read(&out, sizeof(out)); }

    bool AtEnd() const { return pos_ >= size_; }
    size_t Pos() const { return pos_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;
};

constexpr const char* kMagic = "DTM1";
constexpr int32_t kVersion = 1;

} // anonymous namespace

std::vector<uint8_t> TilemapSerializer::Save(const TilemapComponent& tm,
                                              const std::string& tileset_path) {
    ByteWriter w;

    // Header
    w.Write(kMagic, 4);
    w.WriteI32(kVersion);
    w.WriteI32(tm.width);
    w.WriteI32(tm.height);
    w.WriteF32(tm.tile_size);
    w.WriteI32(tm.tileset_cols);
    w.WriteI32(tm.tileset_rows);
    w.WriteString(tileset_path);

    // Single-layer tiles (backward compat)
    int32_t tile_count = static_cast<int32_t>(tm.tiles.size());
    w.WriteI32(tile_count);
    for (int t : tm.tiles) w.WriteI32(t);

    // Layers
    int32_t layer_count = static_cast<int32_t>(tm.layers.size());
    w.WriteI32(layer_count);
    for (const auto& layer : tm.layers) {
        w.WriteString(layer.name);
        w.WriteF32(layer.opacity);
        w.WriteU8(layer.visible ? 1 : 0);
        w.WriteI32(layer.sorting_layer);
        w.WriteI32(layer.order_in_layer_base);
        w.WriteU8(layer.generate_colliders ? 1 : 0);
        w.WriteI32(layer.collider_tile_min);
        // tiles
        int32_t tile_count = static_cast<int32_t>(layer.tiles.size());
        w.WriteI32(tile_count);
        for (int t : layer.tiles) w.WriteI32(t);
    }

    // Animations
    int32_t anim_count = static_cast<int32_t>(tm.animations.size());
    w.WriteI32(anim_count);
    for (const auto& [tile_id, anim] : tm.animations) {
        w.WriteI32(tile_id);
        w.WriteU8(anim.loop ? 1 : 0);
        int32_t frame_count = static_cast<int32_t>(anim.frames.size());
        w.WriteI32(frame_count);
        for (const auto& f : anim.frames) {
            w.WriteI32(f.tile_id);
            w.WriteF32(f.duration);
        }
    }

    // Properties
    int32_t prop_count = static_cast<int32_t>(tm.properties.size());
    w.WriteI32(prop_count);
    for (const auto& [tile_id, prop] : tm.properties) {
        w.WriteI32(tile_id);
        w.WriteU8(prop.solid ? 1 : 0);
        w.WriteI32(prop.collision_type);
        w.WriteF32(prop.friction);
        w.WriteF32(prop.restitution);
        int32_t custom_count = static_cast<int32_t>(prop.custom.size());
        w.WriteI32(custom_count);
        for (const auto& [k, v] : prop.custom) {
            w.WriteString(k);
            w.WriteString(v);
        }
    }

    return w.Data();
}

bool TilemapSerializer::Load(const uint8_t* data, size_t size,
                             TilemapComponent& out_tm,
                             std::string& out_tileset_path) {
    ByteReader r(data, size);

    // Header
    char magic[4];
    if (!r.Read(magic, 4)) return false;
    if (std::memcmp(magic, kMagic, 4) != 0) return false;

    int32_t version = 0;
    if (!r.ReadI32(version) || version != kVersion) return false;

    if (!r.ReadI32(out_tm.width)) return false;
    if (!r.ReadI32(out_tm.height)) return false;
    if (!r.ReadF32(out_tm.tile_size)) return false;
    if (!r.ReadI32(out_tm.tileset_cols)) return false;
    if (!r.ReadI32(out_tm.tileset_rows)) return false;
    if (!r.ReadString(out_tileset_path)) return false;

    // Single-layer tiles (backward compat)
    int32_t tile_count = 0;
    if (!r.ReadI32(tile_count)) return false;
    out_tm.tiles.resize(static_cast<size_t>(tile_count));
    for (int t = 0; t < tile_count; ++t) {
        if (!r.ReadI32(out_tm.tiles[t])) return false;
    }

    // Layers
    int32_t layer_count = 0;
    if (!r.ReadI32(layer_count)) return false;
    out_tm.layers.clear();
    out_tm.layers.reserve(static_cast<size_t>(layer_count));
    for (int i = 0; i < layer_count; ++i) {
        TilemapLayer layer;
        if (!r.ReadString(layer.name)) return false;
        if (!r.ReadF32(layer.opacity)) return false;
        uint8_t vis = 0;
        if (!r.ReadU8(vis)) return false;
        layer.visible = vis != 0;
        if (!r.ReadI32(layer.sorting_layer)) return false;
        if (!r.ReadI32(layer.order_in_layer_base)) return false;
        uint8_t gc = 0;
        if (!r.ReadU8(gc)) return false;
        layer.generate_colliders = gc != 0;
        if (!r.ReadI32(layer.collider_tile_min)) return false;
        int32_t tile_count = 0;
        if (!r.ReadI32(tile_count)) return false;
        layer.tiles.resize(static_cast<size_t>(tile_count));
        for (int t = 0; t < tile_count; ++t) {
            if (!r.ReadI32(layer.tiles[t])) return false;
        }
        layer.width = out_tm.width;
        layer.height = out_tm.height;
        layer.dirty = true;
        out_tm.layers.push_back(std::move(layer));
    }

    // Animations
    int32_t anim_count = 0;
    if (!r.ReadI32(anim_count)) return false;
    out_tm.animations.clear();
    for (int i = 0; i < anim_count; ++i) {
        int32_t tile_id = 0;
        if (!r.ReadI32(tile_id)) return false;
        TileAnimation anim;
        uint8_t loop = 0;
        if (!r.ReadU8(loop)) return false;
        anim.loop = loop != 0;
        int32_t frame_count = 0;
        if (!r.ReadI32(frame_count)) return false;
        anim.frames.resize(static_cast<size_t>(frame_count));
        for (int f = 0; f < frame_count; ++f) {
            if (!r.ReadI32(anim.frames[f].tile_id)) return false;
            if (!r.ReadF32(anim.frames[f].duration)) return false;
        }
        anim.ComputeDuration();
        out_tm.animations[tile_id] = std::move(anim);
    }

    // Properties
    int32_t prop_count = 0;
    if (!r.ReadI32(prop_count)) return false;
    out_tm.properties.clear();
    for (int i = 0; i < prop_count; ++i) {
        int32_t tile_id = 0;
        if (!r.ReadI32(tile_id)) return false;
        TileProperties prop;
        uint8_t solid = 0;
        if (!r.ReadU8(solid)) return false;
        prop.solid = solid != 0;
        if (!r.ReadI32(prop.collision_type)) return false;
        if (!r.ReadF32(prop.friction)) return false;
        if (!r.ReadF32(prop.restitution)) return false;
        int32_t custom_count = 0;
        if (!r.ReadI32(custom_count)) return false;
        for (int c = 0; c < custom_count; ++c) {
            std::string key, val;
            if (!r.ReadString(key)) return false;
            if (!r.ReadString(val)) return false;
            prop.custom[key] = val;
        }
        out_tm.properties[tile_id] = std::move(prop);
    }

    out_tm.dirty = true;
    return true;
}

bool TilemapSerializer::LoadFromFile(const std::string& filepath,
                                    TilemapComponent& out_tm,
                                    std::string& out_tileset_path) {
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    if (!f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
        return false;
    return Load(data.data(), data.size(), out_tm, out_tileset_path);
}

bool TilemapSerializer::SaveToFile(const std::string& filepath,
                                   const TilemapComponent& tm,
                                   const std::string& tileset_path) {
    auto data = Save(tm, tileset_path);
    std::ofstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    return f.good();
}
