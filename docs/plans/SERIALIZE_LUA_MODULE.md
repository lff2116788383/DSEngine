# dse.serialize — Lua 自描述二进制序列化

减少脚本层手动拼/解字节的工作量：把任意 Lua 值（含嵌套表）一行编码成紧凑二进制串，
再一行解码回来。典型用途是配合 `dse.net` 收发结构化游戏消息。

- 状态：✅ 已实现并验证（`dse_serialize_smoke.exe` EXIT=0 / `SERIALIZE_SMOKE_PASS`）。
- 始终可用：纯 Lua/C，无外部依赖，不受 `DSE_ENABLE_NET` / `DSE_ENABLE_HTTP` 开关影响
  （随 `dse_engine` 编译，启用 Lua 即有）。
- 绑定实现：`engine/scripting/lua/bindings/lua_binding_serialize.cpp`。

## API

```lua
dse.serialize.encode(value)            -> string
-- 把 value 编码成二进制串。支持：nil / boolean / integer / number(double) /
-- string（二进制安全）/ table（数组或字典，可任意嵌套）。
-- 不支持 function / userdata / thread（报错）；表嵌套上限 100 层（防环引用）。

dse.serialize.decode(string [, pos])   -> value, next_pos
-- 从 pos（1 基，默认 1）解码出一个值；next_pos 是紧随其后的下一个字节位置，
-- 可用于顺序解码拼接的多个值。数据被截断/损坏时报错（用 pcall 可捕获）。
```

## 与 Lua 5.4 `string.pack` 的区别

| | `string.pack/unpack` | `dse.serialize` |
|---|---|---|
| 形态 | 需预先写死格式串，适合定长结构 | 自描述（类型随数据写入），适合动态/嵌套表 |
| schema | 收发双方都要约定格式串 | 无需声明，直接编码任意值 |
| 嵌套表 | 不直接支持 | 原生支持任意嵌套 array/map |

定长、高频的小结构用 `string.pack` 更省字节；动态/嵌套消息用 `dse.serialize` 更省心。

## 配合 dse.net 用（推荐）

```lua
local enc, dec = dse.serialize.encode, dse.serialize.decode

-- 发送端：一行编码结构化消息
dse.net.send(conn, enc({ type="spawn", npc={id=42, hp=100}, pos={x=1.5,y=-3} }),
             dse.net.RELIABLE, 0)

-- 接收端：一行解码回表
dse.net.on("message", function(conn, data, lane)
    local msg = dec(data)
    print(msg.type, msg.npc.id, msg.pos.x)
end)
```

完整示例：`examples/lua/net_message.lua`。

## 二进制格式（参考；小端，标签 1 字节）

```
0=nil  1=false  2=true
3=int    : zigzag LEB128（有符号 64 位）
4=number : 8 字节 IEEE-754 double（小端）
5=string : ULEB128 长度 + 原始字节
6=array  : ULEB128 个数 + 依次 N 个值
7=map    : ULEB128 对数 + 依次 N 对 (key, value)
```

整数与浮点子类型分别走标签 3/4，往返后 `math.type` 保持不变。
格式无版本号、自描述，跨端一致即可互通。

## 验证

```powershell
# 随 -WithNet 一起验证（复用同一 LUA 构建目录）
.\scripts\verify_windows_build.ps1 -WithNet -NetOnly
# 或单独构建运行
cmake --build build_net_lua --config Debug --target dse_serialize_smoke
.\bin\dse_serialize_smoke.exe   # -> SERIALIZE_SMOKE_PASS, EXIT=0
```

覆盖：标量往返、integer/float 子类型保留、二进制安全字符串、嵌套数组+字典、
顺序解码（`next_pos` 串联）、截断数据报错。
