# DSEngine 网络层设计方案（v2）

> 目标分支：`feature/engine-lib`
> 状态：**设计待评审 + MVP 已落地**（传输底座已实现并三端实测；复制/同步层的**最小可用闭环**已实现，见 §8「MVP 已实现」）
> v2 相对 v1 的变化（吃进评审意见）：① `NetId` 由"直接用 64-bit UUID"改为**会话内紧凑稠密 id**（复用 Phase 7 `Handle{index,generation}`），UUID 仅用于 spawn 关联/持久化；② 新增 §4.12「协议版本/迟加入/重连」；③ §4.3/§4.8/§4.9 写清范围限制；④ §10 重构为「技术债与范围限制（诚实登记 D-1…D-6）」。
> 前置文档：
> - `docs/roadmap/NETWORK_GNS_INTEGRATION_PLAN.md` —— 传输底座选型与构建方案（GNS）
> - `docs/roadmap/NETWORK_GNS_PROGRESS.md` —— 传输底座落地进度（Phase 0–6，已实测）
> - `docs/roadmap/HTTP_LUA_MODULE.md` / `docs/AI_CHAT_INTEGRATION_PLAN.md` —— HTTP 客户端
>
> 本文档定位：把网络层**作为一个整体**讲清楚——下层「传输底座」现状（已实现，简述）+ 上层「复制/同步层」设计（未实现，重点）+ 分阶段路线图。体例对齐 `MEMORY_MANAGEMENT_DESIGN.md`。

---

## 0. TL;DR

DSEngine 现在有一套**已实测、可用的传输底座**（`engine/net`，GameNetworkingSockets 后端：可靠/不可靠 + 多通道 lanes + 内置加密 + 连接质量 + 扁平 C ABI + Lua 绑定，默认 `DSE_ENABLE_NET=OFF`，三端构建零影响），外加一个**异步 HTTP(S) 客户端**（`engine/http`）。

但**缺少把"游戏世界状态"自动同步的高层逻辑**——也就是主流引擎里的"复制/同步层"（Unreal Replication、Unity Netcode、Godot High-level Multiplayer 那一层）。本方案在现有传输之上设计这一层：

- **权威模型**：默认 **server-authoritative**（服务器权威），支持 listen-server（主机兼客户端）与 dedicated server。
- **网络身份（两级）**：上线引用用**会话内紧凑稠密 `NetId`**（复用 Phase 7 `Handle{index,generation}`，每包都带、必须小），持久 `UUID`（`UUIDComponent`）仅用于 spawn 关联与存档。
- **可复制组件**：用 `ReplicationTraits<T>` 标注哪些组件/字段需要同步，配套**紧凑二进制编解码**（区别于现有 rapidjson 存档路径——存档要可读，复制要小而快）。
- **快照 + 增量（delta）**：每连接维护 baseline，按 ack 发增量；不可靠道发状态快照、可靠道发关键事件。
- **兴趣管理（AOI）**：按距离/网格做相关性裁剪，每连接只发其关心的实体。
- **RPC**：server↔client、multicast，可靠/不可靠，反射注册。
- **客户端预测 + 服务器校正 + 插值**（高级，后期）：本地输入预测、服务器回滚校正、远端实体插值缓冲。

原则同内存子系统：**默认 OFF、零回归**；高层逻辑建在 `engine/net/replication/`，依赖 ECS（EnTT）与一套网络专用二进制序列化，**不污染现有公共接口**。

---

## 1. 现状与问题（实现依据）

| 维度 | 现状（代码实证） | 问题 / 缺口 |
|------|------------------|------|
| 传输底座 | `engine/net`：`INetTransport`/`INetListener` 抽象 + GNS 后端（`backends/gns/gns_transport.cpp`，全引擎唯一 include `<steam/...>`） | ✅ 已实现且三端实测；但只是"管道" |
| 收发能力 | 可靠/不可靠、多通道 lanes（优先级/防队头阻塞）、连接质量、内置加密 | ✅ 完整 |
| 宿主接入 | 扁平 C ABI `net_c_api.{h,cpp}`（不泄漏 C++/GNS 类型）+ Lua 绑定 `dse.net` | ✅ 完整 |
| HTTP | `engine/http`：异步 HTTP(S) 客户端（IXWebSocket + OpenSSL），`Poll()` 回调在调用线程 | ✅ 完整，用于调 REST/LLM |
| 构建 | `DSE_ENABLE_NET` 默认 OFF；`engine/net/**`、`engine/http/**` 从主目标 FILTER 掉，仅 ON 时编为独立 `dse_net`/`dse_http` | ✅ 零回归 |
| 测试 | `tests/net/{net_smoke,net_capi_smoke,net_lua_smoke}` 回环冒烟（reliable+unreliable+lane）；Linux/Win 运行、Android 编链 | ✅ 已验 |
| **高层复制/同步** | **无** | ❌ 没有实体复制、状态同步、RPC、预测/插值、AOI |
| **网络序列化** | 只有 rapidjson（场景存档 `scene_component_serialization.*`，JSON，可读但体积大） | ❌ 缺紧凑二进制 + 量化 + 位打包，per-frame 复制不能用 JSON |
| 传输实例数 | GNS 状态回调是进程级的，`GnsTransport` 用全局 `s_active` 路由 | ⚠️ 同进程同时只能一个 transport 实例 |
| P2P/NAT | `ENABLE_ICE=OFF`、`USE_STEAMWEBRTC=OFF`，仅直连 | ⚠️ 需公网 IP / 端口转发 / 专用服务器 |

**一句话**：传输地基扎实，**缺的是"地基之上的房子"——把 ECS 世界状态在多端之间自动同步的复制层**。这也是 `NETWORK_GNS_INTEGRATION_PLAN.md §0` 早已点明的事实："传输库只负责把字节送达；预测/复制/AOI 要我们在 `engine/net` 之上自己实现"。

---

## 2. 总体架构

```
                         ┌─────────────────────────────────────────────┐
  gameplay / 脚本 / 编辑器 │  ReplicationServer / ReplicationClient (门面) │   ← 本方案新增
                         │  RPC 注册/派发 · NetWorld(entity↔NetId)        │
                         └───────────────┬─────────────────────────────┘
                                         │ 依赖
              ┌──────────────────────────┼───────────────────────────┐
              ▼                          ▼                           ▼
     ┌───────────────┐         ┌──────────────────┐        ┌──────────────────┐
     │ ECS (EnTT)    │         │ NetSerialization │        │ 传输底座 engine/net │ ← 已实现
     │ World/registry│         │ Bit/ByteWriter   │        │ INetTransport     │
     │ UUIDComponent │         │ ReplicationTraits│        │ (GNS 后端)        │
     └───────────────┘         └──────────────────┘        └──────────────────┘
                                         │
                                  走 Memory 门面（MemoryTag::Net）+ 对象池
```

**分层职责**

| 层 | 职责 | 状态 |
|---|---|---|
| 传输底座 `engine/net` | 字节级可靠/不可靠收发、加密、lanes、质量 | ✅ 已实现 |
| 网络序列化 `engine/net/serialize` | 紧凑二进制读写、量化、位打包、组件 encode/decode | ❌ 新增 |
| 复制/同步 `engine/net/replication` | 实体身份、快照/增量、AOI、ack、应用到 ECS | ❌ 新增 |
| RPC `engine/net/rpc` | 远程过程调用注册/序列化/派发 | ❌ 新增 |
| 预测/插值（客户端） | 输入预测、服务器校正、远端插值 | ❌ 新增（后期） |
| 宿主暴露 | C ABI + Lua（复用现有 `net_c_api` 模式扩展） | 部分（仅传输） |

**关键设计取舍**

- **建在传输抽象之上、不碰 GNS**：复制层只依赖 `INetTransport`/`INetListener`，与底座解耦；未来换后端/开 P2P 不影响复制层。
- **server-authoritative 优先**：默认服务器权威（防作弊、状态一致性好）；P2P/host-migration 留作后续。
- **复制序列化独立于存档序列化**：存档走 rapidjson（可读、向后兼容）；复制走紧凑二进制（小、快、可量化）。两者可共享"哪些字段"的元信息，但**编码路径分开**。
- **默认 OFF、零回归**：与 `DSE_ENABLE_NET` 同一开关；OFF 时这些模块完全不编译。
- **走统一内存门面**：所有网络分配计入 `MemoryTag::Net`；消息/快照缓冲用对象池消除每消息堆分配（见 §4.10）。

---

## 3. 传输层现状（已实现，简述）

> 详见 `NETWORK_GNS_PROGRESS.md`。这里只给"复制层依赖到的接口面"。

`engine/net/net_transport.h` 抽象（稳定，复制层只用它）：

```cpp
class INetTransport {
    bool Init(const NetConfig&); void Shutdown();
    bool Listen(uint16_t port);                       // 服务端
    ConnectionId Connect(const Address&);             // 客户端
    void Close(ConnectionId, CloseReason);
    bool ConfigureLanes(ConnectionId, const LaneConfig&);   // 多通道
    bool Send(ConnectionId, const void* data, size_t len, SendMode, LaneId);
    void Flush(ConnectionId);
    void Poll(INetListener&);                          // 每帧泵：派发事件 + 收消息
    bool GetQuality(ConnectionId, ConnQuality&);
};
INetTransport* CreateGnsTransport();
```

**复制层用到的能力映射**

| 复制层需求 | 传输底座提供 |
|---|---|
| 关键事件（spawn/despawn/RPC）必达 | `SendMode::Reliable` |
| 高频状态快照可丢 | `SendMode::Unreliable` |
| 状态流不堵事件流 | 不同 `LaneId`（`ConfigureLanes` 设优先级） |
| 带宽自适应 / 拥塞 | GNS 内部拥塞控制 + `GetQuality` 反馈 |
| 连接建立/断开 | `INetListener::OnConnecting/OnConnected/OnClosed` |
| 收数据 | `INetListener::OnMessage(conn, view, lane)` |

**已知约束**（复制层设计需绕开）：① 同进程单 transport 实例（listen-server 场景：一个进程既 Listen 又对外 Connect 是可以的，但不能开两个独立 transport）；② 仅直连，无 P2P/NAT 穿透。

---

## 4. 高层复制/同步层设计（本方案核心）

命名空间 `dse::net::repl`，文件位于 `engine/net/replication/`、`engine/net/serialize/`、`engine/net/rpc/`。

### 4.1 权威模型

| 模型 | 说明 | 本期 |
|---|---|---|
| **Dedicated server** | 独立服务器进程持有权威世界，客户端只渲染+发输入 | ✅ 目标 |
| **Listen server** | 主机进程既是服务器又是本地玩家 | ✅ 目标 |
| Pure client | 连到权威服务器，只接收复制 + 发 RPC/输入 | ✅ 目标 |
| P2P / host-migration | 无中心权威 / 主机迁移 | ❌ 后续 |

**权威规则**：服务器是唯一真相源。客户端对"被复制实体"的修改不直接生效，只能通过**输入/RPC**请求服务器改；服务器校验后改权威状态，再复制回所有相关客户端。这是防作弊与一致性的基础。

### 4.2 网络身份：紧凑会话 `NetId` + 持久 `UUID`

**两级身份，分清"上线传输"与"持久标识"**：

| 身份 | 类型 | 用途 | 上线开销 |
|---|---|---|---|
| `NetId`（会话内） | 紧凑稠密句柄 `{index, generation}` | 每帧快照/增量/RPC 引用实体 | ✅ 每包都带，必须小 |
| `UUID`（持久） | `UUIDComponent` 的 `uint64_t` | 存档、跨会话、回放、预制体关联 | 仅 spawn 时带一次（或只持久化用） |

理由：随机 64-bit UUID 每个实体每包都带，带宽与索引都吃亏。线上引用用**会话内分配的稠密小 id**——直接复用内存子系统 Phase 7 的 `Handle{index, generation}`：`index` 做稠密数组下标（O(1) 查找）、`generation` 防悬垂/复用错配。64-bit UUID 仅用于 spawn 关联与持久化。

```cpp
// NetId = 会话内紧凑句柄（复用 engine/core/memory/handle.h 的 Handle 设计）
struct NetId { uint32_t index; uint32_t generation; };   // 上线时按实际位宽压缩
inline constexpr NetId kInvalidNetId{0xFFFFFFFFu, 0};

class NetWorld {
    Entity ToEntity(NetId) const;                  // 稠密表 O(1) + generation 校验
    NetId  ToNetId(Entity) const;
    Entity SpawnReplicated(NetId, ArchetypeId);    // 携带 (NetId, ArchetypeId, [UUID])
    void   DespawnReplicated(NetId);               // 回收 index、bump generation
};
```

- **服务器**：实体进入某连接相关集时分配会话 `NetId`（稠密表，离开/销毁回收 index 并 bump generation）；持久实体另带 `UUIDComponent`。
- **客户端**：按 `NetId.index` 维护稠密镜像表；`generation` 不匹配即丢弃过期引用。
- **上线压缩**：`NetId` 只发实际位宽（如 index 16~20 bit、generation 4~8 bit），由 `NetBitWriter` 位打包。
- `ArchetypeId`：小整数，标识"哪类实体/预制体"，让客户端知道建哪些组件（两端共享的 archetype 注册表）。

### 4.3 可复制组件与字段标注 `ReplicationTraits<T>`

不是所有组件都复制（Transform/血量要，渲染缓存不要）。用 **traits 显式标注**（编译期、零侵入 EnTT）：

```cpp
template <class C> struct ReplicationTraits {
    static constexpr bool kReplicated = false;       // 默认不复制
};

// 为某组件特化即可纳入复制（示例）
template <> struct ReplicationTraits<Transform> {
    static constexpr bool kReplicated = true;
    static constexpr LaneId   kLane   = 1;           // 走状态道
    static constexpr SendMode kMode   = SendMode::Unreliable;  // 高频、可丢
    static void Write(NetBitWriter& w, const Transform& t);    // 紧凑/量化编码
    static void Read (NetBitReader& r, Transform& t);
    static bool Changed(const Transform& a, const Transform& b);  // 增量判定
};
```

- `Write/Read`：手写紧凑编码（位置量化到定点、四元数 smallest-three 压缩等），**不用 JSON**。
- `Changed`：用于增量——只在值变化时进 delta。
- 一张"已复制组件表"在编译期收集（`ReplicatedComponentList = TypeList<...>`），驱动遍历。
- **与存档的关系**：存档元信息（哪些字段）可与复制共享一份描述，但 `scene_component_serialization`（rapidjson）继续负责磁盘存档，复制走 `NetBitWriter`。
  > ⚠️ 两条序列化路径意味着"加字段两边都要改"的 **schema 漂移风险**，已登记为技术债（见 §10 D-1）；理想终态是统一反射/schema 单一真相源，同时生成存档与复制两套编解码。

### 4.4 快照 + 增量（delta compression）

每个连接维护"客户端已确认的世界视图（baseline）"，服务器只发**变化部分**：

```
服务器每 tick：
  for conn in connections:
     relevant = AOI(conn)                       // §4.5 兴趣裁剪
     for e in relevant:
        for C in ReplicatedComponents(e):
           if Changed(C_now, baseline[conn][e].C):
              写入 delta(e, C)                   // 量化编码
     发 delta 包（不可靠道，带 tick 号）
     可靠道发 spawn/despawn/事件

客户端：
  收 delta → 应用到本地镜像 entity → 回 ack(tick)
服务器：
  收 ack(tick) → 把该 tick 的快照设为该连接新 baseline（环形缓冲保存近 N 个 tick）
```

- **不可靠 + ack 基线**：状态走不可靠道（省带宽、低延迟），靠"ack 推进 baseline"保证最终一致；丢一个状态包不致命（下个包是相对新基线的增量，但需保证基线只在 ack 后推进 → 即"按已确认基线做 delta"）。
- **spawn/despawn/重要事件**：走可靠道，必达。
- **环形 baseline 缓冲**：每连接保存最近 N 个 tick 的快照，按 ack 选基线（经典 Quake3/Source 模型）。
- **优先级 / 带宽预算**：每连接每 tick 有字节预算（结合 `GetQuality`），按组件优先级（玩家 > 远处道具）裁剪，未发的下 tick 再发。

### 4.5 兴趣管理 / 相关性（AOI）

不能把整个世界发给每个客户端。每连接只发其"关心"的实体：

| 策略 | 说明 | 本期 |
|---|---|---|
| Always relevant | 玩家自身、全局实体始终发 | ✅ |
| 距离裁剪 | 与该连接控制实体距离 < R 才发 | ✅ |
| 网格/格子 (grid) | 世界分格，只发同格+邻格（大世界，配合 `LARGE_WORLD_COORDINATES`） | ⏳ 进阶 |
| 视锥 / 遮挡 | 更精细，成本高 | ❌ 后续 |

接口：`bool IsRelevant(ConnectionId, Entity)` + 进入/离开相关集时触发 spawn/despawn。

### 4.6 通道（lanes）映射

利用传输底座的多通道避免队头阻塞：

| Lane | 内容 | 模式 | 优先级 |
|---|---|---|---|
| 0 | 控制/握手 | Reliable | 最高 |
| 1 | 状态快照/增量 | Unreliable | 中 |
| 2 | spawn/despawn/可靠事件 | Reliable | 高 |
| 3 | RPC | Reliable/Unreliable | 高 |

连接建立后用 `ConfigureLanes` 设定优先级与权重。

### 4.7 RPC（远程过程调用）

```cpp
enum class RpcTarget { Server, Client, Multicast };   // C→S, S→Client, S→所有相关
// 注册（反射/宏生成 id 与编解码）
DSE_RPC(RpcTarget::Server, void RequestFire(NetId weapon, Vec3 dir));
// 调用方写：Rpc().RequestFire(conn, weapon, dir);  → 序列化 + 经对应 lane 发送
// 接收方：按 rpc-id 派发到已注册处理函数；Server RPC 必须校验调用者权限
```

- **Server RPC（C→S）**：客户端请求服务器做事（开火、交互）；服务器**必须校验**（是否拥有该实体、冷却、合法性）。
- **Client/Multicast RPC（S→C）**：服务器通知客户端播特效/音效/UI（非权威表现层）。
- 可靠性：默认 Reliable；高频表现类可选 Unreliable。
- 序列化：参数走 `NetBitWriter`；rpc-id + 参数布局由宏/注册表生成，保证两端一致。

### 4.8 客户端预测 + 服务器校正 + 插值（高级，后期）

降低手感延迟，标注为**进阶阶段**：

- **输入预测**：本地立即按输入模拟自己的角色（不等服务器回包），同时把"带序号的输入"发服务器。
- **服务器校正（reconciliation）**：服务器回权威状态 + 已处理到的输入序号；客户端用它纠正本地，并**重放**之后未确认的输入。
- **远端实体插值**：对别人控制的实体，缓冲若干状态包，在过去 ~100ms 做插值（平滑），必要时外推。
- 依赖：固定 tick、确定性模拟（至少对预测的子系统）、输入序号。复杂度高，单独阶段。
  > ⚠️ **范围限制**：引擎物理（Jolt/PhysX）非确定性，物理驱动实体无法可靠预测/回滚；预测实际**只覆盖简单运动学角色**，物理实体一律服务器权威 + 远端插值。见 §10 D-2。

### 4.9 时间 / tick 同步

- **固定 tick**（如 server 30/60Hz）驱动复制；与渲染帧解耦。
- **时钟同步**：握手时估算 RTT，客户端维护一个相对服务器的时钟，用于插值与预测对齐。
- ⚠️ **前置依赖**：server-authoritative 需要与渲染解耦的**固定步长模拟 tick**；若当前引擎主循环没有独立 tick，需先改造主循环。见 §10 D-3。

### 4.10 内存与序列化基础设施

- 所有网络分配走 `dse::core::Memory`，标签 `MemoryTag::Net`，纳入预算/泄漏视图。
- **消除每消息堆分配**：当前 GNS 后端 `Send` 非 0 lane 走 `AllocateMessage`+`memcpy`；复制层用**对象池 + 复用缓冲**打包，减少抖动（呼应内存子系统 Phase 4 池）。
- `NetBitWriter`/`NetBitReader`：位级写读（bool 1 bit、量化整数 N bit、定点位置、四元数压缩），是 delta 压缩省带宽的关键，区别于字节对齐的 rapidjson。

### 4.11 脚本 / C ABI 暴露

复用现有 `net_c_api` 模式，向上加复制/RPC 接口：

- C ABI：`dse_repl_*`（spawn/despawn 回调、注册可复制组件、注册 RPC、tick）。
- Lua：`dse.repl`（注册 RPC handler、标注复制、读写复制组件），让玩法脚本无需碰 C++ 即可联机。

### 4.12 协议版本、迟加入与重连

- **协议版本握手**：连接建立后首条可靠消息交换 `protocol_version`（构建/网络协议号）；不匹配直接 `Close(Rejected)`，避免新旧客户端串扰。
- **迟加入 (late-join)**：晚连客户端不需要"历史"——AOI 天然处理：它进入相关集时按当前权威状态收到 spawn + 全量初始快照（baseline 从空开始增量收敛）。
- **重连**：断开按掉线处理，重连视为新会话（重新分配 `NetId`、重发 baseline）。会话恢复（保留 NetId 映射做断线续连）留作后续（见 §10 D-6）。

---

## 5. 模块与文件布局（建议）

```
engine/net/
  net_types.h / net_transport.h / net_c_api.*          # 已有：传输底座
  backends/gns/gns_transport.cpp                        # 已有：GNS 后端
  serialize/
    byte_stream.h         # ✅ MVP：字节级 ByteWriter/ByteReader（小端 POD）
    net_bitwriter.h / net_quantize.h   # 后续：位级读写 + 量化（替换 byte_stream 而不改协议外形）
  replication/
    repl_protocol.h       # ✅ MVP：NetId / MsgType 线协议常量
    repl_transform_codec.h# ✅ MVP：TransformComponent ↔ 字节 编解码（两端共用）
    replication_server.h/.cpp # ✅ MVP：spawn/despawn/全量快照/输入RPC + 服务器权威
    replication_client.h/.cpp # ✅ MVP：应用 spawn/despawn/快照 + 发输入
    repl_traits.h         # 后续：ReplicationTraits<T> + 已复制组件表（多组件）
    archetype.h           # 后续：ArchetypeId 注册表（两端共享）
  rpc/
    rpc_registry.h/.cpp   # 后续：通用 RPC id + 编解码 + 派发
tests/net/
  repl_smoke.cpp          # ✅ MVP：回环 spawn/快照/输入RPC/despawn 全链路（exit 0）
  rpc_smoke.cpp           # 后续：C→S RPC + S→C multicast
  bitwriter_test.cpp      # 后续：位读写 + 量化往返
```

CMake：仍由 `cmake/CMakeLists.txt.gns` 在 `DSE_ENABLE_NET=ON` 时把上述源纳入 `dse_net`；新增 smoke 加入 ctest（opt-in）。

---

## 6. API 草案（伪代码，最终以代码为准）

```cpp
namespace dse::net::repl {

// ── 服务器 ──
class ReplicationServer {
public:
    bool Start(uint16_t port, World& authoritative_world);
    void Tick(float dt);          // 采集变化 → AOI → delta → 发送；处理 ack/输入/RPC
    void Stop();
    // 控制权威世界中实体的网络可见性
    void MarkReplicated(Entity, ArchetypeId);
    void SetRelevancy(RelevancyPolicy);    // Always / Distance(R) / Grid
};

// ── 客户端 ──
class ReplicationClient {
public:
    bool Connect(const Address&, World& local_mirror_world);
    void Tick(float dt);          // 收 delta → 应用到镜像；发输入/ack/RPC
    void Disconnect();
    NetId LocalPlayer() const;
};

// ── RPC（宏展开为注册 + 类型安全 stub）──
DSE_RPC(RpcTarget::Server,    void RequestInteract(NetId target));
DSE_RPC(RpcTarget::Multicast, void PlayVfx(NetId at, uint16_t vfxId));

} // namespace dse::net::repl
```

---

## 7. 安全与防作弊

- **服务器权威**是第一道防线：客户端无法直接改权威状态。
- **Server RPC 必校验**：调用者是否拥有该实体、参数范围、冷却/频率（rate limit）。
- **输入而非状态**：客户端发输入意图，不发"我现在在哪/血量多少"。
- **传输加密**：GNS 自带（AES-GCM + curve25519），防嗅探/篡改。
- 反速度/穿墙等：服务器侧做合理性检测（移动距离上限、碰撞校验）。

---

## 8. 分阶段路线图

| Phase | 内容 | 交付物 / 验收 |
|---|---|---|
| **0** | 本设计文档评审 | 本文档（待评审） |
| **1** | `serialize/`：`NetBitWriter/Reader` + 量化 + 单测 | `bitwriter_test` 往返通过 |
| **2** | `NetId`/`NetWorld` + `ArchetypeId` 注册表 + spawn/despawn（可靠道）回环 | `repl_smoke`：client 镜像出现/消失 |
| **3** | `ReplicationTraits` + 全量快照复制（先不做 delta） | server 改 Transform → client 收到（回环 exit 0） |
| **4** | 增量（delta）+ per-conn baseline + ack（不可靠道 + 环形缓冲） | 带宽较全量显著下降；丢包下最终一致 |
| **5** | AOI（Always + 距离裁剪）+ lane/带宽预算 | 大量实体下每连接只收相关子集 |
| **6** | RPC（C→S 校验、S→C/multicast）+ 宏/注册表 + 单测 | `rpc_smoke` 双向通过 |
| **7** | C ABI `dse_repl_*` + Lua `dse.repl` 绑定 | Lua 脚本可注册 RPC / 标注复制（lua smoke） |
| **8（进阶）** | 客户端预测 + 服务器校正 + 远端插值 | 高延迟下手感平滑（演示工程） |
| **后续** | P2P/ICE（开 WebRTC submodule）、host-migration、网格 AOI、回放 | 独立工程 |

每阶段：补回环/单元测试 + 更新本文档进度 + 三端 `-WithNet` 回归保持绿。

### 8.1 MVP 已实现（最小可用联机闭环）

本次落地了一个**最小但端到端可用**的复制闭环，覆盖 Phase 1–3 与 Phase 6 的核心（输入 RPC），默认 `DSE_ENABLE_NET=OFF` 零回归：

| 能力 | 实现 |
|---|---|
| 字节序列化 | `serialize/byte_stream.h`（`ByteWriter/ByteReader`，小端 POD） |
| 网络身份 | `repl_protocol.h`：`NetId`（MVP 用 `uint32_t` 自增）、`MsgType`、`ArchetypeId` |
| Transform 编解码 | `repl_transform_codec.h`：position+rotation+scale = 10×f32，两端共用 |
| 服务器权威复制 | `ReplicationServer`：`MarkReplicated(e, owner)/SetOwner/Unreplicate/Tick`，广播 spawn/despawn（可靠）+ 全量快照（不可靠） |
| 输入 RPC 权威校验 | InputMove 三重校验：①目标存在且存活 ②**来源连接为该实体属主**（非属主拒绝）③幅度上限；另有**每连接每-tick 输入限流**（防 flood） |
| 客户端镜像 | `ReplicationClient`：应用 spawn/despawn/快照到镜像 registry，`SendMove` 发输入；late-join 由服务器 `OnConnected` 补发当前全部 spawn |
| 回环测试 | `tests/net/repl_smoke.cpp` → ctest `dse_repl_smoke`：握手→spawn→快照一致→**所有权负例（非属主移动被拒）**→属主输入RPC服务器权威移动→despawn，exit 0 |

**MVP 的有意简化（对应后续 Phase / §10 技术债）：**
- 只复制 `TransformComponent` 一种组件（Phase 3 的 `ReplicationTraits<T>` 泛化留后续）。
- **内存：**`ByteWriter` 走 `std::vector` 默认 allocator，**未计入 `MemoryTag::Net`**，且每条消息一次堆分配（未用对象池）——偏离 §4 内存集成（→ §10.1 D-7）。
- **全量快照单包发送，未分片/未限 MTU**：实体多时单个不可靠消息可能超 GNS 上限（→ §10.1 D-8，Phase 4 delta + 分片解决）。
- **无协议版本握手**（§4.12 已设计但 MVP 未实现）：双端构建不一致会错误解析字节（→ §10.1 D-9）。
- **四元数镜像端直接覆盖、未重新归一化**（MVP 无量化故暂无误差，属隐患）。
- **全量快照**，无 delta/baseline/ack（Phase 4）。
- 无 AOI，所有被标记实体发给所有客户端（Phase 5）。
- 全部走默认 lane 0（靠 `SendMode` 区分可靠/不可靠），未做多 lane 优先级调度（Phase 5）。
- `NetId` 用 `uint32_t` 自增，未用 §4.2 的 `{index,generation}` 紧凑句柄 + 位压缩（后续）。
- 字节对齐、无量化压缩（Phase 1 进阶：换 `byte_stream` 为位级 `NetBitWriter`，不改协议外形）。
- 未做客户端预测/插值（Phase 8）、通用 RPC 注册表（Phase 6 仅手写 InputMove 一条）、C ABI / Lua 绑定（Phase 7）。
- 字节流假设小端（当前所有目标平台均小端）。

**单传输实例约束（§10 D-5）的体现**：回环测试用一个 transport 实例同时承载 server 与 client，靠一个路由监听器按连接 id 把事件分发给 `ReplicationServer`/`ReplicationClient`；真实部署中专用服务器进程与客户端进程各自只有一个监听者，直接 `transport->Poll(server_or_client)` 即可。

---

## 9. 测试策略

- **位读写往返**：`NetBitWriter→Reader` 对所有量化类型往返一致（含边界）。
- **复制回环**（单进程 127.0.0.1）：server 世界改一个组件 → client 镜像世界读到一致值；spawn/despawn 镜像同步。
- **RPC 回环**：C→S 调用被服务器收到并校验；S→C multicast 到达所有相关连接。
- **丢包/乱序**（可选注入）：delta + ack 在丢包下最终一致。
- 全部 `DSE_ENABLE_NET=ON` 时编译，加入 ctest opt-in；OFF 构建零影响。

---

## 10. 技术债与范围限制（诚实登记）

> 本方案是"方向最优 + 务实折中"，并非"零技术债"。以下显式登记，避免埋雷。

### 10.1 主动承担的技术债

| 编号 | 债务 | 现状折中 | 理想终态 / 偿还路径 |
|---|---|---|---|
| **D-1** | **双序列化路径 schema 漂移** | 存档走 rapidjson、复制走 `NetBitWriter`，字段描述两份，加字段易漏改 | 统一反射/schema 单一真相源，自动生成存档 + 复制两套编解码 |
| **D-2** | **预测范围受物理非确定性限制** | 预测仅覆盖简单运动学角色；物理实体服务器权威 + 插值 | 引入确定性子模拟（定点/固定步长）或回滚专用简化物理 |
| **D-3** | **固定 tick 主循环依赖** | 复制需独立固定步长 tick，当前主循环可能没有 | 改造引擎主循环：模拟 tick 与渲染帧解耦 |
| **D-4** | **协议版本 / 长期兼容** | 含握手版本号，但无字段级向后兼容策略 | 字段级可选 / 版本化 schema（配合 D-1） |
| **D-5** | **单 transport 实例（既有债）** | GNS 后端全局 `s_active`，同进程仅一个 transport | 回调路由实例化（去全局态），支持同进程多栈（利于测试/多世界） |
| **D-6** | **会话恢复 / 断线续连** | 重连 = 新会话，重发 baseline | 保留 NetId 映射做断线续连（可选） |
| **D-7** | **复制层 MVP 未走内存门面** | `ByteWriter` 用 `std::vector` 默认 allocator，未计入 `MemoryTag::Net`，每条消息一次堆分配 | 改用计入 `MemoryTag::Net` 的容器/分配器 + 复用对象池（与传输层 `Send` 处的分配债一并偿还） |
| **D-8** | **全量快照未分片 / 未限 MTU** | 整个快照打成一个不可靠消息，实体多时可能超 GNS 单消息上限 | Phase 4 delta 后按 MTU 分片 / 多包，并随 baseline+ack 控量 |
| **D-9** | **MVP 无协议版本握手** | §4.12 已设计版本号握手，但 MVP 复制协议未实现；双端构建不一致会错误解析 | 落地 §4.12 握手：连接建立后先交换协议版本，不匹配则拒绝 |

> MVP 已偿还的债：**输入 RPC 所有权校验 + 频率限流**（§7 要求的"调用者是否拥有该实体 + rate limit"已在 `ReplicationServer::OnMessage` 实现，不再是债）。

### 10.2 范围限制与风险

| 项 | 说明 | 缓解 |
|---|---|---|
| 复制层是大模块 | 工作量与复杂度高 | 严格分阶段，每阶段独立验收 |
| 强依赖网络序列化 | 没有紧凑二进制就做不了 delta | Phase 1 先做 `serialize/` |
| 仅直连无 NAT 穿透 | 公网联机需服务器/端口转发 | dedicated server 不受影响；P2P 留后续 |
| 量化精度 vs 带宽 | 压太狠会抖动 | 按组件可调位宽，关键量保高精 |
| AOI 扩展性 | 距离裁剪 O(N×连接)/tick | 大世界上网格 AOI（配合 `LARGE_WORLD_COORDINATES`） |
| 反作弊深度 | 完整反作弊是独立工程 | 本期只做服务器权威 + 输入校验 + 频率限制 |
| API 实验性 | 传输与复制 API 可能变 | 标注实验；稳定后纳入 SDK |

---

## 11. 验收标准（整体）

1. `DSE_ENABLE_NET=OFF`（默认）时，三端构建与现状完全一致，无任何网络符号/依赖。
2. `ON` 时，传输回环（现有 `net_smoke` 系列）+ 新增复制/RPC 回环 smoke 全部 exit 0。
3. 复制层只依赖 `INetTransport` 抽象与 ECS，不 include GNS 头、不污染现有公共接口。
4. 所有网络分配计入 `MemoryTag::Net`，可在内存视图中看到。
5. 文档随每阶段更新进度，与代码一致。

---

## 12. 非目标（本方案明确不做）

- 不在本方案内实现 P2P/ICE/NAT 穿透与 host-migration（传输底座已关 ICE，留后续）。
- 不替换现有 rapidjson 存档序列化（存档与复制是两条路径）。
- 不强制把现有玩法/组件接入复制（与内存/STL 适配器一致：先提供能力 + 示范，按需接入）。
- 不在首版做客户端预测/回滚（列为进阶 Phase 8）。
