/**
 * @file gl_loader.h
 * @brief OpenGL / OpenGL ES 统一包含头
 *
 * 桌面端: glad（OpenGL 4.3+）
 * Android: GLES3 / gl31（OpenGL ES 3.1）
 */

#ifndef DSE_RENDER_RHI_OPENGL_GL_LOADER_H
#define DSE_RENDER_RHI_OPENGL_GL_LOADER_H

#ifdef __ANDROID__
#   include <GLES3/gl31.h>
#   include <GLES3/gl3ext.h>
#else
#   include <glad/gl.h>
#endif

#endif // DSE_RENDER_RHI_OPENGL_GL_LOADER_H
