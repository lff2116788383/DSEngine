#pragma once
// DSEngine 网络层 — 与后端无关的公共类型。
// 上层(gameplay/脚本)只依赖本文件与 net_transport.h，不直接 include 任何 GNS 头。
#include <cstdint>
#include <cstddef>
#include <string>

namespace dse::net {

// 连接句柄。0 表示无效连接。
using ConnectionId = uint32_t;
inline constexpr ConnectionId kInvalidConnection = 0;

// 发送模式。可靠/非可靠走同一连接。
enum class SendMode : uint32_t {
    Reliable   = 0,  // 有序可靠（自动分片重组）
    Unreliable = 1,  // 尽力而为（可丢、可乱序）
};

// 连接关闭/异常原因（简化分类）。
enum class CloseReason : uint32_t {
    Normal             = 0,  // 主动正常关闭
    ClosedByPeer       = 1,  // 对端关闭
    ProblemDetected    = 2,  // 本地探测到连接异常（超时/网络问题）
    Rejected           = 3,  // 连接被拒
};

// 网络地址（IPv4/IPv6 + 端口，文本形式如 "127.0.0.1:27015"）。
struct Address {
    std::string host;   // "127.0.0.1" / "::1" / 域名(由上层解析后传入数值地址)
    uint16_t    port = 0;
};

// 实时连接质量快照（来自后端的统计）。
struct ConnQuality {
    float ping_ms          = 0.0f;
    float packet_loss      = 0.0f;  // 0..1
    float out_bytes_per_sec = 0.0f;
    float in_bytes_per_sec  = 0.0f;
    int   pending_reliable  = 0;     // 待发可靠字节
};

// 收到的消息视图（数据在下一次 Poll 前有效；需保留请自行拷贝）。
struct MessageView {
    const void* data = nullptr;
    size_t      size = 0;
};

} // namespace dse::net
