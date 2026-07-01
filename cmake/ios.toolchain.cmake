# =============================================================================
# ios.toolchain.cmake — iOS 交叉编译工具链
#
# 用法：
#   cmake -B build-ios -G Xcode \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
#       -DDSE_ENABLE_VULKAN=ON \
#       -DDSE_ENABLE_APPLE_PLATFORM=ON \
#       -DDSE_ENABLE_D3D11=OFF
#
# 前置依赖：
#   - Xcode 15+ (含 iOS SDK)
#   - Vulkan SDK for macOS (含 MoltenVK)
# =============================================================================

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum iOS deployment target")

# Xcode 自动签名（开发阶段）
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer" CACHE STRING "iOS code signing identity")
set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "" CACHE STRING "Apple Developer Team ID")

# 确保使用正确的 SDK
set(CMAKE_XCODE_ATTRIBUTE_SDKROOT "iphoneos")

# 启用 ARC（Objective-C++ 源文件）
set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)

# Bitcode 已被 Apple 废弃（Xcode 14+），显式关闭
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE NO)
