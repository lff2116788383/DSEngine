/**
 * @file nav_mesh_system.h
 * @brief NavMesh / 寻路系统 — 基于 Recast/Detour
 *
 * 设计原则:
 * - 引擎级服务（与 Physics3DSystem 同级），通过 ServiceLocator 注册
 * - Bake 一次，多次查询；支持二进制序列化避免每次启动重建
 * - 单 dtNavMeshQuery（Phase 1 单线程）；后续可扩展 per-thread query
 *
 * 用法:
 * @code
 *   auto* nav = ServiceLocator::Instance().Get<NavMeshSystem>();
 *   nav->BakeFromTriangles(verts, nverts, tris, ntris);
 *   std::vector<glm::vec3> path;
 *   nav->FindPath({0,0,0}, {10,0,10}, path);
 * @endcode
 */

#ifndef DSE_NAVIGATION_NAV_MESH_SYSTEM_H
#define DSE_NAVIGATION_NAV_MESH_SYSTEM_H

#ifdef DSE_ENABLE_NAVMESH

#include <glm/glm.hpp>
#include <string>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

namespace dse::navigation {

/// NavMesh 构建配置（单位:世界单位）
struct NavMeshBuildConfig {
    float cell_size       = 0.3f;   ///< 体素 XZ 大小
    float cell_height     = 0.2f;   ///< 体素 Y 大小
    float agent_height    = 2.0f;   ///< 代理高度
    float agent_radius    = 0.6f;   ///< 代理半径
    float agent_max_climb = 0.9f;   ///< 最大攀爬高度
    float agent_max_slope = 45.0f;  ///< 最大坡度（度）
    float region_min_size = 8.0f;   ///< 最小区域尺寸（体素）
    float region_merge_size = 20.0f;///< 区域合并尺寸（体素）
    float edge_max_len    = 12.0f;  ///< 边最大长度（世界单位）
    float edge_max_error  = 1.3f;   ///< 边简化最大误差
    int   verts_per_poly  = 6;      ///< 每个多边形最大顶点数（建议 6）
    float detail_sample_dist = 6.0f;
    float detail_sample_max_error = 1.0f;
};

class NavMeshSystem {
public:
    NavMeshSystem();
    ~NavMeshSystem();

    NavMeshSystem(const NavMeshSystem&) = delete;
    NavMeshSystem& operator=(const NavMeshSystem&) = delete;

    /// 初始化（创建查询对象等）
    bool Init();
    /// 释放所有资源
    void Shutdown();

    /// 是否已构建可用的 navmesh
    bool IsReady() const;

    // ---- 构建 ----

    /**
     * @brief 从三角面数据构建 navmesh
     * @param verts 顶点数组 [x0,y0,z0, x1,y1,z1, ...]，长度 = nverts*3
     * @param nverts 顶点数
     * @param tris 三角形索引数组 [i0,i1,i2, ...]，长度 = ntris*3
     * @param ntris 三角形数
     * @param cfg 构建配置
     * @return 成功返回 true
     */
    bool BakeFromTriangles(const float* verts, int nverts,
                           const int* tris, int ntris,
                           const NavMeshBuildConfig& cfg = {});

    // ---- 查询 ----

    /**
     * @brief 路径查找
     * @param start 起点世界坐标
     * @param end 终点世界坐标
     * @param[out] path 输出路径点（含起点和终点）
     * @param max_points 最多返回路径点数（默认 256）
     * @return 找到路径返回 true
     */
    bool FindPath(const glm::vec3& start, const glm::vec3& end,
                  std::vector<glm::vec3>& path, int max_points = 256) const;

    /**
     * @brief 查找最近的 navmesh 点
     * @param pos 查询位置
     * @param[out] nearest 输出最近点（找不到时为输入 pos）
     * @return 找到返回 true
     */
    bool FindNearestPoint(const glm::vec3& pos, glm::vec3& nearest) const;

    /**
     * @brief 在 navmesh 上做 2D 射线检测
     * @param start 起点
     * @param end 终点
     * @param[out] hit_pos 命中点（未命中时为 end）
     * @return 命中障碍返回 true（即 end 在 navmesh 外）
     */
    bool Raycast(const glm::vec3& start, const glm::vec3& end,
                 glm::vec3& hit_pos) const;

    // ---- 序列化 ----

    /// 保存 navmesh 二进制数据到文件
    bool SaveNavMesh(const std::string& path) const;
    /// 从文件加载 navmesh 二进制数据
    bool LoadNavMesh(const std::string& path);

private:
    void ReleaseNavMesh();

    dtNavMesh*      nav_mesh_  = nullptr;
    dtNavMeshQuery* nav_query_ = nullptr;
    dtQueryFilter*  filter_    = nullptr;
    bool initialized_ = false;
};

} // namespace dse::navigation

#endif // DSE_ENABLE_NAVMESH

#endif // DSE_NAVIGATION_NAV_MESH_SYSTEM_H
