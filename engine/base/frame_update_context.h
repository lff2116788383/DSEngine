/**
 * @file frame_update_context.h
 * @brief 每帧逻辑更新上下文：统一承载贯穿 update graph 的逐帧状态。
 *
 * 设计目标（见 docs/design/TIME_SCALE.md、docs/architecture/ARCHITECTURE.md）：
 * 把"需要穿过整条 update graph 的逐帧状态"聚合到单一可扩展载体。未来新增此类
 * 贯穿式状态（如插值 alpha、暂停标志、关卡时间等）只需在本结构体增加字段，
 * 无需再逐层修改 Update / OnUpdate 的签名——这正是 time_scale 落地时暴露的
 * "贯穿式特性需改多层签名"成本的长期解法。
 *
 * 与 dse::render::FrameContext（相机矩阵，渲染期载体）区分：本结构服务于逻辑更新期，
 * 由 EngineInstance::Tick 构造后显式传入 FramePipeline::Update → update graph → 各模块。
 */

#ifndef DSE_ENGINE_BASE_FRAME_UPDATE_CONTEXT_H
#define DSE_ENGINE_BASE_FRAME_UPDATE_CONTEXT_H

#include <cstdint>

#include "engine/base/time_context.h"

namespace dse {

struct FrameUpdateContext {
    TimeContext   time;             ///< 时间通道（scaled / unscaled / time_scale）
    std::uint64_t frame_index = 0;  ///< 单调递增帧序号（首帧 = 0）
};

} // namespace dse

#endif // DSE_ENGINE_BASE_FRAME_UPDATE_CONTEXT_H
