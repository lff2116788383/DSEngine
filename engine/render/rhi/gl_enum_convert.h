/**
 * @file gl_enum_convert.h
 * @brief RHI 枚举 → OpenGL 常量的映射工具，供 GL 后端内部使用
 *
 * 将 RHI 无关枚举（BlendFactor/CompareFunc/CullFace）转换为对应的 OpenGL 常量。
 * 仅限 GL 后端内部使用，Vulkan 后端无需此文件。
 */

#ifndef DSE_RENDER_GL_ENUM_CONVERT_H
#define DSE_RENDER_GL_ENUM_CONVERT_H

#include "engine/render/rhi/rhi_types.h"
#include <cstdint>

/// OpenGL 常量值（内联命名空间，避免与 GLAD 头文件的宏冲突）
namespace GLConst {
constexpr uint32_t ZERO                  = 0x0000;
constexpr uint32_t ONE                   = 0x0001;
constexpr uint32_t SRC_ALPHA             = 0x0302;
constexpr uint32_t ONE_MINUS_SRC_ALPHA   = 0x0303;
constexpr uint32_t DST_ALPHA             = 0x0304;
constexpr uint32_t ONE_MINUS_DST_ALPHA   = 0x0305;
constexpr uint32_t SRC_COLOR             = 0x0300;
constexpr uint32_t ONE_MINUS_SRC_COLOR   = 0x0301;
constexpr uint32_t DST_COLOR             = 0x0306;
constexpr uint32_t ONE_MINUS_DST_COLOR   = 0x0307;

constexpr uint32_t NEVER                 = 0x0200;
constexpr uint32_t LESS                  = 0x0201;
constexpr uint32_t EQUAL                 = 0x0202;
constexpr uint32_t LEQUAL                = 0x0203;
constexpr uint32_t GREATER               = 0x0204;
constexpr uint32_t NOTEQUAL              = 0x0205;
constexpr uint32_t GEQUAL                = 0x0206;
constexpr uint32_t ALWAYS                = 0x0207;

constexpr uint32_t FRONT                 = 0x0404;
constexpr uint32_t BACK                  = 0x0405;
constexpr uint32_t FRONT_AND_BACK        = 0x0408;
} // namespace GLConst

namespace dse {
namespace render {

/// 将 RHI BlendFactor 转换为 OpenGL 常量
inline uint32_t ToGLBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:              return GLConst::ZERO;
        case BlendFactor::One:               return GLConst::ONE;
        case BlendFactor::SrcAlpha:          return GLConst::SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:  return GLConst::ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:          return GLConst::DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:  return GLConst::ONE_MINUS_DST_ALPHA;
        case BlendFactor::SrcColor:          return GLConst::SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:  return GLConst::ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:          return GLConst::DST_COLOR;
        case BlendFactor::OneMinusDstColor:  return GLConst::ONE_MINUS_DST_COLOR;
    }
    return GLConst::ONE; // fallback
}

/// 将 RHI CompareFunc 转换为 OpenGL 常量
inline uint32_t ToGLCompareFunc(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return GLConst::NEVER;
        case CompareFunc::Less:         return GLConst::LESS;
        case CompareFunc::Equal:        return GLConst::EQUAL;
        case CompareFunc::LessEqual:    return GLConst::LEQUAL;
        case CompareFunc::Greater:      return GLConst::GREATER;
        case CompareFunc::NotEqual:     return GLConst::NOTEQUAL;
        case CompareFunc::GreaterEqual: return GLConst::GEQUAL;
        case CompareFunc::Always:       return GLConst::ALWAYS;
    }
    return GLConst::LESS; // fallback
}

/// 将 RHI CullFace 转换为 OpenGL 常量
inline uint32_t ToGLCullFace(CullFace face) {
    switch (face) {
        case CullFace::None:          return 0; // 不剔除
        case CullFace::Front:         return GLConst::FRONT;
        case CullFace::Back:          return GLConst::BACK;
        case CullFace::FrontAndBack:  return GLConst::FRONT_AND_BACK;
    }
    return GLConst::BACK; // fallback
}

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_ENUM_CONVERT_H
