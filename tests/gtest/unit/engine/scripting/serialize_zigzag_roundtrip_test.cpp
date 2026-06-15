/**
 * @file serialize_zigzag_roundtrip_test.cpp
 * @brief dse.serialize 整数 SLEB128/zigzag 编码往返测试。
 *
 * 重点覆盖有符号 zigzag 变长编码（PutSLEB）：负数、零、正数，
 * 以及 64 位边界值 INT64_MIN/INT64_MAX —— 验证符号位处理在极值下正确。
 * 纯 Lua/C，不依赖窗口/RHI/3D。
 */

#include <gtest/gtest.h>

#include "engine/scripting/lua/bindings/lua_binding_registry.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
#include "depends/lua/lualib.h"
}

#include <cstdint>
#include <limits>

namespace {

class SerializeZigzagRoundtripTest : public ::testing::Test {
protected:
    void SetUp() override {
        L_ = luaL_newstate();
        ASSERT_NE(L_, nullptr);
        luaL_openlibs(L_);
        dse::runtime::lua_binding::RegisterPhase1LuaApi(L_);
        // 确认 dse.serialize 已注册
        ASSERT_EQ(luaL_dostring(L_, "assert(dse.serialize and dse.serialize.encode and dse.serialize.decode)"), 0)
            << lua_tostring(L_, -1);
    }

    void TearDown() override {
        if (L_) lua_close(L_);
    }

    // 将整数 v 经 encode→decode 往返，返回解码出的整数；同时校验解码后的下一个
    // 读取位置恰好等于编码串长度 + 1（即整串被完整消费）。
    int64_t RoundtripInt(int64_t v) {
        lua_pushinteger(L_, static_cast<lua_Integer>(v));
        lua_setglobal(L_, "V");
        const char* script =
            "local s = dse.serialize.encode(V)\n"
            "assert(type(s) == 'string', 'encode must return string')\n"
            "local out, nextpos = dse.serialize.decode(s)\n"
            "assert(nextpos == #s + 1, 'next pos must be end+1')\n"
            "assert(math.type(out) == 'integer', 'decoded value must stay integer')\n"
            "RESULT = out\n";
        int rc = luaL_dostring(L_, script);
        EXPECT_EQ(rc, 0) << (rc ? lua_tostring(L_, -1) : "");
        lua_getglobal(L_, "RESULT");
        int64_t result = static_cast<int64_t>(lua_tointeger(L_, -1));
        lua_pop(L_, 1);
        return result;
    }

    lua_State* L_ = nullptr;
};

// 测试 序列化锯齿往返：零且Small Magnitudes
TEST_F(SerializeZigzagRoundtripTest, ZeroAndSmallMagnitudes) {
    EXPECT_EQ(RoundtripInt(0), 0);
    EXPECT_EQ(RoundtripInt(1), 1);
    EXPECT_EQ(RoundtripInt(-1), -1);
    EXPECT_EQ(RoundtripInt(2), 2);
    EXPECT_EQ(RoundtripInt(-2), -2);
    EXPECT_EQ(RoundtripInt(63), 63);
    EXPECT_EQ(RoundtripInt(-63), -63);
}

// 测试 序列化锯齿往返：多Byte Varint Boundaries
TEST_F(SerializeZigzagRoundtripTest, MultiByteVarintBoundaries) {
    // 跨越 SLEB128 单字节/多字节边界的值
    const int64_t values[] = {127, 128, -128, -129, 16383, 16384, -16384, -16385,
                              1000000, -1000000, 2147483647LL, -2147483648LL};
    for (int64_t v : values) {
        EXPECT_EQ(RoundtripInt(v), v) << "roundtrip failed for " << v;
    }
}

// 测试 序列化锯齿往返：整数64 Extremes
TEST_F(SerializeZigzagRoundtripTest, Int64Extremes) {
    // 直接命中 zigzag 符号位扩展（s >> 63）在 64 位极值下的正确性
    EXPECT_EQ(RoundtripInt(std::numeric_limits<int64_t>::max()),
              std::numeric_limits<int64_t>::max());
    EXPECT_EQ(RoundtripInt(std::numeric_limits<int64_t>::min()),
              std::numeric_limits<int64_t>::min());
    EXPECT_EQ(RoundtripInt(std::numeric_limits<int64_t>::max() - 1),
              std::numeric_limits<int64_t>::max() - 1);
    EXPECT_EQ(RoundtripInt(std::numeric_limits<int64_t>::min() + 1),
              std::numeric_limits<int64_t>::min() + 1);
}

} // namespace
