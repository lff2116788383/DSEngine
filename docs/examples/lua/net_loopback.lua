-- net_loopback.lua — dse.net（GameNetworkingSockets）回环示例
--
-- 演示用 dse.net 在同进程内起 server(listen) + client(connect)，握手后
-- 客户端经多 lane 发可靠/非可靠消息，服务端通过事件回调收消息。
--
-- 定位提醒：
--   dse.net  = 游戏专用 UDP 可靠/非可靠传输（客户端↔服务端 / 状态同步）。
--   dse.http = 跟 DeepSeek 等 HTTPS REST 服务通信（AI NPC 走这个）。
--   两者互补，dse.net 不能用来连 HTTPS API。
--
-- 跑法（引擎宿主需以 -DDSE_ENABLE_NET=ON 构建）：
--   把本脚本设为启动脚本，引擎每帧 Tick 会自动 PumpNet 派发回调；
--   或在无头环境里自行循环调用 dse.net.poll()（见 tests/net/net_lua_smoke.cpp）。

local PORT = 27600

assert(dse.net, "dse.net 不可用：请用 -DDSE_ENABLE_NET=ON 构建引擎")
assert(dse.net.init(false), "dse.net 初始化失败")

local client_conn = nil
local sent = false

-- 事件回调：连接中 / 已连接 / 断开 / 收到消息
dse.net.on("connecting", function(conn, host, port)
    print(string.format("[net] 连接中 conn=%d 对端=%s:%d", conn, host, port))
end)

dse.net.on("connected", function(conn)
    print(string.format("[net] 已连接 conn=%d", conn))
    -- 客户端这一侧连上后：配置 2 条 lane（防队头阻塞），分别发可靠/非可靠消息
    if conn == client_conn and not sent then
        sent = true
        dse.net.configure_lanes(conn, { 0, 0 }, { 1, 1 })          -- priorities, weights
        dse.net.send(conn, "HELLO-RELIABLE",  dse.net.RELIABLE,   0)
        dse.net.send(conn, "PING-UNRELIABLE", dse.net.UNRELIABLE, 1)
        dse.net.flush(conn)                                         -- 立即发送，绕过 Nagle 合批
    end
end)

dse.net.on("closed", function(conn, reason)
    print(string.format("[net] 断开 conn=%d reason=%d", conn, reason))
end)

dse.net.on("message", function(conn, data, lane)
    print(string.format("[net] 收到 conn=%d lane=%d data=%q", conn, lane, data))
    -- 服务端收到后可回包；这里查一下链路质量
    local q = dse.net.get_quality(conn)
    if q then
        print(string.format("       质量 ping=%.1fms loss=%.3f out=%.0fB/s in=%.0fB/s pending=%d",
            q.ping_ms, q.packet_loss, q.out_bytes_per_sec, q.in_bytes_per_sec, q.pending_reliable))
    end
end)

-- 起服务端 + 客户端（同进程回环）
assert(dse.net.listen(PORT), "listen 失败")
client_conn = dse.net.connect("127.0.0.1", PORT)
assert(client_conn ~= 0, "connect 失败")

-- 引擎宿主每帧自动 PumpNet；若需手动驱动（无头）：
--   for _ = 1, 200 do dse.net.poll(); end
-- 退出前清理：
--   dse.net.shutdown()
