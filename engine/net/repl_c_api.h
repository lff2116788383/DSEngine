/**
 * @file repl_c_api.h
 * @brief 复制层扁平 C ABI — 供 Lua/C# 脚本调用复制与 RPC 功能。
 *
 * 依赖：DSE_NET_ENABLED 构建标志。与 net_c_api.h（传输层）平级。
 *
 * 命名约定：dse_repl_* (replication)、dse_rpc_* (remote procedure call)
 */
#ifndef DSE_NET_REPL_C_API_H
#define DSE_NET_REPL_C_API_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ─── 不透明句柄 ──────────────────────────────────────────────────────────

typedef struct dse_repl_server_s* dse_repl_server;
typedef struct dse_repl_client_s* dse_repl_client;

// ─── 服务器 API ──────────────────────────────────────────────────────────

/// 创建复制服务器实例。
dse_repl_server dse_repl_server_create(void);

/// 销毁。
void dse_repl_server_destroy(dse_repl_server srv);

/// 初始化（绑定已有 transport 句柄和 ECS registry 指针）。
/// transport_ptr: dse_net_transport*; registry_ptr: entt::registry*
int dse_repl_server_init(dse_repl_server srv, void* transport_ptr, void* registry_ptr);

/// 将实体纳入复制。返回分配的 NetId（0=失败）。
uint32_t dse_repl_server_mark(dse_repl_server srv, uint32_t entity, uint32_t owner_conn);

/// 设置实体属主。
void dse_repl_server_set_owner(dse_repl_server srv, uint32_t entity, uint32_t owner_conn);

/// 移出复制。
void dse_repl_server_unreplicate(dse_repl_server srv, uint32_t entity);

/// 每帧 Tick（发送快照/delta + AOI 处理）。
void dse_repl_server_tick(dse_repl_server srv);

/// 设置 AOI 策略。policy: 0=Always, 1=Distance
void dse_repl_server_set_aoi(dse_repl_server srv, int policy, float radius);

/// 获取已连接客户端数。
uint32_t dse_repl_server_client_count(dse_repl_server srv);

/// 获取当前快照序号。
uint32_t dse_repl_server_seq(dse_repl_server srv);

// ─── 客户端 API ──────────────────────────────────────────────────────────

/// 创建复制客户端实例。
dse_repl_client dse_repl_client_create(void);

/// 销毁。
void dse_repl_client_destroy(dse_repl_client cli);

/// 初始化。
int dse_repl_client_init(dse_repl_client cli, void* transport_ptr, void* registry_ptr);

/// 发送移动输入。
void dse_repl_client_send_move(dse_repl_client cli, uint32_t net_id, float dx, float dy, float dz);

/// 是否已完成握手。
int dse_repl_client_connected(dse_repl_client cli);

/// 镜像实体数量。
uint32_t dse_repl_client_mirror_count(dse_repl_client cli);

/// NetId → 本地实体 ID（0xFFFFFFFF=未知）。
uint32_t dse_repl_client_to_entity(dse_repl_client cli, uint32_t net_id);

// ─── RPC API ─────────────────────────────────────────────────────────────

/// RPC handler 回调类型（C ABI）。
/// sender: 发送方连接; target: 目标 NetId; payload/len: 参数数据。
/// 返回 1=处理成功, 0=失败。
typedef int (*dse_rpc_handler_fn)(uint32_t sender, uint32_t target,
                                  const void* payload, size_t len, void* userdata);

/// RPC 校验回调（仅 Server RPC）。返回 1=允许, 0=拒绝。
typedef int (*dse_rpc_validator_fn)(uint32_t sender, uint32_t target, void* userdata);

/// 在服务器端注册 RPC。target: 0=Server, 1=Client, 2=Multicast。
/// 返回 rpc_id（0xFFFF=失败）。
uint16_t dse_rpc_server_register(dse_repl_server srv, const char* name, int target,
                                  dse_rpc_handler_fn handler, dse_rpc_validator_fn validator,
                                  void* userdata);

/// 在客户端注册 RPC handler（接收 Server→Client RPC）。
uint16_t dse_rpc_client_register(dse_repl_client cli, const char* name, int target,
                                  dse_rpc_handler_fn handler, void* userdata);

/// 客户端发送 RPC 到服务器。
int dse_rpc_client_send(dse_repl_client cli, uint16_t rpc_id, uint32_t target_net_id,
                         const void* payload, size_t payload_size);

/// 服务器广播 RPC 到所有客户端。
void dse_rpc_server_broadcast(dse_repl_server srv, uint16_t rpc_id, uint32_t target_net_id,
                               const void* payload, size_t payload_size);

#ifdef __cplusplus
}
#endif

#endif // DSE_NET_REPL_C_API_H
