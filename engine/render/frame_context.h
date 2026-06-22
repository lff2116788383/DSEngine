/**
 * @file frame_context.h
 * @brief 帧/场景级相机上下文
 *
 * 取代旧「相机矩阵缓存在 CommandBuffer 上」的耦合（见 RHI_ABSTRACTION_BOUNDARY §8.2 D6）：
 * 相机属于帧/场景概念，由各 Pass 构造后显式传递给消费者（render_scene / 模块渲染系统 /
 * 探针捕获），而非挂在命令记录器（CommandBuffer）上经 GetViewMatrix/GetProjectionMatrix 取回。
 * view/projection 均已含投影修正（GetProjectionCorrection），与绘制执行器同源。
 */

#ifndef DSE_RENDER_FRAME_CONTEXT_H
#define DSE_RENDER_FRAME_CONTEXT_H

#include <glm/glm.hpp>

namespace dse {
namespace render {

/// 一帧绘制所用的相机矩阵载体（值类型，按 const& 传递）。
struct FrameContext {
    glm::mat4 view = glm::mat4(1.0f);        ///< 视图矩阵（Camera-Relative 时相机在原点）
    glm::mat4 projection = glm::mat4(1.0f);  ///< 投影矩阵（含 clip correction）
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_FRAME_CONTEXT_H
