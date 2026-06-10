/**
 * @file lua_binding_serialize.cpp
 * @brief Lua 绑定：自描述二进制序列化 (dse.serialize)
 *
 * 目标：减少脚本层手动拼/解字节的工作量——把任意 Lua 值（nil/boolean/integer/
 * number/string + 任意嵌套的 array/map 表）一行编码成紧凑二进制串，再一行解码回来。
 * 典型用途：配合 dse.net 收发结构化游戏消息（dse.net.send(conn, dse.serialize.encode(t))）。
 *
 * 与 Lua 5.4 自带的 string.pack/unpack 的区别：后者需要预先写死格式串、适合定长结构；
 * 本模块是「自描述」的（类型随数据写入），适合动态/嵌套的表，无需脚本声明 schema。
 *
 * API：
 *   dse.serialize.encode(value)            -> string                 编码任意支持的 Lua 值
 *   dse.serialize.decode(string [, pos])   -> value, next_pos        从 pos(默认1) 解码一个值
 *
 * 支持类型：nil / boolean / integer / number(double) / string / table(数组或字典，可嵌套)。
 * 不支持：function / userdata / thread（编码报错）。表深度上限 100（防环引用无限递归）。
 *
 * 二进制格式（小端，无需版本号；标签 1 字节）：
 *   0=nil 1=false 2=true
 *   3=int   : zigzag LEB128（有符号 64 位）
 *   4=number: 8 字节 IEEE-754 double（小端）
 *   5=string: ULEB128 长度 + 原始字节
 *   6=array : ULEB128 个数 + 依次 N 个值
 *   7=map   : ULEB128 对数 + 依次 N 对 (key, value)
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include <cstdint>
#include <cstring>
#include <string>

namespace dse::runtime::lua_binding {
namespace {

enum Tag : uint8_t {
    T_NIL = 0, T_FALSE = 1, T_TRUE = 2,
    T_INT = 3, T_NUM = 4, T_STR = 5, T_ARR = 6, T_MAP = 7
};

constexpr int kMaxDepth = 100;

// ── 编码 ──（错误信息一律用字符串字面量；出错时不残留有堆分配的 std::string 局部）
struct Encoder {
    std::string out;
    const char* err = nullptr;

    void PutByte(uint8_t b) { out.push_back(static_cast<char>(b)); }
    void PutULEB(uint64_t v) {
        do {
            uint8_t b = v & 0x7f;
            v >>= 7;
            if (v) b |= 0x80;
            PutByte(b);
        } while (v);
    }
    void PutSLEB(int64_t s) {
        uint64_t zz = (static_cast<uint64_t>(s) << 1) ^ static_cast<uint64_t>(s >> 63);
        PutULEB(zz);
    }

    bool Value(lua_State* L, int idx, int depth) {
        if (err) return false;
        if (depth > kMaxDepth) { err = "table 嵌套过深（可能存在环引用）"; return false; }
        idx = lua_absindex(L, idx);
        switch (lua_type(L, idx)) {
            case LUA_TNIL:     PutByte(T_NIL);   return true;
            case LUA_TBOOLEAN: PutByte(lua_toboolean(L, idx) ? T_TRUE : T_FALSE); return true;
            case LUA_TNUMBER:
                if (lua_isinteger(L, idx)) {
                    PutByte(T_INT);
                    PutSLEB(static_cast<int64_t>(lua_tointeger(L, idx)));
                } else {
                    PutByte(T_NUM);
                    double d = static_cast<double>(lua_tonumber(L, idx));
                    char buf[8];
                    std::memcpy(buf, &d, 8);
                    out.append(buf, 8);
                }
                return true;
            case LUA_TSTRING: {
                size_t len = 0;
                const char* s = lua_tolstring(L, idx, &len);
                PutByte(T_STR);
                PutULEB(len);
                out.append(s, len);
                return true;
            }
            case LUA_TTABLE:   return Table(L, idx, depth);
            default:           err = "不支持的类型（function/userdata/thread 无法序列化）"; return false;
        }
    }

    bool Table(lua_State* L, int idx, int depth) {
        const uint64_t seq = static_cast<uint64_t>(lua_rawlen(L, idx));
        // 统计总键数
        uint64_t total = 0;
        lua_pushnil(L);
        while (lua_next(L, idx) != 0) { ++total; lua_pop(L, 1); }

        if (total == seq) {
            // 纯序列（含空表）→ 数组
            PutByte(T_ARR);
            PutULEB(seq);
            for (uint64_t i = 1; i <= seq; ++i) {
                lua_geti(L, idx, static_cast<lua_Integer>(i));
                bool ok = Value(L, -1, depth + 1);
                lua_pop(L, 1);
                if (!ok) return false;
            }
        } else {
            // 字典 → map
            PutByte(T_MAP);
            PutULEB(total);
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                // key 在 -2，value 在 -1；复制 key 以免 Value 内转换原 key 干扰 lua_next
                lua_pushvalue(L, -2);
                bool ok = Value(L, -1, depth + 1);   // 编码 key 副本
                lua_pop(L, 1);                        // 弹 key 副本，恢复 value 在 -1
                if (ok) ok = Value(L, -1, depth + 1); // 编码 value
                lua_pop(L, 1);                        // 弹 value，保留 key 供 next
                if (!ok) { lua_pop(L, 1); return false; }  // 出错：弹掉残留 key
            }
        }
        return true;
    }
};

// ── 解码 ──（直接把结果压栈；出错用 luaL_error + 字面量）
struct Decoder {
    lua_State*  L;
    const char* p;
    size_t      n;
    size_t      pos;

    uint8_t Byte() {
        if (pos >= n) luaL_error(L, "dse.serialize.decode: 数据被截断");
        return static_cast<uint8_t>(p[pos++]);
    }
    uint64_t ULEB() {
        uint64_t result = 0; int shift = 0;
        for (;;) {
            uint8_t b = Byte();
            result |= static_cast<uint64_t>(b & 0x7f) << shift;
            if (!(b & 0x80)) break;
            shift += 7;
            if (shift > 63) luaL_error(L, "dse.serialize.decode: 变长整数溢出");
        }
        return result;
    }
    int64_t SLEB() {
        uint64_t zz = ULEB();
        return static_cast<int64_t>((zz >> 1) ^ (~(zz & 1) + 1));
    }

    void Value(int depth) {
        if (depth > kMaxDepth) luaL_error(L, "dse.serialize.decode: 嵌套过深");
        luaL_checkstack(L, 4, "dse.serialize.decode 栈空间不足");
        uint8_t tag = Byte();
        switch (tag) {
            case T_NIL:   lua_pushnil(L); break;
            case T_FALSE: lua_pushboolean(L, 0); break;
            case T_TRUE:  lua_pushboolean(L, 1); break;
            case T_INT:   lua_pushinteger(L, static_cast<lua_Integer>(SLEB())); break;
            case T_NUM: {
                if (pos + 8 > n) luaL_error(L, "dse.serialize.decode: 数据被截断(number)");
                double d; std::memcpy(&d, p + pos, 8); pos += 8;
                lua_pushnumber(L, static_cast<lua_Number>(d));
                break;
            }
            case T_STR: {
                uint64_t len = ULEB();
                if (pos + len > n) luaL_error(L, "dse.serialize.decode: 数据被截断(string)");
                lua_pushlstring(L, p + pos, static_cast<size_t>(len));
                pos += static_cast<size_t>(len);
                break;
            }
            case T_ARR: {
                uint64_t cnt = ULEB();
                lua_createtable(L, cnt > INT32_MAX ? 0 : static_cast<int>(cnt), 0);
                for (uint64_t i = 1; i <= cnt; ++i) {
                    Value(depth + 1);                       // value 压栈
                    lua_rawseti(L, -2, static_cast<lua_Integer>(i));
                }
                break;
            }
            case T_MAP: {
                uint64_t cnt = ULEB();
                lua_createtable(L, 0, cnt > INT32_MAX ? 0 : static_cast<int>(cnt));
                for (uint64_t i = 0; i < cnt; ++i) {
                    Value(depth + 1);   // key
                    Value(depth + 1);   // value
                    lua_settable(L, -3);
                }
                break;
            }
            default:
                luaL_error(L, "dse.serialize.decode: 未知标签 %d", (int)tag);
        }
    }
};

int L_Encode(lua_State* L) {
    luaL_checkany(L, 1);
    bool ok;
    std::string result;          // 成功时持有结果；失败路径保持空串（无堆分配，longjmp 安全）
    const char* errmsg = nullptr;
    {
        Encoder e;
        ok = e.Value(L, 1, 0);
        if (ok) result.swap(e.out);
        else    errmsg = e.err;
    }
    if (!ok) return luaL_error(L, "dse.serialize.encode: %s", errmsg ? errmsg : "未知错误");
    lua_pushlstring(L, result.data(), result.size());
    return 1;
}

int L_Decode(lua_State* L) {
    size_t n = 0;
    const char* s = luaL_checklstring(L, 1, &n);
    lua_Integer start = luaL_optinteger(L, 2, 1);   // 1 基
    if (start < 1) start = 1;
    Decoder d{ L, s, n, static_cast<size_t>(start - 1) };
    d.Value(0);                                       // 结果压栈 (1 个值)
    lua_pushinteger(L, static_cast<lua_Integer>(d.pos) + 1);  // 下一个读取位置(1 基)
    return 2;
}

} // anonymous namespace

void RegisterSerializeBindings(lua_State* L) {
    lua_newtable(L);
    const luaL_Reg funcs[] = {
        {"encode", L_Encode},
        {"decode", L_Decode},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, funcs, 0);
}

} // namespace dse::runtime::lua_binding
