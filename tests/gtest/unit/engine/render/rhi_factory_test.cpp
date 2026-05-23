/**
 * @file rhi_factory_test.cpp
 * @brief RHI 工厂纯逻辑函数测试（BackendToString / ValidateBackend）
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_factory.h"

using namespace dse::render;

TEST(RhiFactoryTest, BackendToStringOpenGL) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::OpenGL), "OpenGL");
}

TEST(RhiFactoryTest, BackendToStringVulkan) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::Vulkan), "Vulkan");
}

TEST(RhiFactoryTest, BackendToStringD3D11) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::D3D11), "D3D11");
}

TEST(RhiFactoryTest, BackendToStringDefault) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::Default), "OpenGL");
}

TEST(RhiFactoryTest, ValidateOpenGLAlwaysAvailable) {
    EXPECT_EQ(ValidateRhiBackend(RhiBackend::OpenGL), RhiBackend::OpenGL);
}

TEST(RhiFactoryTest, ValidateDefaultPassesThrough) {
    EXPECT_EQ(ValidateRhiBackend(RhiBackend::Default), RhiBackend::Default);
}

TEST(RhiFactoryTest, ValidateVulkanFallback) {
    auto result = ValidateRhiBackend(RhiBackend::Vulkan);
#ifdef DSE_ENABLE_VULKAN
    EXPECT_EQ(result, RhiBackend::Vulkan);
#else
    // 未启用 Vulkan，应回退到 D3D11 或 OpenGL
    EXPECT_TRUE(result == RhiBackend::D3D11 || result == RhiBackend::OpenGL);
#endif
}

TEST(RhiFactoryTest, ValidateD3D11Fallback) {
    auto result = ValidateRhiBackend(RhiBackend::D3D11);
#ifdef DSE_ENABLE_D3D11
    EXPECT_EQ(result, RhiBackend::D3D11);
#else
    EXPECT_EQ(result, RhiBackend::OpenGL);
#endif
}

TEST(RhiFactoryTest, BackendEnumValues) {
    EXPECT_EQ(static_cast<unsigned int>(RhiBackend::OpenGL), 0u);
    EXPECT_EQ(static_cast<unsigned int>(RhiBackend::Vulkan), 1u);
    EXPECT_EQ(static_cast<unsigned int>(RhiBackend::D3D11), 2u);
}
