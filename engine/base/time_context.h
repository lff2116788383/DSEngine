/**
 * @file time_context.h
 * @brief 每帧时间上下文：显式携带缩放 / 真实两条时间通道，贯穿 update graph。
 *
 * 见 docs/design/TIME_SCALE.md。gameplay / 动画 / 粒子 / Tween / 物理累加器使用
 * scaled_dt；UI / 暂停菜单 / 输入 / 统计使用 unscaled_dt（scale=0 时仍前进）。
 */

#ifndef DSE_ENGINE_BASE_TIME_CONTEXT_H
#define DSE_ENGINE_BASE_TIME_CONTEXT_H

namespace dse {

struct TimeContext {
    float scaled_dt   = 0.0f;  ///< 全局缩放后增量时间（gameplay/anim/particle/tween/物理累加）
    float unscaled_dt = 0.0f;  ///< 真实增量时间（UI/暂停菜单/输入/统计）
    float time_scale  = 1.0f;  ///< 全局时间缩放（0=暂停, 1=正常）
};

} // namespace dse

#endif // DSE_ENGINE_BASE_TIME_CONTEXT_H
