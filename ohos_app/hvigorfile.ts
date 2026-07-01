/**
 * DSEngine HarmonyOS 项目级 hvigor 构建脚本
 *
 * hvigor 是 HarmonyOS 的构建工具（等价于 Android 的 Gradle）。
 * 此文件定义项目级构建插件。
 */

import { appTasks } from '@ohos/hvigor-ohos-plugin';

export default {
  system: appTasks,
  plugins: []
}
