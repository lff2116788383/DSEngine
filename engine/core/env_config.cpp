/**
 * @file env_config.cpp
 * @brief DSE_* 环境变量集中读取与自描述清单实现
 */

#include "engine/core/env_config.h"
#include "engine/base/debug.h"

#include <cstdlib>
#include <cstring>
#include <cerrno>

namespace dse {
namespace core {
namespace env {

const std::vector<EnvVarDesc>& KnownEnvVars() {
    static const std::vector<EnvVarDesc> kVars = {
        {names::kRuntimeModules,      "逗号分隔的运行时模块 DLL 名称列表"},
        {names::kDataRoot,            "资源/数据根目录覆盖"},
        {names::kMaxFrames,           "运行指定帧数后自动退出（自动化/CI）"},
        {names::kScreenshotFrame,     "在指定帧截图（自动化/CI）"},
        {names::kStartupSceneRegress, "启用启动场景回归检查"},
        {names::kStartupLua,          "启动时执行的 Lua 脚本路径"},
        {names::kLogLevel,            "日志级别（trace/info/warn/error）"},
        {names::kCrashHandler,        "设为 0 关闭崩溃处理器"},
        {names::kCrashDir,            "崩溃转储输出目录"},
        {names::kSplash,              "设为 0 关闭启动 splash"},
        {names::kSplashDebug,         "打印 splash 调试信息"},
        {names::kRhiBackend,          "RHI 后端：opengl/d3d11/vulkan"},
        {names::kRenderThread,        "设为 1 启用独立渲染线程"},
        {names::kRenderScale,         "渲染分辨率缩放系数"},
        {names::kVsync,               "垂直同步开关"},
        {names::kForceSdr,            "强制 SDR 输出（禁用 HDR）"},
        {names::kDisableGpuDriven,    "（旧）非 0 关闭 GPU-Driven"},
        {names::kGpuDrivenPolicy,     "GPU-Driven 策略：off/auto/force"},
        {names::kGpuDrivenDiag,       "打印 GPU-Driven 诊断"},
        {names::kAsyncUploadBudget,   "异步上传预算（字节/帧）"},
        {names::kDisableHiz,          "禁用 Hi-Z 遮挡剔除"},
        {names::kPassDiag,            "打印渲染 Pass 诊断"},
        {names::kRenderReadbackDiag,  "打印渲染回读诊断"},
        {names::kVulkanMaxPasses,     "Vulkan GPU-Driven 最大 Pass 数"},
        {names::kRenderPipeProfile,   "渲染管线剖面名称"},
        {names::kRenderPipeProfPath,  "渲染管线剖面文件路径"},
    };
    return kVars;
}

const char* GetRaw(const char* name) {
    if (name == nullptr) return nullptr;
    const char* v = std::getenv(name);
    if (v == nullptr || v[0] == '\0') return nullptr;
    return v;
}

bool IsSet(const char* name) {
    return GetRaw(name) != nullptr;
}

std::string GetString(const char* name, const std::string& default_value) {
    const char* v = GetRaw(name);
    return v ? std::string(v) : default_value;
}

bool GetBool(const char* name, bool default_value) {
    const char* v = GetRaw(name);
    if (v == nullptr) return default_value;
    const char c = v[0];
    // "0"/"false"/"no"/"off" 视为 false，其余非空视为 true
    if (c == '0' || c == 'f' || c == 'F' || c == 'n' || c == 'N') return false;
    return true;
}

int GetInt(const char* name, int default_value) {
    const char* v = GetRaw(name);
    if (v == nullptr) return default_value;
    errno = 0;
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    if (end == v || errno != 0) return default_value;
    return static_cast<int>(parsed);
}

float GetFloat(const char* name, float default_value) {
    const char* v = GetRaw(name);
    if (v == nullptr) return default_value;
    errno = 0;
    char* end = nullptr;
    float parsed = std::strtof(v, &end);
    if (end == v || errno != 0) return default_value;
    return parsed;
}

void DumpKnownEnvVars() {
    DEBUG_LOG_INFO("[env] 已知 DSE_* 环境变量当前取值:");
    for (const auto& desc : KnownEnvVars()) {
        const char* v = GetRaw(desc.name);
        DEBUG_LOG_INFO("[env]   {} = {}  ({})",
                       desc.name,
                       v ? v : "<unset>",
                       desc.description);
    }
}

} // namespace env
} // namespace core
} // namespace dse
