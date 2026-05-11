#ifndef DSE_FLUID_SYSTEM_H
#define DSE_FLUID_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

namespace dse {
struct FluidEmitterComponent;

namespace gameplay3d {

/**
 * @class FluidSystem
 * @brief 基于粒子的流体模拟系统（SPH + 屏幕空间渲染）
 *
 * 每帧更新流程：
 *   1. 根据 emission_rate 发射新粒子
 *   2. 对每个存活粒子：
 *      a. 空间哈希邻居搜索
 *      b. 密度与压力估计
 *      c. 压力 + 粘性力
 *      d. 外力（重力）
 *      e. 位置积分
 *      f. 碰撞检测（地面）
 *   3. 移除过期粒子，压缩缓冲区
 *   4. 上传实例数据到 GPU（位置 + 半径，供渲染使用）
 *
 * 渲染通过现有粒子管线或专用的屏幕空间流体 pass
 * （深度 → 平滑 → 法线 → 着色）完成。
 */
class FluidSystem {
public:
    FluidSystem() = default;
    ~FluidSystem() = default;

    void Init(World& world, RhiDevice* rhi);
    void Update(World& world, float delta_time);
    void Shutdown(World& world);

private:
    // --- SPH 核函数 ---
    static float KernelPoly6(float r2, float h);
    static float KernelSpikyGrad(float r, float h);
    static float KernelViscosityLaplacian(float r, float h);

    void EmitParticles(FluidEmitterComponent& fluid, const glm::vec3& emitter_pos,
                       const glm::quat& emitter_rot, float dt);
    void SimulateSPH(FluidEmitterComponent& fluid, float dt);
    void CompactParticles(FluidEmitterComponent& fluid);

    // 空间哈希（用于邻居搜索）
    struct SpatialHash {
        float cell_size = 0.1f;
        std::unordered_map<uint64_t, std::vector<uint32_t>> cells;

        void Clear();
        void Insert(uint32_t index, const glm::vec3& pos);
        uint64_t Hash(int x, int y, int z) const;
        void GetNeighborCells(const glm::vec3& pos, float radius,
                              std::vector<uint32_t>& out_indices) const;
    };

    SpatialHash spatial_hash_;
    RhiDevice* rhi_ = nullptr;

    void UploadGpuData(FluidEmitterComponent& fluid);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_FLUID_SYSTEM_H
