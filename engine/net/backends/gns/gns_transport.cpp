// DSEngine 网络层 — GameNetworkingSockets 后端实现。
// 本文件是整个引擎中**唯一** include <steam/...> 的位置。
//
// [A3 架构优化] 实例级回调路由：通过静态连接注册表将 GNS 进程级回调派发到
// 正确的 GnsTransport 实例，支持同进程多 transport 并存（多世界/并行测试）。
#include "engine/net/net_transport.h"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dse::net {
namespace {

CloseReason MapCloseState(int eState) {
    switch (eState) {
        case k_ESteamNetworkingConnectionState_ClosedByPeer:        return CloseReason::ClosedByPeer;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: return CloseReason::ProblemDetected;
        default:                                                    return CloseReason::Normal;
    }
}

Address MapAddr(const SteamNetworkingIPAddr& a) {
    char buf[64] = {0};
    a.ToString(buf, sizeof(buf), false);
    return Address{std::string(buf), a.m_port};
}

// ── Instance Registry ────────────────────────────────────────────────────────
// 替代旧的 s_active 全局指针。每个 GnsTransport 实例在 Init 时注册，Shutdown 时移除。
// 通过连接句柄或监听 socket 反查所属实例，支持多 transport 并存。

class GnsTransport;

struct GnsRegistry {
    std::mutex                                          mtx;
    std::unordered_map<HSteamListenSocket, GnsTransport*> listen_map;
    std::unordered_map<HSteamNetConnection, GnsTransport*> conn_map;
    int                                                 ref_count = 0;  // GNS init 引用计数
};

static GnsRegistry& Registry() {
    static GnsRegistry reg;
    return reg;
}

} // namespace

class GnsTransport final : public INetTransport {
public:
    bool Init(const NetConfig& cfg) override {
        auto& reg = Registry();
        std::lock_guard lock(reg.mtx);

        // 引用计数式 GNS 初始化（多实例共享同一底层库）
        if (reg.ref_count == 0) {
            SteamNetworkingErrMsg errMsg;
            if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
                return false;
            }
            SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(
                &GnsTransport::OnStatusChangedStatic);
        }
        ++reg.ref_count;

        m_iface = SteamNetworkingSockets();
        if (!m_iface) return false;
        m_pollGroup = m_iface->CreatePollGroup();
        (void)cfg;
        return true;
    }

    void Shutdown() override {
        if (!m_iface) return;

        auto& reg = Registry();
        std::lock_guard lock(reg.mtx);

        for (auto c : m_conns) {
            m_iface->CloseConnection(c, 0, "shutdown", false);
            reg.conn_map.erase(c);
        }
        m_conns.clear();

        if (m_listenSocket != k_HSteamListenSocket_Invalid) {
            reg.listen_map.erase(m_listenSocket);
            m_iface->CloseListenSocket(m_listenSocket);
            m_listenSocket = k_HSteamListenSocket_Invalid;
        }
        if (m_pollGroup != k_HSteamNetPollGroup_Invalid) {
            m_iface->DestroyPollGroup(m_pollGroup);
            m_pollGroup = k_HSteamNetPollGroup_Invalid;
        }

        --reg.ref_count;
        if (reg.ref_count <= 0) {
            reg.ref_count = 0;
            GameNetworkingSockets_Kill();
        }
        m_iface = nullptr;
    }

    bool Listen(uint16_t port) override {
        SteamNetworkingIPAddr addr;
        addr.Clear();
        addr.m_port = port;
        m_listenSocket = m_iface->CreateListenSocketIP(addr, 0, nullptr);
        if (m_listenSocket == k_HSteamListenSocket_Invalid) return false;

        auto& reg = Registry();
        std::lock_guard lock(reg.mtx);
        reg.listen_map[m_listenSocket] = this;
        return true;
    }

    ConnectionId Connect(const Address& a) override {
        SteamNetworkingIPAddr addr;
        addr.Clear();
        std::string s = a.host + ":" + std::to_string(a.port);
        if (!addr.ParseString(s.c_str())) return kInvalidConnection;
        HSteamNetConnection c = m_iface->ConnectByIPAddress(addr, 0, nullptr);
        if (c == k_HSteamNetConnection_Invalid) return kInvalidConnection;
        m_iface->SetConnectionPollGroup(c, m_pollGroup);

        auto& reg = Registry();
        std::lock_guard lock(reg.mtx);
        m_conns.insert(c);
        reg.conn_map[c] = this;
        return static_cast<ConnectionId>(c);
    }

    void Close(ConnectionId conn, CloseReason /*reason*/) override {
        auto c = static_cast<HSteamNetConnection>(conn);
        m_iface->CloseConnection(c, 0, "close", false);

        auto& reg = Registry();
        std::lock_guard lock(reg.mtx);
        m_conns.erase(c);
        reg.conn_map.erase(c);
    }

    bool ConfigureLanes(ConnectionId conn, const LaneConfig& cfg) override {
        const int n = static_cast<int>(cfg.priorities.size());
        if (n <= 0) return false;
        if (!cfg.weights.empty() && cfg.weights.size() != cfg.priorities.size()) return false;
        const int* pri = cfg.priorities.data();
        const uint16* w = cfg.weights.empty() ? nullptr : cfg.weights.data();
        return m_iface->ConfigureConnectionLanes(
                   static_cast<HSteamNetConnection>(conn), n, pri, w) == k_EResultOK;
    }

    bool Send(ConnectionId conn, const void* data, size_t len,
              SendMode mode, LaneId lane) override {
        const int flags = (mode == SendMode::Reliable) ? k_nSteamNetworkingSend_Reliable
                                                       : k_nSteamNetworkingSend_Unreliable;
        if (lane == kDefaultLane) {
            EResult r = m_iface->SendMessageToConnection(
                static_cast<HSteamNetConnection>(conn), data, static_cast<uint32>(len), flags, nullptr);
            return r == k_EResultOK;
        }
        SteamNetworkingMessage_t* msg = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(len));
        if (!msg) return false;
        std::memcpy(msg->m_pData, data, len);
        msg->m_conn    = static_cast<HSteamNetConnection>(conn);
        msg->m_nFlags  = flags;
        msg->m_idxLane = lane;
        int64 result = 0;
        m_iface->SendMessages(1, &msg, &result, /*bDeleteFailedMessages=*/true);
        return result > 0;
    }

    void Flush(ConnectionId conn) override {
        m_iface->FlushMessagesOnConnection(static_cast<HSteamNetConnection>(conn));
    }

    void Poll(INetListener& listener) override {
        m_listener = &listener;
        m_iface->RunCallbacks();

        for (;;) {
            ISteamNetworkingMessage* msgs[16];
            int n = m_iface->ReceiveMessagesOnPollGroup(m_pollGroup, msgs, 16);
            if (n <= 0) break;
            for (int i = 0; i < n; ++i) {
                MessageView v{msgs[i]->m_pData, static_cast<size_t>(msgs[i]->m_cbSize)};
                listener.OnMessage(static_cast<ConnectionId>(msgs[i]->m_conn), v,
                                   static_cast<LaneId>(msgs[i]->m_idxLane));
                msgs[i]->Release();
            }
            if (n < 16) break;
        }
        m_listener = nullptr;
    }

    bool GetQuality(ConnectionId conn, ConnQuality& out) override {
        SteamNetConnectionRealTimeStatus_t st;
        if (m_iface->GetConnectionRealTimeStatus(
                static_cast<HSteamNetConnection>(conn), &st, 0, nullptr) != k_EResultOK) {
            return false;
        }
        out.ping_ms = static_cast<float>(st.m_nPing);
        out.packet_loss = (st.m_flConnectionQualityLocal >= 0.0f)
                              ? (1.0f - st.m_flConnectionQualityLocal) : 0.0f;
        out.out_bytes_per_sec = st.m_flOutBytesPerSec;
        out.in_bytes_per_sec  = st.m_flInBytesPerSec;
        out.pending_reliable  = st.m_cbPendingReliable;
        return true;
    }

private:
    void HandleStatus(SteamNetConnectionStatusChangedCallback_t* info) {
        HSteamNetConnection c = info->m_hConn;
        switch (info->m_info.m_eState) {
            case k_ESteamNetworkingConnectionState_Connecting:
                if (info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid) {
                    m_iface->AcceptConnection(c);
                    m_iface->SetConnectionPollGroup(c, m_pollGroup);
                    auto& reg = Registry();
                    std::lock_guard lock(reg.mtx);
                    m_conns.insert(c);
                    reg.conn_map[c] = this;
                }
                if (m_listener) m_listener->OnConnecting(static_cast<ConnectionId>(c),
                                                         MapAddr(info->m_info.m_addrRemote));
                break;
            case k_ESteamNetworkingConnectionState_Connected:
                if (m_listener) m_listener->OnConnected(static_cast<ConnectionId>(c));
                break;
            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
                if (m_listener) m_listener->OnClosed(static_cast<ConnectionId>(c),
                                                     MapCloseState(info->m_info.m_eState));
                m_iface->CloseConnection(c, 0, nullptr, false);
                {
                    auto& reg = Registry();
                    std::lock_guard lock(reg.mtx);
                    m_conns.erase(c);
                    reg.conn_map.erase(c);
                }
                break;
            default:
                break;
        }
    }

    // 全局回调入口 — 通过注册表路由到正确的实例。
    static void OnStatusChangedStatic(SteamNetConnectionStatusChangedCallback_t* info) {
        auto& reg = Registry();
        GnsTransport* target = nullptr;
        {
            std::lock_guard lock(reg.mtx);
            // 优先通过连接句柄查找（outgoing + 已注册 incoming）
            auto it = reg.conn_map.find(info->m_hConn);
            if (it != reg.conn_map.end()) {
                target = it->second;
            } else if (info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid) {
                // 新入站连接：通过监听 socket 查找所属实例
                auto lit = reg.listen_map.find(info->m_info.m_hListenSocket);
                if (lit != reg.listen_map.end()) {
                    target = lit->second;
                }
            }
        }
        if (target) target->HandleStatus(info);
    }

    ISteamNetworkingSockets* m_iface = nullptr;
    HSteamListenSocket  m_listenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup  m_pollGroup = k_HSteamNetPollGroup_Invalid;
    std::unordered_set<HSteamNetConnection> m_conns;
    INetListener* m_listener = nullptr;
};

INetTransport* CreateGnsTransport() { return new GnsTransport(); }

} // namespace dse::net
