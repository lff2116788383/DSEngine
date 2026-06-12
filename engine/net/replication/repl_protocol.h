/**
 * @file repl_protocol.h
 * @brief 复制层 MVP 线协议常量与消息类型。
 *
 * 见 docs/architecture/NETWORK_LAYER_DESIGN.md §4。MVP 只做：
 *   - 全量快照（不可靠）：每 tick 把所有被复制实体的 TransformComponent 发给客户端
 *   - spawn / despawn（可靠）：客户端按 NetId 建立 / 删除镜像实体
 *   - InputMove（可靠，C→S）：客户端请求移动，服务器权威应用
 * 未做（留后续 Phase）：增量(delta)、AOI、位级量化、客户端预测/插值、多 lane 调度。
 */
#ifndef DSE_NET_REPLICATION_REPL_PROTOCOL_H
#define DSE_NET_REPLICATION_REPL_PROTOCOL_H

#include <cstdint>

namespace dse::net::repl {

/// MVP 网络身份：会话内自增 id（v2 设计的紧凑句柄 {index,generation} 的简化版，
/// generation 留待后续阶段；见 §4.2 / §10 D-6）。
using NetId = uint32_t;
inline constexpr NetId kInvalidNetId = 0;

/// 预制体/原型标识。MVP 只复制 TransformComponent，archetype 暂统一为 0。
using ArchetypeId = uint32_t;
inline constexpr ArchetypeId kDefaultArchetype = 0;

/// 消息首字节类型标签。
enum class MsgType : uint8_t {
    Spawn     = 1,   ///< 可靠：u32 netId, u32 archetype
    Despawn   = 2,   ///< 可靠：u32 netId
    Snapshot  = 3,   ///< 不可靠：u16 count, 然后每个实体 [u32 netId, 10×f32 transform]
    InputMove = 16,  ///< 可靠(C→S)：u32 netId, f32 dx, f32 dy, f32 dz
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPL_PROTOCOL_H
