/**
* @file jolt_job_system_adapter.h
* @brief Jolt Physics JobSystem 适配器 —— 转发到引擎统一 JobSystem
*
* 将 Jolt 的 JobSystem 接口适配到引擎的 dse::core::JobSystem，
* 消除物理独立线程池造成的超订。物理作业从此弹性伸缩且不与
* gameplay 抢核（共用同一 worker 池）。
*
* 注意：本头文件不包含 engine/core/job_system.h，避免与 Jolt
* 头文件的命名空间产生冲突。JobSystem 指针通过 void* 传入，
* 在 QueueJob 中通过函数指针回调。
*/

#ifndef DSE_JOLT_JOB_SYSTEM_ADAPTER_H
#define DSE_JOLT_JOB_SYSTEM_ADAPTER_H

#ifdef DSE_ENABLE_JOLT

#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/Color.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <thread>
#include <functional>

namespace dse {
namespace physics3d {

/// Jolt JobSystem 适配器：转发到引擎统一 JobSystem
class JoltJobSystemAdapter final : public JPH::JobSystemWithBarrier {
public:
    /// 函数指针类型：将任务提交到引擎 JobSystem
    /// 使用 std::function<void()> 而非 JPH::JobSystem::JobFunction，
    /// 使桥接函数实现无需在同一 TU 中包含 Jolt 头文件和 job_system.h。
    using ExecuteFn = void(*)(void* job_sys, const std::function<void()>& func);

    /// @param engine_js 引擎 JobSystem 指针（生命周期由调用方管理）
    /// @param execute_fn 将任务提交到引擎 JobSystem 的函数指针
    JoltJobSystemAdapter(void* engine_js, ExecuteFn execute_fn)
        : engine_js_(engine_js), execute_fn_(execute_fn) {
        Init(JPH::cMaxPhysicsBarriers);
    }

    ~JoltJobSystemAdapter() override = default;

    // ── JobSystem 接口实现 ──

    int GetMaxConcurrency() const override {
        return static_cast<int>(std::thread::hardware_concurrency());
    }

    JPH::JobSystem::JobHandle CreateJob(const char* inName, JPH::ColorArg inColor,
                        const JPH::JobSystem::JobFunction& inJobFunction,
                        JPH::uint32 inNumDependencies = 0) override {
        JPH::JobSystem::Job* job = new JPH::JobSystem::Job(inName, inColor, this, inJobFunction, inNumDependencies);
        return JPH::JobSystem::JobHandle(job);
    }

    void FreeJob(JPH::JobSystem::Job* inJob) override {
        delete inJob;
    }

protected:
    void QueueJob(JPH::JobSystem::Job* inJob) override {
        if (!execute_fn_) {
            inJob->Execute();
            inJob->Release();
            return;
        }

        inJob->AddRef();
        // 将 Jolt job 的 Execute+Release 包装为 std::function<void()>
        execute_fn_(engine_js_, [inJob]() {
            inJob->Execute();
            inJob->Release();
        });
    }

    void QueueJobs(JPH::JobSystem::Job** inJobs, JPH::uint inNumJobs) override {
        for (JPH::uint i = 0; i < inNumJobs; ++i) {
            QueueJob(inJobs[i]);
        }
    }

private:
    void* engine_js_ = nullptr;
    ExecuteFn execute_fn_ = nullptr;
};

} // namespace physics3d
} // namespace dse

#endif // DSE_ENABLE_JOLT
#endif // DSE_JOLT_JOB_SYSTEM_ADAPTER_H
