/**
 * @file repl_lua_smoke.cpp
 * @brief 无头验证 Lua 绑定 dse.repl：在裸 lua_State 上注册 Phase1 API，
 *        用 Lua 脚本创建 ReplicationServer + ReplicationClient，验证
 *        dse.repl.* 接口可正确创建/初始化/操作（不依赖实际网络连接）。
 *
 * 此测试验证：
 * 1. dse.repl 模块表存在且包含所有声明的函数
 * 2. server_create / client_create 返回有效 userdata
 * 3. server_set_aoi / server_seq / server_client_count 可调用
 * 4. RPC 注册函数可调用
 *
 * 退出码：0 = 通过；非 0 = 失败。
 */

#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cstdio>
#include <string>

#ifdef DSE_NET_ENABLED

int main() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    dse::runtime::lua_binding::RegisterPhase1LuaApi(L);

    // 验证 dse.repl 存在
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "repl");
    if (!lua_istable(L, -1)) {
        std::fprintf(stderr, "[repl-lua-smoke] FAIL: dse.repl not a table\n");
        lua_close(L);
        return 1;
    }

    // 验证所有函数存在
    const char* expected_funcs[] = {
        "server_create", "server_init", "server_mark", "server_set_owner",
        "server_unreplicate", "server_tick", "server_set_aoi",
        "server_client_count", "server_seq",
        "client_create", "client_init", "client_send_move",
        "client_connected", "client_mirror_count", "client_to_entity",
        "rpc_server_register", "rpc_client_register",
        "rpc_client_send", "rpc_server_broadcast"
    };
    constexpr int func_count = sizeof(expected_funcs) / sizeof(expected_funcs[0]);

    for (int i = 0; i < func_count; ++i) {
        lua_getfield(L, -1, expected_funcs[i]);
        if (!lua_isfunction(L, -1)) {
            std::fprintf(stderr, "[repl-lua-smoke] FAIL: dse.repl.%s not a function\n",
                         expected_funcs[i]);
            lua_close(L);
            return 2;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 2);  // repl + dse

    // 创建 server/client userdata
    const char* script =
        "local srv = dse.repl.server_create()\n"
        "assert(srv, 'server_create returned nil')\n"
        "local cli = dse.repl.client_create()\n"
        "assert(cli, 'client_create returned nil')\n"
        // 测试常量
        "assert(dse.repl.AOI_ALWAYS == 0, 'AOI_ALWAYS')\n"
        "assert(dse.repl.AOI_DISTANCE == 1, 'AOI_DISTANCE')\n"
        "assert(dse.repl.RPC_SERVER == 0, 'RPC_SERVER')\n"
        "assert(dse.repl.RPC_CLIENT == 1, 'RPC_CLIENT')\n"
        "assert(dse.repl.RPC_MULTICAST == 2, 'RPC_MULTICAST')\n"
        // server 操作（不需要真实传输层，只验证 API 可调用）
        "dse.repl.server_set_aoi(srv, 'always', 100.0)\n"
        "dse.repl.server_set_aoi(srv, 'distance', 50.0)\n"
        "local seq = dse.repl.server_seq(srv)\n"
        "assert(type(seq) == 'number', 'seq should be number')\n"
        "local count = dse.repl.server_client_count(srv)\n"
        "assert(type(count) == 'number', 'client_count should be number')\n"
        // client 操作
        "local connected = dse.repl.client_connected(cli)\n"
        "assert(connected == false, 'should not be connected')\n"
        "local mirror_count = dse.repl.client_mirror_count(cli)\n"
        "assert(type(mirror_count) == 'number', 'mirror_count should be number')\n"
        // RPC 注册
        "local rpc_id = dse.repl.rpc_server_register(srv, 'test_rpc', 'server',\n"
        "    function(sender, target, payload) return true end)\n"
        "assert(type(rpc_id) == 'number', 'rpc_id should be number')\n"
        "local rpc_id2 = dse.repl.rpc_client_register(cli, 'test_rpc2', 'server',\n"
        "    function(sender, target, payload) return true end)\n"
        "assert(type(rpc_id2) == 'number', 'rpc_id2 should be number')\n"
        "return true\n";

    if (luaL_dostring(L, script) != LUA_OK) {
        std::fprintf(stderr, "[repl-lua-smoke] script error: %s\n",
                     lua_tostring(L, -1));
        lua_close(L);
        return 3;
    }

    // 检查返回值
    bool ok = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    // GC 测试（确保 userdata 可被回收不崩溃）
    lua_gc(L, LUA_GCCOLLECT, 0);

    lua_close(L);

    if (ok) {
        std::printf("[repl-lua-smoke] PASS: all dse.repl.* APIs verified\n");
        return 0;
    } else {
        std::fprintf(stderr, "[repl-lua-smoke] FAIL: unexpected return\n");
        return 4;
    }
}

#else  // !DSE_NET_ENABLED

int main() {
    std::printf("[repl-lua-smoke] SKIP: DSE_NET_ENABLED not defined\n");
    return 0;
}

#endif
