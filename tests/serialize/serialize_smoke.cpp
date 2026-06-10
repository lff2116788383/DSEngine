/**
 * @file serialize_smoke.cpp
 * @brief 无头验证 Lua 绑定 dse.serialize：在裸 lua_State 上注册 Phase1 API，
 *        用脚本对多种值/嵌套表做 encode→decode 往返，深度比较是否一致；
 *        并校验 decode 返回的「下一个位置」可用于顺序解码拼接的多个值。
 *        纯 Lua/C，无任何外部依赖、不依赖窗口/RHI。
 *
 * 退出码：0 = 全部往返一致；非 0 = 失败。
 */
#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cstdio>

int main() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    dse::runtime::lua_binding::RegisterPhase1LuaApi(L);

    const char* script = R"LUA(
        assert(dse.serialize, 'dse.serialize missing')
        local enc, dec = dse.serialize.encode, dse.serialize.decode

        local function deep_eq(a, b)
            if type(a) ~= type(b) then return false end
            if type(a) ~= 'table' then return a == b end
            local na = 0
            for k, v in pairs(a) do na = na + 1; if not deep_eq(v, b[k]) then return false end end
            local nb = 0
            for _ in pairs(b) do nb = nb + 1 end
            return na == nb
        end

        local function roundtrip(v)
            local s = enc(v)
            assert(type(s) == 'string', 'encode must return string')
            local out, nextpos = dec(s)
            assert(nextpos == #s + 1, 'next pos must be end+1, got '..tostring(nextpos)..' len '..#s)
            assert(deep_eq(v, out), 'roundtrip mismatch')
        end

        -- 标量
        roundtrip(nil)
        roundtrip(true)
        roundtrip(false)
        roundtrip(0)
        roundtrip(-1)
        roundtrip(123456789)
        roundtrip(-987654321)
        roundtrip(3.14159)
        roundtrip(-0.5)
        roundtrip('')
        roundtrip('hello world')
        roundtrip('binary\0with\0zeros')   -- 二进制安全

        -- 整数 vs 浮点 子类型应被保留
        do
            local i = dec(enc(5))
            local f = dec(enc(5.0))
            assert(math.type(i) == 'integer', 'integer subtype lost')
            assert(math.type(f) == 'float', 'float subtype lost')
        end

        -- 数组 / 字典 / 嵌套
        roundtrip({1, 2, 3})
        roundtrip({})                       -- 空表
        roundtrip({x = 1, y = 'two', [5] = true})
        roundtrip({
            name = 'npc_42',
            pos = { x = 1.5, y = -2.0 },
            tags = { 'enemy', 'boss' },
            stats = { hp = 100, mp = 50, alive = true },
            inventory = { {id=1, n=3}, {id=7, n=1} },
        })

        -- 顺序解码：拼接两个独立编码值，用 next_pos 串联
        do
            local blob = enc('first') .. enc({a = 1})
            local v1, p1 = dec(blob, 1)
            local v2, p2 = dec(blob, p1)
            assert(v1 == 'first', 'seq v1')
            assert(deep_eq(v2, {a = 1}), 'seq v2')
            assert(p2 == #blob + 1, 'seq end')
        end

        -- 截断数据应报错（pcall 捕获）
        do
            local s = enc('truncate-me')
            local ok = pcall(dec, string.sub(s, 1, 2))
            assert(not ok, 'truncated decode should error')
        end

        _G.SER_OK = true
    )LUA";

    int ret = 4;
    if (luaL_dostring(L, script) != LUA_OK) {
        std::fprintf(stderr, "[serialize-smoke] script error: %s\n", lua_tostring(L, -1));
    } else {
        lua_getglobal(L, "SER_OK");
        bool ok = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (ok) {
            std::fprintf(stderr, "SERIALIZE_SMOKE_PASS\n");
            ret = 0;
        } else {
            std::fprintf(stderr, "SERIALIZE_SMOKE_FAIL\n");
        }
    }
    lua_close(L);
    return ret;
}
