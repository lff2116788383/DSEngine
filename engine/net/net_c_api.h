#ifndef DSE_NET_C_API_H
#define DSE_NET_C_API_H
// [实验性 — v0.1 不含联机] 仅在 -DDSE_ENABLE_NET=ON 时编译；v0.1 不提供联机、
// 不属于稳定 public/SDK API，接口可能变动。
// DSEngine 网络层 — 扁平 C ABI（供 Lua / C# / FFI 等宿主语言调用）。
// 不暴露任何 C++/GNS 类型；句柄为不透明指针，事件经回调结构体派发。
// 实现见 net_c_api.cpp（仅依赖 engine/net 抽象，不直接 include GNS 头）。
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 不透明传输句柄。
typedef struct dse_net_transport dse_net_transport;

// 连接句柄（0 = 无效，对应 dse::net::kInvalidConnection）。
typedef uint32_t dse_net_conn;

// 发送模式（对应 dse::net::SendMode）。
typedef enum {
    DSE_NET_RELIABLE   = 0,
    DSE_NET_UNRELIABLE = 1
} dse_net_send_mode;

// 关闭原因（对应 dse::net::CloseReason）。
typedef enum {
    DSE_NET_CLOSE_NORMAL          = 0,
    DSE_NET_CLOSE_BY_PEER         = 1,
    DSE_NET_CLOSE_PROBLEM         = 2,
    DSE_NET_CLOSE_REJECTED        = 3
} dse_net_close_reason;

// 连接质量快照（对应 dse::net::ConnQuality）。
typedef struct {
    float ping_ms;
    float packet_loss;        // 0..1
    float out_bytes_per_sec;
    float in_bytes_per_sec;
    int   pending_reliable;
} dse_net_quality;

// 每帧 Poll 时派发的事件回调；任一字段可为 NULL（不关心则不设）。
// user 为 dse_net_poll 传入的透传指针。
typedef struct {
    void (*on_connecting)(void* user, dse_net_conn conn, const char* host, uint16_t port);
    void (*on_connected) (void* user, dse_net_conn conn);
    void (*on_closed)    (void* user, dse_net_conn conn, uint32_t reason);
    void (*on_message)   (void* user, dse_net_conn conn, const void* data, size_t len, uint16_t lane);
} dse_net_callbacks;

// ── 生命周期 ──
dse_net_transport* dse_net_create(void);                 // 创建后端实例（GNS）
void               dse_net_destroy(dse_net_transport* t);// 销毁（内部会先 Shutdown）
int  dse_net_init(dse_net_transport* t, int log_debug);  // 1=成功，0=失败
void dse_net_shutdown(dse_net_transport* t);

// ── 服务端 / 客户端 ──
int          dse_net_listen(dse_net_transport* t, uint16_t port);            // 1=成功
dse_net_conn dse_net_connect(dse_net_transport* t, const char* host, uint16_t port); // 0=失败
void         dse_net_close(dse_net_transport* t, dse_net_conn conn, uint32_t reason);

// ── 通道（lanes）──
// num_lanes 条 lane；priorities 长度须为 num_lanes；weights 可为 NULL（等权）。1=成功。
int dse_net_configure_lanes(dse_net_transport* t, dse_net_conn conn,
                            int num_lanes, const int* priorities, const uint16_t* weights);

// ── 收发 ──
// mode 取 dse_net_send_mode；lane 选择通道（0=默认）。1=成功。
int  dse_net_send(dse_net_transport* t, dse_net_conn conn,
                  const void* data, size_t len, uint32_t mode, uint16_t lane);
void dse_net_flush(dse_net_transport* t, dse_net_conn conn);

// ── 每帧泵 ──
void dse_net_poll(dse_net_transport* t, const dse_net_callbacks* cbs, void* user);

// ── 诊断 ──
int dse_net_get_quality(dse_net_transport* t, dse_net_conn conn, dse_net_quality* out); // 1=成功

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DSE_NET_C_API_H
