/**
 * @file crash_handler.h
 * @brief 进程级崩溃捕获与报告子系统。
 *
 * 设计目标（v1，本地优先、服务器无关）：
 *  - 安装全局未处理异常/信号处理器，玩家进程崩溃时在本机落地：
 *      1) 一份可读的崩溃报告 .txt（版本号 / 异常类型 / 出错地址 / 调用栈 /
 *         加载模块 / 系统信息 / 自定义元数据 / 崩溃前“面包屑”）；
 *      2) Windows 上额外写一份 minidump .dmp（可用 PDB 在 VS/WinDbg 还原到源码行）。
 *  - 上传做成“服务器无关”：可选注册一个 UploadCallback，崩溃报告写完后回调，
 *     由上层决定是自建端点（复用 dse.http）还是接 Sentry/BugSplat 等 SaaS；
 *     默认不注册 = 纯本地，零外部依赖、零运维。
 *
 * 线程安全：AddBreadcrumb / SetMetadata 可在任意线程调用（内部加锁）。
 * 处理器内尽量只做异步信号安全/最少分配的工作，符号解析为“尽力而为”。
 */

#ifndef DSE_DIAGNOSTICS_CRASH_HANDLER_H
#define DSE_DIAGNOSTICS_CRASH_HANDLER_H

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse {
namespace diagnostics {

/**
 * @brief 固定容量的“面包屑”环形缓冲：记录崩溃前最近 N 条事件/日志，
 *        随崩溃报告一并落地，提供崩溃前上下文。线程安全。
 */
class DSE_EXPORT BreadcrumbBuffer {
public:
    explicit BreadcrumbBuffer(std::size_t capacity = 64);

    /// 追加一条面包屑（超出容量时丢弃最旧一条）。
    void Push(const std::string& entry);

    /// 按时间顺序（最旧 -> 最新）返回当前快照。
    std::vector<std::string> Snapshot() const;

    /// 清空。
    void Clear();

    /// 重设容量并清空（用于重新配置；本类持有互斥量，不可拷贝/赋值）。
    void Reset(std::size_t capacity);

    std::size_t Capacity() const { return capacity_; }
    std::size_t Size() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::string> entries_;  ///< 环形存储
    std::size_t capacity_;
    std::size_t head_ = 0;   ///< 下一个写入位置
    std::size_t count_ = 0;  ///< 当前条数
};

/**
 * @brief 一次崩溃的结构化信息。平台相关的异常捕获负责填充本结构，
 *        再交给与平台无关的 FormatCrashReport / WriteCrashReport 落地。
 */
struct CrashReportInfo {
    std::string app_name = "DSEngine";
    std::string app_version;                 ///< 默认取 DSE_VERSION_STRING
    std::string timestamp_utc;               ///< ISO-8601，如 2026-06-10T10:13:00Z
    std::string reason;                      ///< 异常/信号摘要，如 "EXCEPTION_ACCESS_VIOLATION (0xC0000005)"
    std::string fault_address;               ///< 出错地址（十六进制，可空）
    std::string os_info;                     ///< 操作系统/架构摘要
    std::string dump_file;                   ///< 关联的 minidump 文件名（无则空）
    std::vector<std::string> call_stack;     ///< 调用栈（已尽力符号化）
    std::vector<std::string> modules;        ///< 加载模块列表（可空）
    std::vector<std::string> breadcrumbs;    ///< 崩溃前面包屑
    std::vector<std::pair<std::string, std::string>> metadata;  ///< 自定义键值
};

/// 将崩溃信息格式化为人类可读的报告文本（.txt 内容）。纯函数、无 OS 依赖。
DSE_EXPORT std::string FormatCrashReport(const CrashReportInfo& info);

/// 将崩溃信息格式化为紧凑 JSON（供上传作为元数据）。纯函数、无 OS 依赖。
DSE_EXPORT std::string FormatCrashReportJson(const CrashReportInfo& info);

/**
 * @brief 把崩溃报告写入目录，返回写入的 .txt 完整路径（失败返回空串）。
 * @param dir 输出目录（不存在会创建）。
 * @param info 崩溃信息。
 * 文件名形如 crash_<app>_<timestamp>_<pid>.txt。
 */
DSE_EXPORT std::string WriteCrashReport(const std::string& dir, const CrashReportInfo& info);

/// 上传回调：崩溃报告写完后被调用（尽力而为）。参数为 .txt 路径、可选 .dmp 路径、JSON 元数据。
using UploadCallback = std::function<void(const std::string& report_path,
                                          const std::string& dump_path,
                                          const std::string& meta_json)>;

struct CrashHandlerConfig {
    std::string app_name = "DSEngine";
    std::string app_version;                 ///< 空则填 DSE_VERSION_STRING
    std::string dump_dir = "crashes";        ///< 报告/dump 输出目录
    bool write_minidump = true;              ///< Windows 上是否写 .dmp
    bool full_memory_dump = false;           ///< minidump 是否包含完整内存（更大）
    std::size_t max_breadcrumbs = 64;        ///< 面包屑容量
    UploadCallback upload_callback = nullptr;///< 可选上传回调（默认纯本地）
};

/**
 * @brief 崩溃报告器单例：安装全局处理器、管理面包屑/元数据、生成报告。
 */
class DSE_EXPORT CrashReporter {
public:
    static CrashReporter& Instance();

    /// 安装全局未处理异常/信号处理器。重复调用安全（幂等）。返回是否安装成功。
    bool Install(const CrashHandlerConfig& config);

    /// 卸载处理器，恢复先前处理器。
    void Uninstall();

    bool IsInstalled() const;

    /// 追加面包屑（线程安全）。
    void AddBreadcrumb(const std::string& entry);

    /// 设置/更新一条自定义元数据（线程安全），随报告落地。
    void SetMetadata(const std::string& key, const std::string& value);

    /// 上次写出的崩溃报告 .txt 路径（无则空）。
    std::string LastReportPath() const;

    /**
     * @brief 在“非崩溃”场景手动生成一份当前状态的报告（用于自检/测试/主动诊断）。
     *        Windows 上若 write_minidump 为真，会写一份当前线程的 minidump。
     * @param reason 报告原因标签。
     * @return 写出的 .txt 路径（失败返回空）。
     */
    std::string WriteManualReport(const std::string& reason);

    /// 当前配置（只读）。
    const CrashHandlerConfig& Config() const { return config_; }

    // 供平台处理器内部使用：基于当前面包屑/元数据/版本构造基础 CrashReportInfo。
    CrashReportInfo BuildBaseInfo(const std::string& reason) const;

    void SetLastReportPath(const std::string& path);

private:
    CrashReporter() = default;
    ~CrashReporter() = default;
    CrashReporter(const CrashReporter&) = delete;
    CrashReporter& operator=(const CrashReporter&) = delete;

    mutable std::mutex mutex_;
    CrashHandlerConfig config_;
    BreadcrumbBuffer breadcrumbs_{64};
    std::vector<std::pair<std::string, std::string>> metadata_;
    std::string last_report_path_;
    bool installed_ = false;
};

/// 便捷宏：记录一条面包屑。
#define DSE_CRASH_BREADCRUMB(...) \
    ::dse::diagnostics::CrashReporter::Instance().AddBreadcrumb(__VA_ARGS__)

}  // namespace diagnostics
}  // namespace dse

#endif  // DSE_DIAGNOSTICS_CRASH_HANDLER_H
