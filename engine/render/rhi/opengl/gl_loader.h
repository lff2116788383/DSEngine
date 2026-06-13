/**
 * @file gl_loader.h
 * @brief OpenGL / OpenGL ES 统一包含头
 *
 * 桌面端:  glad（OpenGL 4.3+）
 * Android:  GLES3 / gl31（OpenGL ES 3.1）
 * Web/WASM: GLES3 / gl3 （OpenGL ES 3.0 = WebGL2，由 Emscripten 静态提供）
 */

#ifndef DSE_RENDER_RHI_OPENGL_GL_LOADER_H
#define DSE_RENDER_RHI_OPENGL_GL_LOADER_H

#if defined(__ANDROID__)
#   include <GLES3/gl31.h>
#   include <GLES3/gl3ext.h>
#   ifndef GLAD_API_PTR
#       define GLAD_API_PTR
#   endif
// 桌面 GL → GLES3 兼容垫片：GLES 仅提供 glClearDepthf，将 glClearDepth(double) 映射过去。
#   ifndef glClearDepth
#       define glClearDepth(d) glClearDepthf(static_cast<GLfloat>(d))
#   endif
#elif defined(__EMSCRIPTEN__)
// WebGL2 = OpenGL ES 3.0：无 Compute / SSBO / 间接绘制，相关能力由 RHI
// SupportsCompute()/SupportsSSBO()/SupportsIndirectDraw() 在运行时门控（见 gl_rhi_device）。
#   include <GLES3/gl3.h>
#   include <GLES3/gl2ext.h>
#   ifndef GLAD_API_PTR
#       define GLAD_API_PTR
#   endif
#   ifndef glClearDepth
#       define glClearDepth(d) glClearDepthf(static_cast<GLfloat>(d))
#   endif
#else
#   include <glad/gl.h>
#endif

// GLES 运行时（Android GLES3.1 / Web WebGL2=GLES3.0）：相较桌面 GL 缺少 timestamp 查询、
// glMultiDraw*、glPolygonMode / GL_LINE_SMOOTH 等桌面专属能力。GL RHI 后端以本宏统一门控
// 这些编译期不可用的符号，避免在多处散落 __ANDROID__/__EMSCRIPTEN__ 平台宏。
// 注意：运行时特性开关仍走 RHI 能力查询（SupportsCompute()/SupportsSSBO()/SupportsIndirectDraw()），
// 本宏仅用于回避桌面专属 GL 符号在 GLES 头文件中“根本不存在”导致的编译错误。
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#   define DSE_GL_ES_RUNTIME 1
#else
#   define DSE_GL_ES_RUNTIME 0
#endif

#endif // DSE_RENDER_RHI_OPENGL_GL_LOADER_H
