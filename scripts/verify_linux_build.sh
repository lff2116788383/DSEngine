#!/usr/bin/env bash
# =============================================================================
# verify_linux_build.sh — Linux(桌面/WSL) 端到端构建验证
#
# 配置 CMake(GLFW X11 + GL 桌面后端) → 构建 dse_engine 静态库与 Lua 运行时
# 可执行文件 → 校验产物为合法 ELF。用于 CI 或本地确认引擎在 Linux 可构建。
#
# 依赖：cmake / ninja / g++(或 clang) + X11/GL 开发库
#   (libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev)
#
# 用法：
#   scripts/verify_linux_build.sh                 # 默认 Debug，构建目录 ~/dse_build_linux
#   BUILD_DIR=/tmp/b BUILD_TYPE=Release scripts/verify_linux_build.sh
# 环境变量：BUILD_DIR / BUILD_TYPE / JOBS
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$HOME/dse_build_linux}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

c_cyan="\033[36m"; c_green="\033[32m"; c_red="\033[31m"; c_rst="\033[0m"
step() { echo -e "\n${c_cyan}>> $*${c_rst}"; }
ok()   { echo -e "   ${c_green}[OK]${c_rst} $*"; }
die()  { echo -e "   ${c_red}[FAIL]${c_rst} $*" >&2; exit 1; }

# ── 1. 平台与工具链检查 ──────────────────────────────────────────────────────
step "检查平台与工具链"
case "$(uname -s)" in
    Linux) : ;;
    *) die "本脚本仅用于 Linux/WSL（当前 $(uname -s)）。" ;;
esac

command -v cmake >/dev/null 2>&1 || die "未找到 cmake（apt install cmake）。"
NINJA_FLAG=""
GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then GENERATOR="Ninja"; fi
command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1 \
    || die "未找到 C++ 编译器（apt install build-essential）。"

# X11/GL 开发库（GLFW 桌面后端需要）；缺失给出明确安装提示
missing=""
for pkg in /usr/include/X11/Xlib.h /usr/include/GL/gl.h; do
    [ -e "$pkg" ] || missing="$missing $pkg"
done
if [ -n "$missing" ]; then
    die "缺少 X11/GL 开发头文件($missing)。请安装：sudo apt-get install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev"
fi
ok "cmake=$(cmake --version | head -1 | awk '{print $3}')  generator=$GENERATOR  jobs=$JOBS"
ok "源码目录=$SRC_DIR"

# ── 2. 配置 ──────────────────────────────────────────────────────────────────
step "配置 CMake ($BUILD_TYPE)"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DDSE_BUILD_SHARED=OFF \
    -DDSE_BUILD_GTESTS=OFF \
    -DDSE_ENABLE_3D=ON \
    -DDSE_ENABLE_JOLT=ON \
    -DDSE_ENABLE_PHYSX=OFF \
    -DDSE_ENABLE_D3D11=OFF \
    -DDSE_ENABLE_VULKAN=OFF \
    || die "CMake 配置失败。"
ok "配置完成：$BUILD_DIR"

# shader_compiler 目标输出到共享的 ${SRC}/bin/。若该机器也做过其它架构(如 Android
# arm64)的构建，bin/dse_shader_compiler 可能是异构二进制，ninja 会因时间戳较新而跳过
# 重建，导致本机执行时报 "Exec format error"。这里检测并删除异构产物，强制重建本机版本。
step "检查 host shader 编译器架构"
HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
    x86_64|amd64) ARCH_TOKEN="x86-64" ;;
    aarch64|arm64) ARCH_TOKEN="aarch64" ;;
    *) ARCH_TOKEN="$HOST_ARCH" ;;
esac
SHADER_BIN="$SRC_DIR/bin/dse_shader_compiler"
if [ -f "$SHADER_BIN" ] && command -v file >/dev/null 2>&1; then
    if ! file -b "$SHADER_BIN" | grep -qi "$ARCH_TOKEN"; then
        echo "   bin/dse_shader_compiler 非本机架构($ARCH_TOKEN)，删除以重建"
        rm -f "$SHADER_BIN"
    fi
fi
ok "shader 编译器架构检查完成"

# ── 3. 构建 ──────────────────────────────────────────────────────────────────
step "构建 dse_engine + Lua 运行时"
cmake --build "$BUILD_DIR" --target dse_engine -j "$JOBS" || die "构建 dse_engine 失败。"
cmake --build "$BUILD_DIR" --target dse_example_lua -j "$JOBS" || die "构建 dse_example_lua 失败。"

# ── 4. 校验产物 ──────────────────────────────────────────────────────────────
step "校验产物"
ENGINE_LIB="$(find "$BUILD_DIR" -maxdepth 2 -name 'libDSEngine*.a' | head -1)"
[ -n "$ENGINE_LIB" ] && [ -f "$ENGINE_LIB" ] || die "未找到引擎静态库 libDSEngine*.a。"
ok "引擎静态库: $ENGINE_LIB ($(du -h "$ENGINE_LIB" | cut -f1))"

# Lua 运行时可执行文件输出到源码 bin/（RUNTIME_OUTPUT_DIRECTORY=${SRC}/bin），兼查构建目录
LUA_EXE="$(find "$SRC_DIR/bin" "$BUILD_DIR" -maxdepth 3 -type f -name 'DSEngine_lua*' ! -name '*.a' ! -name '*.pdb' 2>/dev/null | head -1)"
[ -n "$LUA_EXE" ] && [ -f "$LUA_EXE" ] || die "未找到 Lua 运行时可执行文件 DSEngine_lua*。"
if command -v file >/dev/null 2>&1; then
    file "$LUA_EXE" | grep -q "ELF" || die "Lua 运行时不是合法 ELF：$LUA_EXE"
fi
ok "Lua 运行时可执行文件: $LUA_EXE"

echo -e "\n${c_cyan}==================== RESULT ====================${c_rst}"
ok "Linux 构建验证全部通过 ($BUILD_TYPE)"
echo -e "   引擎库: $ENGINE_LIB"
echo -e "   可执行: $LUA_EXE"
exit 0
