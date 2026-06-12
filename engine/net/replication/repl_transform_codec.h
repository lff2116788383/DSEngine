/**
 * @file repl_transform_codec.h
 * @brief TransformComponent 的 MVP 网络编解码（position + rotation + scale，10×f32）。
 *
 * MVP 不做量化压缩，直接写 10 个 float（见 §4.10）。服务器与客户端共用此编解码，
 * 保证两端字段顺序一致；后续可在此替换为位级量化而不改协议外形。
 */
#ifndef DSE_NET_REPLICATION_REPL_TRANSFORM_CODEC_H
#define DSE_NET_REPLICATION_REPL_TRANSFORM_CODEC_H

#include "engine/ecs/transform.h"
#include "engine/net/serialize/byte_stream.h"

namespace dse::net::repl {

inline void WriteTransform(ByteWriter& w, const TransformComponent& t) {
    w.WriteF32(t.position.x); w.WriteF32(t.position.y); w.WriteF32(t.position.z);
    w.WriteF32(t.rotation.x); w.WriteF32(t.rotation.y);
    w.WriteF32(t.rotation.z); w.WriteF32(t.rotation.w);
    w.WriteF32(t.scale.x);    w.WriteF32(t.scale.y);    w.WriteF32(t.scale.z);
}

inline void ReadTransform(ByteReader& r, TransformComponent& t) {
    t.position.x = r.ReadF32(); t.position.y = r.ReadF32(); t.position.z = r.ReadF32();
    t.rotation.x = r.ReadF32(); t.rotation.y = r.ReadF32();
    t.rotation.z = r.ReadF32(); t.rotation.w = r.ReadF32();
    t.scale.x    = r.ReadF32(); t.scale.y    = r.ReadF32(); t.scale.z    = r.ReadF32();
    t.dirty = true; // 镜像端需重算 local_to_world
}

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPL_TRANSFORM_CODEC_H
