/**
 * @file repl_transform_codec.h
 * @brief TransformComponent 编解码辅助（全量快照用）。
 */
#ifndef DSE_NET_REPLICATION_REPL_TRANSFORM_CODEC_H
#define DSE_NET_REPLICATION_REPL_TRANSFORM_CODEC_H

#include "engine/ecs/transform.h"
#include "engine/net/serialize/byte_stream.h"

namespace dse::net::repl {

/// 写入 TransformComponent 到 ByteWriter（10 × f32 = 40 bytes）。
inline void WriteTransform(ByteWriter& w, const TransformComponent& t) {
    w.WriteF32(t.position.x); w.WriteF32(t.position.y); w.WriteF32(t.position.z);
    w.WriteF32(t.rotation.x); w.WriteF32(t.rotation.y);
    w.WriteF32(t.rotation.z); w.WriteF32(t.rotation.w);
    w.WriteF32(t.scale.x);    w.WriteF32(t.scale.y);    w.WriteF32(t.scale.z);
}

/// 从 ByteReader 读取 TransformComponent（10 × f32）。
inline void ReadTransform(ByteReader& r, TransformComponent& t) {
    t.position.x = r.ReadF32(); t.position.y = r.ReadF32(); t.position.z = r.ReadF32();
    t.rotation.x = r.ReadF32(); t.rotation.y = r.ReadF32();
    t.rotation.z = r.ReadF32(); t.rotation.w = r.ReadF32();
    t.scale.x = r.ReadF32();    t.scale.y = r.ReadF32();    t.scale.z = r.ReadF32();
    t.dirty = true;
}

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPL_TRANSFORM_CODEC_H
