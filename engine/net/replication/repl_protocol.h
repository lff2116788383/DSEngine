/**
 * @file repl_protocol.h
 * @brief 复制层线协议常量与消息类型（完整版）。
 *
 * 见 docs/architecture/NETWORK_LAYER_DESIGN.md §4。
 * 包含：协议版本握手、spawn/despawn、全量/增量快照、RPC、ACK。
 */
#ifndef DSE_NET_REPLICATION_REPL_PROTOCOL_H
#define DSE_NET_REPLICATION_REPL_PROTOCOL_H

#include <cstdint>

namespace dse::net::repl {

/// 协议版本号（双端握手时交换，不匹配则拒绝连接）。
inline constexpr uint16_t kProtocolVersion = 2;

/// 网络身份：会话内自增 id。
using NetId = uint32_t;
inline constexpr NetId kInvalidNetId = 0;

/// 预制体/原型标识。
using ArchetypeId = uint32_t;
inline constexpr ArchetypeId kDefaultArchetype = 0;

/// RPC 标识。
using RpcId = uint16_t;
inline constexpr RpcId kInvalidRpcId = 0xFFFF;

/// RPC 目标类型。
enum class RpcTarget : uint8_t {
    Server    = 0,  ///< C→S：客户端请求服务器执行
    Client    = 1,  ///< S→C：服务器向指定客户端发送
    Multicast = 2,  ///< S→All：服务器向所有相关客户端广播
};

/// 消息首字节类型标签。
enum class MsgType : uint8_t {
    // ── 握手 ──
    Handshake       = 0,   ///< 可靠：u16 protocol_version
    HandshakeAck    = 1,   ///< 可靠：u16 protocol_version, u8 accepted(1/0)

    // ── 复制 ──
    Spawn           = 2,   ///< 可靠：u32 netId, u32 archetype
    Despawn         = 3,   ///< 可靠：u32 netId
    Snapshot        = 4,   ///< 不可靠：u32 seq, u16 count, [u32 netId, 10×f32 transform]
    DeltaSnapshot   = 5,   ///< 不可靠：u32 seq, u32 baseline_seq, u16 count, [u32 netId, delta_flags, changed_fields]
    SnapshotAck     = 6,   ///< 可靠(C→S)：u32 last_received_seq

    // ── 输入 ──
    InputMove       = 16,  ///< 可靠(C→S)：u32 netId, f32 dx, f32 dy, f32 dz

    // ── RPC ──
    RpcCall         = 32,  ///< 可靠/不可靠：u16 rpc_id, u32 target_netId, [payload...]
};

/// Delta 标志位：标记 TransformComponent 哪些字段发生变化。
enum DeltaFlags : uint8_t {
    kDeltaPosition = 0x01,
    kDeltaRotation = 0x02,
    kDeltaScale    = 0x04,
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPL_PROTOCOL_H
