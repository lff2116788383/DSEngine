#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# DSEngine 网络层（GNS）Android arm64-v8a 交叉编译验证脚本  —— Phase 3
#
# 在 Git bash（Windows 主机 + NDK windows-x86_64 工具链）下，一键完成：
#   1) 交叉编译 arm64 OpenSSL 1.1.1w（静态 libcrypto.a / libssl.a）
#   2) 交叉编译 arm64 protobuf v3.21.12 运行时（install + protobuf-config.cmake）
#   3) 用 NDK 工具链配置引擎（DSE_ENABLE_NET=ON）并构建 dse_net_smoke（arm64 可执行）
# 里程碑口径：编译 + 链接通过即视为通过（真机运行留作后续，与现有 Android 验收一致）。
#
# 依赖（可用环境变量覆盖默认值）：
#   ANDROID_NDK   NDK 根目录            默认 /c/Android/android-ndk-r26d
#   OPENSSL_SRC   OpenSSL 1.1.1w 源码   默认 /c/Android/openssl-1.1.1w
#   HOST_PROTOC   主机 protoc.exe       默认 …/dse_net_deps/protobuf/bin/protoc.exe
#                                       （须与在树 depends/protobuf 版本一致）
#   HOST_SHADERC  主机着色器编译器       默认 <repo>/bin/dse_shader_compiler.exe
#   ANDROID_API   API level             默认 24
# 产物目录（仓库外，临时）：
#   OSSL_OUT      arm64 OpenSSL 安装     默认 /c/Android/ossl-android-arm64
#   PB_OUT        arm64 protobuf 安装    默认 /c/Android/protobuf-android-arm64
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ANDROID_NDK="${ANDROID_NDK:-/c/Android/android-ndk-r26d}"
OPENSSL_SRC="${OPENSSL_SRC:-/c/Android/openssl-1.1.1w}"
HOST_PROTOC="${HOST_PROTOC:-/c/Users/Administrator/dse_net_deps/protobuf/bin/protoc.exe}"
HOST_SHADERC="${HOST_SHADERC:-$REPO_DIR/bin/dse_shader_compiler.exe}"
ANDROID_API="${ANDROID_API:-24}"
OSSL_OUT="${OSSL_OUT:-/c/Android/ossl-android-arm64}"
PB_OUT="${PB_OUT:-/c/Android/protobuf-android-arm64}"
BUILD_DIR="${BUILD_DIR:-$REPO_DIR/build_android_net}"

NDK_BIN="$ANDROID_NDK/toolchains/llvm/prebuilt/windows-x86_64/bin"
[ -d "$NDK_BIN" ] || NDK_BIN="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"

echo "== DSEngine Android net build =="
echo "   repo        : $REPO_DIR"
echo "   NDK         : $ANDROID_NDK"
echo "   host protoc : $HOST_PROTOC"
echo "   OpenSSL out : $OSSL_OUT"
echo "   protobuf out: $PB_OUT"

command -v cmake >/dev/null || { echo "!! 需要 cmake 在 PATH"; exit 1; }
command -v ninja >/dev/null || { echo "!! 需要 ninja 在 PATH（choco install ninja）"; exit 1; }
[ -d "$NDK_BIN" ] || { echo "!! NDK 工具链目录不存在：$NDK_BIN"; exit 1; }
[ -f "$HOST_PROTOC" ] || { echo "!! 主机 protoc 不存在：$HOST_PROTOC"; exit 1; }

# ── 1) arm64 OpenSSL（静态）──────────────────────────────────────────────────
if [ -f "$OSSL_OUT/lib/libcrypto.a" ] && [ -f "$OSSL_OUT/lib/libssl.a" ]; then
    echo "-- [1/3] OpenSSL 已存在，跳过：$OSSL_OUT"
else
    echo "-- [1/3] 交叉编译 arm64 OpenSSL → $OSSL_OUT"
    [ -d "$OPENSSL_SRC" ] || { echo "!! OpenSSL 源码不存在：$OPENSSL_SRC"; exit 1; }
    # Git 自带的精简 perl 缺 Pod::Usage（OpenSSL configdata.pm 顶层 use 之，编译期必触发）。
    # 在源码树（'.' 在 @INC）放一个最小桩，让 use 成功；不影响构建产物。
    if [ ! -f "$OPENSSL_SRC/Pod/Usage.pm" ]; then
        mkdir -p "$OPENSSL_SRC/Pod"
        cat > "$OPENSSL_SRC/Pod/Usage.pm" <<'EOF'
package Pod::Usage;
use strict; use warnings;
require Exporter; our @ISA = qw(Exporter); our @EXPORT = qw(pod2usage);
sub pod2usage { exit(0); }
1;
EOF
    fi
    export ANDROID_NDK_HOME="$ANDROID_NDK"
    export ANDROID_NDK_ROOT="$ANDROID_NDK"
    export PATH="$NDK_BIN:$PATH"
    ( cd "$OPENSSL_SRC" \
        && perl Configure android-arm64 -D__ANDROID_API__="$ANDROID_API" \
                no-shared no-tests no-engine no-asm \
                --prefix="$(cygpath -m "$OSSL_OUT" 2>/dev/null || echo "$OSSL_OUT")" \
                --openssldir="$(cygpath -m "$OSSL_OUT" 2>/dev/null || echo "$OSSL_OUT")/ssl" \
        && make -j"$(nproc)" \
        && make install_sw )
fi

# ── 2) arm64 protobuf 运行时（install + config）──────────────────────────────
if [ -f "$PB_OUT/lib/libprotobuf.a" ] && [ -f "$PB_OUT/lib/cmake/protobuf/protobuf-config.cmake" ]; then
    echo "-- [2/3] protobuf 已存在，跳过：$PB_OUT"
else
    echo "-- [2/3] 交叉编译 arm64 protobuf → $PB_OUT"
    cmake -S "$REPO_DIR/depends/protobuf" -B "$REPO_DIR/build_protobuf_android" -G Ninja \
        "-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
        "-DCMAKE_MAKE_PROGRAM=$(command -v ninja)" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM="android-$ANDROID_API" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
        -Dprotobuf_WITH_ZLIB=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX="$PB_OUT"
    cmake --build "$REPO_DIR/build_protobuf_android" --target install
fi

# ── 3) 配置 + 构建 dse_net_smoke（arm64）─────────────────────────────────────
echo "-- [3/3] 配置 + 构建 dse_net_smoke（arm64-v8a）"
rm -rf "$BUILD_DIR"
cmake -S "$REPO_DIR" -B "$BUILD_DIR" -G Ninja \
    "-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    "-DCMAKE_MAKE_PROGRAM=$(command -v ninja)" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM="android-$ANDROID_API" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_GTESTS=OFF \
    -DDSE_ENABLE_3D=OFF -DDSE_ENABLE_D3D11=OFF -DDSE_ENABLE_VULKAN=OFF -DDSE_ENABLE_PHYSX=OFF \
    -DDSE_ENABLE_NET=ON \
    -DDSE_NET_OPENSSL_ROOT="$OSSL_OUT" \
    -DDSE_NET_PROTOBUF_DIR="$PB_OUT" \
    -DDSE_HOST_PROTOC="$HOST_PROTOC" \
    -DDSE_HOST_SHADER_COMPILER="$HOST_SHADERC"
cmake --build "$BUILD_DIR" --target dse_net_smoke -j 4

SMOKE="$REPO_DIR/bin/dse_net_smoke"
if [ -f "$SMOKE" ] && file "$SMOKE" | grep -q "ARM aarch64"; then
    echo ""
    echo "== PASS：dse_net_smoke 已交叉编译为 arm64-v8a 可执行（编译+链接通过）=="
    file "$SMOKE"
else
    echo "!! FAIL：未找到 arm64 的 dse_net_smoke 可执行"
    exit 1
fi
