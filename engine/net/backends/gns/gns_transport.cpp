// DSEngine 网络层 — GameNetworkingSockets 后端实现。
// 本文件是整个引擎中**唯一** include <steam/...> 的位置。
#include "engine/net/net_transport.h"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <cstring>
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

// GNS 的 SteamNetworkingIPAddr → 与后端无关的 Address。
Address MapAddr(const SteamNetworkingIPAddr& a) {
    char buf[64] = {0};
    a.ToString(buf, sizeof(buf), false);  // 仅主机，不拼端口
    return Address{std::string(buf), a.m_port};
}

} // namespace

// 单实例后端：GNS 的连接状态回调是进程级的，这里用 s_active 路由到当前实例。
class GnsTransport final : public INetTransport {
public:
    bool Init(const NetConfig& cfg) override {
        SteamNetworkingErrMsg errMsg;
        if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
            return false;
        }
        m_iface = SteamNetworkingSockets();
        if (!m_iface) return false;
        m_pollGroup = m_iface->CreatePollGroup();
        s_active = this;
        SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(&GnsTransport::OnStatusChangedStatic);
        (void)cfg;
        return true;
    }

    void Shutdown() override {
        if (!m_iface) return;
        for (auto c : m_conns) m_iface->CloseConnection(c, 0, "shutdown", false);
        m_conns.clear();
        if (m_listenSocket != k_HSteamListenSocket_Invalid) {
            m_iface->CloseListenSocket(m_listenSocket);
            m_listenSocket = k_HSteamListenSocket_Invalid;
        }
        if (m_pollGroup != k_HSteamNetPollGroup_Invalid) {
            m_iface->DestroyPollGroup(m_pollGroup);
            m_pollGroup = k_HSteamNetPollGroup_Invalid;
        }
        s_active = nullptr;
        m_iface = nullptr;
        GameNetworkingSockets_Kill();
    }

    bool Listen(uint16_t port) override {
        SteamNetworkingIPAddr addr;
        addr.Clear();
        addr.m_port = port;  // 监听所有接口的该端口
        m_listenSocket = m_iface->CreateListenSocketIP(addr, 0, nullptr);
        return m_listenSocket != k_HSteamListenSocket_Invalid;
    }

    ConnectionId Connect(const Address& a) override {
        SteamNetworkingIPAddr addr;
        addr.Clear();
        std::string s = a.host + ":" + std::to_string(a.port);
        if (!addr.ParseString(s.c_str())) return kInvalidConnection;
        HSteamNetConnection c = m_iface->ConnectByIPAddress(addr, 0, nullptr);
        if (c == k_HSteamNetConnection_Invalid) return kInvalidConnection;
        m_iface->SetConnectionPollGroup(c, m_pollGroup);
        m_conns.insert(c);
        return static_cast<ConnectionId>(c);
    }

    void Close(ConnectionId conn, CloseReason /*reason*/) override {
        auto c = static_cast<HSteamNetConnection>(conn);
        m_iface->CloseConnection(c, 0, "close", false);
        m_conns.erase(c);
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
        // 非 0 通道：SendMessageToConnection 不接受 lane，需走 AllocateMessage + SendMessages。
        SteamNetworkingMessage_t* msg = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(len));
        if (!msg) return false;
        std::memcpy(msg->m_pData, data, len);
        msg->m_conn    = static_cast<HSteamNetConnection>(conn);
        msg->m_nFlags  = flags;
        msg->m_idxLane = lane;
        int64 result = 0;
        m_iface->SendMessages(1, &msg, &result, /*bDeleteFailedMessages=*/true);
        return result > 0;  // 正数=消息编号（成功）；负数=EResult 错误
    }

    void Flush(ConnectionId conn) override {
        m_iface->FlushMessagesOnConnection(static_cast<HSteamNetConnection>(conn));
    }

    void Poll(INetListener& listener) override {
        m_listener = &listener;
        m_iface->RunCallbacks();  // 触发连接状态回调（经 s_active 路由）

        // 收消息（服务端与客户端连接统一加入同一 poll group）
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
                // 服务端：来自监听 socket 的入站连接 → 接受并入 poll group
                if (info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid) {
                    m_iface->AcceptConnection(c);
                    m_iface->SetConnectionPollGroup(c, m_pollGroup);
                    m_conns.insert(c);
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
                m_conns.erase(c);
                break;
            default:
                break;
        }
    }

    static void OnStatusChangedStatic(SteamNetConnectionStatusChangedCallback_t* info) {
        if (s_active) s_active->HandleStatus(info);
    }

    ISteamNetworkingSockets* m_iface = nullptr;
    HSteamListenSocket  m_listenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup  m_pollGroup = k_HSteamNetPollGroup_Invalid;
    std::unordered_set<HSteamNetConnection> m_conns;
    INetListener* m_listener = nullptr;  // 仅在 Poll() 期间有效

    static GnsTransport* s_active;
};

GnsTransport* GnsTransport::s_active = nullptr;

INetTransport* CreateGnsTransport() { return new GnsTransport(); }

} // namespace dse::net
