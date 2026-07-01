/**
 * @file index.d.ts
 * @brief DSEngine NAPI 模块 TypeScript 类型声明
 *
 * 对应 engine/platform/harmony/napi_bridge.cpp 导出的函数。
 * ArkTS 侧通过 `import napi from 'libdse_napi.so'` 引用。
 */

/**
 * 通知引擎恢复帧循环
 * 由 EntryAbility.onForeground() 调用
 */
export const onResume: () => void;

/**
 * 通知引擎暂停帧循环
 * 由 EntryAbility.onBackground() 调用
 */
export const onPause: () => void;

/**
 * 通知引擎内存压力等级变化
 * 由 EntryAbility.onMemoryLevel() 调用
 *
 * @param level 内存压力等级
 *   - 0: MEMORY_LEVEL_MODERATE  — 中等压力
 *   - 1: MEMORY_LEVEL_LOW       — 低内存
 *   - 2: MEMORY_LEVEL_CRITICAL  — 极低内存
 */
export const onMemoryLevel: (level: number) => void;
