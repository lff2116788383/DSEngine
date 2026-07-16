/**
 * @file env_config.h
 * @brief 集中管理 DSE_* 环境变量：统一的名称常量、类型化读取与自描述清单
 *
 * 背景：引擎/编辑器中的环境变量读取原本散落在各处（frame_pipeline、rhi、
 * splash、debug…），新增变量既无统一注册也无文档，排查问题时难以知道有哪些开关。
 *
 * 本模块提供：
 * - 全部已知 DSE_* 变量的名称常量（kEnv* / 见 EnvVars 命名空间），单一事实来源；
 * - 类型化读取器（GetString/GetBool/GetInt/GetFloat/IsSet），语义统一；
 * - 自描述清单 KnownEnvVars() + DumpKnownEnvVars()，可一处枚举全部开关及说明。
 *
 * 语义约定：
 * - GetBool：非空且首字符不是 '0'/'f'/'F'/'n'/'N' 视为 true；"", "0", "false",
 *   "no" 等视为 false。空指针（未设置）返回 default_value。
 */

#ifndef DSE_CORE_ENV_CONFIG_H
#define DSE_CORE_ENV_CONFIG_H

#include <string>
#include <vector>

namespace dse {
namespace core {
namespace env {

/// 已知 DSE_* 环境变量名称常量（单一事实来源）。新增变量请同时在此登记并补充 KnownEnvVars()。
namespace names {
// 运行时 / 引擎
inline constexpr const char* kRuntimeModules      = "DSE_RUNTIME_MODULES";
inline constexpr const char* kDataRoot            = "DSE_DATA_ROOT";
inline constexpr const char* kMaxFrames           = "DSE_MAX_FRAMES";
inline constexpr const char* kScreenshotFrame     = "DSE_SCREENSHOT_FRAME";
inline constexpr const char* kStartupSceneRegress = "DSE_ENABLE_STARTUP_SCENE_REGRESSION";
inline constexpr const char* kStartupLua          = "DSE_STARTUP_LUA";
inline constexpr const char* kLogLevel            = "DSE_LOG_LEVEL";
// 崩溃处理
inline constexpr const char* kCrashHandler        = "DSE_CRASH_HANDLER";
inline constexpr const char* kCrashDir            = "DSE_CRASH_DIR";
// Splash
inline constexpr const char* kSplash              = "DSE_SPLASH";
inline constexpr const char* kSplashDebug         = "DSE_SPLASH_DEBUG";
// 渲染 / RHI
inline constexpr const char* kRhiBackend          = "DSE_RHI_BACKEND";
inline constexpr const char* kRenderThread        = "DSE_RENDER_THREAD";
inline constexpr const char* kRenderScale         = "DSE_RENDER_SCALE";
inline constexpr const char* kVsync               = "DSE_VSYNC";
inline constexpr const char* kForceSdr            = "DSE_FORCE_SDR";
inline constexpr const char* kDisableGpuDriven    = "DSE_DISABLE_GPU_DRIVEN";
inline constexpr const char* kGpuDrivenPolicy     = "DSE_GPU_DRIVEN_POLICY";
inline constexpr const char* kGpuDrivenDiag       = "DSE_GPU_DRIVEN_DIAG";
inline constexpr const char* kAsyncUploadBudget   = "DSE_ASYNC_UPLOAD_BUDGET";
inline constexpr const char* kDisableHiz          = "DSE_DISABLE_HIZ";
inline constexpr const char* kPassDiag            = "DSE_PASS_DIAG";
inline constexpr const char* kRenderReadbackDiag  = "DSE_RENDER_READBACK_DIAG";
inline constexpr const char* kVulkanMaxPasses     = "DSE_VULKAN_MAX_PASSES";
inline constexpr const char* kRenderPipeProfile   = "DSE_RENDER_PIPELINE_PROFILE";
inline constexpr const char* kRenderPipeProfPath  = "DSE_RENDER_PIPELINE_PROFILE_PATH";
} // namespace names

/// 变量描述：用于自描述清单与诊断输出。
struct EnvVarDesc {
    const char* name;         ///< 变量名
    const char* description;  ///< 用途说明
};

/// 返回全部已知变量的自描述清单（供 UI/诊断枚举）。
const std::vector<EnvVarDesc>& KnownEnvVars();

/// 是否已设置（存在且非空字符串）。
bool IsSet(const char* name);

/// 读取字符串；未设置返回 default_value。
std::string GetString(const char* name, const std::string& default_value = "");

/// 读取原始 C 字符串指针（未设置返回 nullptr）；等价于 std::getenv，但集中收口。
const char* GetRaw(const char* name);

/// 读取布尔；未设置返回 default_value。见文件头语义约定。
bool GetBool(const char* name, bool default_value = false);

/// 读取整数；未设置或解析失败返回 default_value。
int GetInt(const char* name, int default_value = 0);

/// 读取浮点；未设置或解析失败返回 default_value。
float GetFloat(const char* name, float default_value = 0.0f);

/// 把全部已知变量及当前取值输出到日志（诊断用）。
void DumpKnownEnvVars();

} // namespace env
} // namespace core
} // namespace dse

#endif // DSE_CORE_ENV_CONFIG_H
