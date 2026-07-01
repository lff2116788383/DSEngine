/**
 * @file EntryAbility.ets
 * @brief DSEngine HarmonyOS 应用入口 — UIAbility 生命周期管理
 *
 * UIAbility 是鸿蒙应用的核心生命周期单元，等价于 Android 的 Activity。
 * 负责：
 *   1. 创建窗口并加载 GamePage（含 XComponent 渲染表面）
 *   2. 前后台切换时通知引擎暂停/恢复帧循环
 *   3. 内存压力回调时通知引擎释放缓存资源
 */

import { UIAbility, AbilityConstant, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG: string = 'DSEngine';
const DOMAIN: number = 0xFF00;

// NAPI 原生模块（对应 engine/platform/harmony/napi_bridge.cpp）
import napi from 'libdse_napi.so';

export default class EntryAbility extends UIAbility {
  private windowStage_: window.WindowStage | null = null;

  /**
   * Ability 创建回调
   * 此时尚无窗口，仅做初始化准备
   */
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onCreate');
  }

  /**
   * 窗口舞台创建回调
   * 创建主窗口并加载 GamePage（XComponent 渲染表面）
   */
  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowStageCreate');
    this.windowStage_ = windowStage;

    // 设置全屏沉浸式（游戏引擎标准配置）
    windowStage.getMainWindow().then((win: window.Window) => {
      win.setWindowLayoutFullScreen(true);
      // 隐藏状态栏和导航栏
      win.setWindowSystemBarEnable([]);
    }).catch((err: Error) => {
      hilog.error(DOMAIN, TAG, 'Failed to set fullscreen: %{public}s', err.message);
    });

    // 加载游戏页面（含 XComponent）
    windowStage.loadContent('pages/GamePage', (err) => {
      if (err) {
        hilog.error(DOMAIN, TAG, 'Failed to load GamePage: %{public}s',
                    JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, TAG, 'GamePage loaded successfully');
    });
  }

  /**
   * 应用进入前台
   * 通知引擎恢复帧循环、重新获取 GPU 资源
   */
  onForeground(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onForeground');
    try {
      napi.onResume();
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'napi.onResume failed: %{public}s', String(e));
    }
  }

  /**
   * 应用进入后台
   * 通知引擎暂停帧循环、释放非必要 GPU 资源以节省电量
   */
  onBackground(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onBackground');
    try {
      napi.onPause();
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'napi.onPause failed: %{public}s', String(e));
    }
  }

  /**
   * 内存压力回调
   * 通知引擎按压力等级释放缓存（纹理、缓冲区等）
   *
   * level 值：
   *   MEMORY_LEVEL_MODERATE  — 中等压力，建议释放非关键缓存
   *   MEMORY_LEVEL_LOW       — 低内存，应释放所有可释放资源
   *   MEMORY_LEVEL_CRITICAL  — 极低内存，系统即将 OOM kill
   */
  onMemoryLevel(level: AbilityConstant.MemoryLevel): void {
    hilog.warn(DOMAIN, TAG, 'EntryAbility onMemoryLevel: %{public}d', level);
    try {
      napi.onMemoryLevel(level);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'napi.onMemoryLevel failed: %{public}s', String(e));
    }
  }

  /**
   * 窗口舞台销毁回调
   * 此时渲染表面即将释放
   */
  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowStageDestroy');
    this.windowStage_ = null;
  }

  /**
   * Ability 销毁回调
   */
  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onDestroy');
  }
}
