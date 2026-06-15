-- net_message.lua — dse.net + dse.serialize：收发结构化游戏消息
--
-- 演示用 dse.serialize 把 Lua 表一行编码成紧凑二进制，经 dse.net 可靠发送，
-- 接收端一行解码回表——无需手写字节拼装，也无需声明 schema。
--
-- 跑法：引擎宿主以 -DDSE_ENABLE_NET=ON 构建，将本脚本设为启动脚本（每帧自动 PumpNet）。

local PORT = 27620

assert(dse.net, "dse.net 不可用：请用 -DDSE_ENABLE_NET=ON 构建引擎")
assert(dse.serialize, "dse.serialize 不可用")
assert(dse.net.init(false), "dse.net 初始化失败")

local enc, dec = dse.serialize.encode, dse.serialize.decode

local client_conn = nil

dse.net.on("connected", function(conn)
    if conn ~= client_conn then return end
    -- 把一个结构化消息（嵌套表）一行编码后发出
    local msg = {
        type = "spawn",
        npc  = { id = 42, name = "守卫", hp = 100 },
        pos  = { x = 12.5, y = -3.0 },
        tags = { "enemy", "patrol" },
    }
    dse.net.send(conn, enc(msg), dse.net.RELIABLE, 0)
    dse.net.flush(conn)
end)

dse.net.on("message", function(conn, data, lane)
    -- 一行解码回 Lua 表
    local msg = dec(data)
    print(string.format("[srv] type=%s npc=%s(id=%d,hp=%d) pos=(%.1f,%.1f) tags=%s,%s",
        msg.type, msg.npc.name, msg.npc.id, msg.npc.hp,
        msg.pos.x, msg.pos.y, msg.tags[1], msg.tags[2]))
end)

assert(dse.net.listen(PORT), "listen 失败")
client_conn = dse.net.connect("127.0.0.1", PORT)
assert(client_conn ~= 0, "connect 失败")

-- 无头驱动（引擎宿主则每帧自动 PumpNet）：
--   for _ = 1, 200 do dse.net.poll() end
-- 退出前：dse.net.shutdown()
