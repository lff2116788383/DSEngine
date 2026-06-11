/**
 * @file nav_mesh_system.cpp
 * @brief NavMesh / 寻路系统 — Recast 构建 + Detour 查询实现
 */

#ifdef DSE_ENABLE_NAVMESH

#include "engine/navigation/nav_mesh_system.h"
#include "engine/base/debug.h"

#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"
#include "DetourAlloc.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <memory>
#include <cstdint>

namespace dse::navigation {

namespace {

/// 简易 Recast 日志上下文（吞掉 ctx 内部输出，避免污染日志）
class SilentContext : public rcContext {
public:
    SilentContext() : rcContext(false) {}
protected:
    void doLog(const rcLogCategory /*category*/, const char* /*msg*/, const int /*len*/) override {}
};

/// 自动释放 Recast 中间数据
template <typename T, void(*FreeFn)(T*)>
struct RcDeleter { void operator()(T* p) const { if (p) FreeFn(p); } };

using HeightfieldPtr  = std::unique_ptr<rcHeightfield,    RcDeleter<rcHeightfield,    rcFreeHeightField>>;
using CompactHFPtr    = std::unique_ptr<rcCompactHeightfield, RcDeleter<rcCompactHeightfield, rcFreeCompactHeightfield>>;
using ContourSetPtr   = std::unique_ptr<rcContourSet,     RcDeleter<rcContourSet,     rcFreeContourSet>>;
using PolyMeshPtr     = std::unique_ptr<rcPolyMesh,       RcDeleter<rcPolyMesh,       rcFreePolyMesh>>;
using PolyMeshDetPtr  = std::unique_ptr<rcPolyMeshDetail, RcDeleter<rcPolyMeshDetail, rcFreePolyMeshDetail>>;

/// 校验三角形 / 顶点输入
bool ValidateInput(const float* verts, int nverts, const int* tris, int ntris) {
    if (!verts || nverts <= 0) {
        DEBUG_LOG_ERROR("[NavMesh] invalid verts (count={})", nverts);
        return false;
    }
    if (!tris || ntris <= 0) {
        DEBUG_LOG_ERROR("[NavMesh] invalid tris (count={})", ntris);
        return false;
    }
    return true;
}

} // namespace

// ============================================================
// 生命周期
// ============================================================

NavMeshSystem::NavMeshSystem() = default;

NavMeshSystem::~NavMeshSystem() {
    Shutdown();
}

bool NavMeshSystem::Init() {
    if (initialized_) return true;
    filter_ = new dtQueryFilter();
    filter_->setIncludeFlags(0xffff);
    filter_->setExcludeFlags(0);
    initialized_ = true;
    return true;
}

void NavMeshSystem::Shutdown() {
    ReleaseNavMesh();
    if (filter_) { delete filter_; filter_ = nullptr; }
    initialized_ = false;
}

void NavMeshSystem::ReleaseNavMesh() {
    if (nav_query_) { dtFreeNavMeshQuery(nav_query_); nav_query_ = nullptr; }
    if (nav_mesh_)  { dtFreeNavMesh(nav_mesh_);       nav_mesh_  = nullptr; }
    tiled_mode_ = false;
}

bool NavMeshSystem::IsReady() const {
    return initialized_ && nav_mesh_ != nullptr && nav_query_ != nullptr;
}

int NavMeshSystem::GetPolyCount() const {
    if (!nav_mesh_) return 0;
    const dtNavMesh* mesh = nav_mesh_;
    int total = 0;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (tile && tile->header)
            total += tile->header->polyCount;
    }
    return total;
}

// ============================================================
// 构建
// ============================================================

bool NavMeshSystem::BakeFromTriangles(const float* verts, int nverts,
                                       const int* tris, int ntris,
                                       const NavMeshBuildConfig& cfg) {
    if (!initialized_) {
        DEBUG_LOG_ERROR("[NavMesh] BakeFromTriangles called before Init()");
        return false;
    }
    if (!ValidateInput(verts, nverts, tris, ntris)) return false;

    ReleaseNavMesh();

    SilentContext ctx;

    // ---- 配置 ----
    rcConfig rcfg{};
    rcfg.cs                     = cfg.cell_size;
    rcfg.ch                     = cfg.cell_height;
    rcfg.walkableSlopeAngle     = cfg.agent_max_slope;
    rcfg.walkableHeight         = (int)ceilf(cfg.agent_height / cfg.cell_height);
    rcfg.walkableClimb          = (int)floorf(cfg.agent_max_climb / cfg.cell_height);
    rcfg.walkableRadius         = (int)ceilf(cfg.agent_radius / cfg.cell_size);
    rcfg.maxEdgeLen             = (int)(cfg.edge_max_len / cfg.cell_size);
    rcfg.maxSimplificationError = cfg.edge_max_error;
    rcfg.minRegionArea          = (int)rcSqr(cfg.region_min_size);
    rcfg.mergeRegionArea        = (int)rcSqr(cfg.region_merge_size);
    rcfg.maxVertsPerPoly        = cfg.verts_per_poly;
    rcfg.detailSampleDist       = cfg.detail_sample_dist < 0.9f ? 0.0f : cfg.cell_size * cfg.detail_sample_dist;
    rcfg.detailSampleMaxError   = cfg.cell_height * cfg.detail_sample_max_error;

    // 计算 AABB
    float bmin[3], bmax[3];
    rcCalcBounds(verts, nverts, bmin, bmax);
    memcpy(rcfg.bmin, bmin, sizeof(bmin));
    memcpy(rcfg.bmax, bmax, sizeof(bmax));
    rcCalcGridSize(rcfg.bmin, rcfg.bmax, rcfg.cs, &rcfg.width, &rcfg.height);

    DEBUG_LOG_INFO("[NavMesh] bake: verts={}, tris={}, grid={}x{}",
                   nverts, ntris, rcfg.width, rcfg.height);

    // ---- 1. 体素化 ----
    HeightfieldPtr solid(rcAllocHeightfield());
    if (!solid || !rcCreateHeightfield(&ctx, *solid, rcfg.width, rcfg.height,
                                        rcfg.bmin, rcfg.bmax, rcfg.cs, rcfg.ch)) {
        DEBUG_LOG_ERROR("[NavMesh] rcCreateHeightfield failed");
        return false;
    }

    std::vector<unsigned char> areas(ntris, 0);
    rcMarkWalkableTriangles(&ctx, rcfg.walkableSlopeAngle, verts, nverts,
                            tris, ntris, areas.data());
    if (!rcRasterizeTriangles(&ctx, verts, nverts, tris, areas.data(), ntris,
                              *solid, rcfg.walkableClimb)) {
        DEBUG_LOG_ERROR("[NavMesh] rcRasterizeTriangles failed");
        return false;
    }

    // ---- 2. 过滤 ----
    rcFilterLowHangingWalkableObstacles(&ctx, rcfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, rcfg.walkableHeight, rcfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, rcfg.walkableHeight, *solid);

    // ---- 3. 紧凑高度场 ----
    CompactHFPtr chf(rcAllocCompactHeightfield());
    if (!chf || !rcBuildCompactHeightfield(&ctx, rcfg.walkableHeight,
                                            rcfg.walkableClimb, *solid, *chf)) {
        DEBUG_LOG_ERROR("[NavMesh] rcBuildCompactHeightfield failed");
        return false;
    }
    solid.reset();

    if (!rcErodeWalkableArea(&ctx, rcfg.walkableRadius, *chf)) {
        DEBUG_LOG_ERROR("[NavMesh] rcErodeWalkableArea failed");
        return false;
    }

    // 分水岭分区
    if (!rcBuildDistanceField(&ctx, *chf) ||
        !rcBuildRegions(&ctx, *chf, 0, rcfg.minRegionArea, rcfg.mergeRegionArea)) {
        DEBUG_LOG_ERROR("[NavMesh] rcBuildRegions failed");
        return false;
    }

    // ---- 4. 等高线 ----
    ContourSetPtr cset(rcAllocContourSet());
    if (!cset || !rcBuildContours(&ctx, *chf, rcfg.maxSimplificationError,
                                   rcfg.maxEdgeLen, *cset)) {
        DEBUG_LOG_ERROR("[NavMesh] rcBuildContours failed");
        return false;
    }

    // ---- 5. PolyMesh ----
    PolyMeshPtr pmesh(rcAllocPolyMesh());
    if (!pmesh || !rcBuildPolyMesh(&ctx, *cset, rcfg.maxVertsPerPoly, *pmesh)) {
        DEBUG_LOG_ERROR("[NavMesh] rcBuildPolyMesh failed");
        return false;
    }

    PolyMeshDetPtr dmesh(rcAllocPolyMeshDetail());
    if (!dmesh || !rcBuildPolyMeshDetail(&ctx, *pmesh, *chf,
                                          rcfg.detailSampleDist,
                                          rcfg.detailSampleMaxError, *dmesh)) {
        DEBUG_LOG_ERROR("[NavMesh] rcBuildPolyMeshDetail failed");
        return false;
    }

    // 标记所有可行走 polygon 为默认区域 + flag 1
    for (int i = 0; i < pmesh->npolys; ++i) {
        if (pmesh->areas[i] == RC_WALKABLE_AREA) {
            pmesh->flags[i] = 0x01;
        }
    }

    // ---- 6. Detour navmesh data ----
    dtNavMeshCreateParams params{};
    params.verts            = pmesh->verts;
    params.vertCount        = pmesh->nverts;
    params.polys            = pmesh->polys;
    params.polyAreas        = pmesh->areas;
    params.polyFlags        = pmesh->flags;
    params.polyCount        = pmesh->npolys;
    params.nvp              = pmesh->nvp;
    params.detailMeshes     = dmesh->meshes;
    params.detailVerts      = dmesh->verts;
    params.detailVertsCount = dmesh->nverts;
    params.detailTris       = dmesh->tris;
    params.detailTriCount   = dmesh->ntris;
    params.walkableHeight   = cfg.agent_height;
    params.walkableRadius   = cfg.agent_radius;
    params.walkableClimb    = cfg.agent_max_climb;
    memcpy(params.bmin, pmesh->bmin, sizeof(params.bmin));
    memcpy(params.bmax, pmesh->bmax, sizeof(params.bmax));
    params.cs               = rcfg.cs;
    params.ch               = rcfg.ch;
    params.buildBvTree      = true;

    unsigned char* nav_data = nullptr;
    int            nav_data_size = 0;
    if (!dtCreateNavMeshData(&params, &nav_data, &nav_data_size)) {
        DEBUG_LOG_ERROR("[NavMesh] dtCreateNavMeshData failed");
        return false;
    }

    nav_mesh_ = dtAllocNavMesh();
    if (!nav_mesh_) {
        dtFree(nav_data);
        return false;
    }
    dtStatus status = nav_mesh_->init(nav_data, nav_data_size, DT_TILE_FREE_DATA);
    if (dtStatusFailed(status)) {
        dtFree(nav_data);
        dtFreeNavMesh(nav_mesh_);
        nav_mesh_ = nullptr;
        DEBUG_LOG_ERROR("[NavMesh] dtNavMesh::init failed");
        return false;
    }

    nav_query_ = dtAllocNavMeshQuery();
    if (!nav_query_ || dtStatusFailed(nav_query_->init(nav_mesh_, 2048))) {
        DEBUG_LOG_ERROR("[NavMesh] dtNavMeshQuery::init failed");
        ReleaseNavMesh();
        return false;
    }

    DEBUG_LOG_INFO("[NavMesh] bake OK: polys={}, navdata_size={} bytes",
                   pmesh->npolys, nav_data_size);
    return true;
}

// ============================================================
// 查询
// ============================================================

bool NavMeshSystem::FindPath(const glm::vec3& start, const glm::vec3& end,
                              std::vector<glm::vec3>& path, int max_points) const {
    path.clear();
    if (!IsReady()) return false;

    // Floating Origin: 输入坐标加 offset 转回 navmesh 空间
    const glm::vec3 s_nav = start + accumulated_offset_;
    const glm::vec3 e_nav = end + accumulated_offset_;
    const float ext[3] = { 2.0f, 4.0f, 2.0f };
    const float s[3] = { s_nav.x, s_nav.y, s_nav.z };
    const float e[3] = { e_nav.x, e_nav.y, e_nav.z };

    dtPolyRef start_ref = 0, end_ref = 0;
    float start_pt[3], end_pt[3];
    nav_query_->findNearestPoly(s, ext, filter_, &start_ref, start_pt);
    nav_query_->findNearestPoly(e, ext, filter_, &end_ref,   end_pt);
    if (!start_ref || !end_ref) return false;

    std::vector<dtPolyRef> poly_path(max_points);
    int poly_count = 0;
    dtStatus st = nav_query_->findPath(start_ref, end_ref, start_pt, end_pt,
                                        filter_, poly_path.data(), &poly_count, max_points);
    if (dtStatusFailed(st) || poly_count == 0) return false;

    std::vector<float> straight(max_points * 3);
    int straight_count = 0;
    st = nav_query_->findStraightPath(start_pt, end_pt, poly_path.data(), poly_count,
                                       straight.data(), nullptr, nullptr,
                                       &straight_count, max_points, 0);
    if (dtStatusFailed(st) || straight_count == 0) return false;

    path.reserve(straight_count);
    for (int i = 0; i < straight_count; ++i) {
        // Floating Origin: 输出坐标减 offset 转回当前坐标系
        path.emplace_back(straight[i*3+0] - accumulated_offset_.x,
                          straight[i*3+1] - accumulated_offset_.y,
                          straight[i*3+2] - accumulated_offset_.z);
    }
    return true;
}

bool NavMeshSystem::FindNearestPoint(const glm::vec3& pos, glm::vec3& nearest) const {
    nearest = pos;
    if (!IsReady()) return false;
    // Floating Origin: 输入坐标加 offset 转回 navmesh 空间
    const glm::vec3 p_nav = pos + accumulated_offset_;
    const float ext[3] = { 2.0f, 4.0f, 2.0f };
    const float p[3] = { p_nav.x, p_nav.y, p_nav.z };
    dtPolyRef ref = 0;
    float pt[3];
    dtStatus st = nav_query_->findNearestPoly(p, ext, filter_, &ref, pt);
    if (dtStatusFailed(st) || !ref) return false;
    // Floating Origin: 输出坐标减 offset 转回当前坐标系
    nearest = glm::vec3(pt[0], pt[1], pt[2]) - accumulated_offset_;
    return true;
}

bool NavMeshSystem::Raycast(const glm::vec3& start, const glm::vec3& end,
                             glm::vec3& hit_pos) const {
    hit_pos = end;
    if (!IsReady()) return false;

    // Floating Origin: 输入坐标加 offset 转回 navmesh 空间
    const glm::vec3 s_nav = start + accumulated_offset_;
    const glm::vec3 e_nav = end + accumulated_offset_;
    const float ext[3] = { 2.0f, 4.0f, 2.0f };
    const float s[3] = { s_nav.x, s_nav.y, s_nav.z };
    const float e[3] = { e_nav.x, e_nav.y, e_nav.z };

    dtPolyRef start_ref = 0;
    float start_pt[3];
    nav_query_->findNearestPoly(s, ext, filter_, &start_ref, start_pt);
    if (!start_ref) return false;

    float t = 1.0f;
    float hit_normal[3];
    dtPolyRef visited[64];
    int visited_count = 0;
    dtStatus st = nav_query_->raycast(start_ref, start_pt, e, filter_,
                                       &t, hit_normal, visited, &visited_count, 64);
    if (dtStatusFailed(st)) return false;

    if (t >= 1.0f) {
        // 未命中障碍，hit_pos 即为终点
        hit_pos = end;
        return false;
    }
    hit_pos = glm::vec3(
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t,
        start.z + (end.z - start.z) * t);
    return true;
}

// ============================================================
// 序列化
// ============================================================

namespace {
constexpr uint32_t NAVMESH_MAGIC   = 0x444E4D58; // 'DNMX'
constexpr uint32_t NAVMESH_VERSION = 1;

struct NavMeshFileHeader {
    uint32_t magic;
    uint32_t version;
    int32_t  data_size;
};
} // namespace

bool NavMeshSystem::SaveNavMesh(const std::string& path) const {
    if (!IsReady()) return false;
    // 当前实现仅支持单 tile（dtNavMesh::init 单 buffer 模式）
    const dtNavMesh* mesh_const = nav_mesh_;
    const dtMeshTile* tile = mesh_const->getTile(0);
    if (!tile || !tile->header || !tile->data) {
        DEBUG_LOG_ERROR("[NavMesh] SaveNavMesh: no tile data");
        return false;
    }

    FILE* fp = nullptr;
#if defined(_WIN32)
    fopen_s(&fp, path.c_str(), "wb");
#else
    fp = fopen(path.c_str(), "wb");
#endif
    if (!fp) {
        DEBUG_LOG_ERROR("[NavMesh] SaveNavMesh: cannot open {}", path);
        return false;
    }

    NavMeshFileHeader hdr{ NAVMESH_MAGIC, NAVMESH_VERSION, tile->dataSize };
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fwrite(tile->data, tile->dataSize, 1, fp);
    fclose(fp);
    DEBUG_LOG_INFO("[NavMesh] saved {} ({} bytes)", path, tile->dataSize);
    return true;
}

bool NavMeshSystem::LoadNavMesh(const std::string& path) {
    if (!initialized_) {
        DEBUG_LOG_ERROR("[NavMesh] LoadNavMesh called before Init()");
        return false;
    }
    FILE* fp = nullptr;
#if defined(_WIN32)
    fopen_s(&fp, path.c_str(), "rb");
#else
    fp = fopen(path.c_str(), "rb");
#endif
    if (!fp) return false;

    NavMeshFileHeader hdr{};
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1 ||
        hdr.magic != NAVMESH_MAGIC || hdr.version != NAVMESH_VERSION ||
        hdr.data_size <= 0) {
        fclose(fp);
        DEBUG_LOG_ERROR("[NavMesh] LoadNavMesh: bad header in {}", path);
        return false;
    }

    unsigned char* data = static_cast<unsigned char*>(dtAlloc(hdr.data_size, DT_ALLOC_PERM));
    if (!data) { fclose(fp); return false; }
    if ((int)fread(data, 1, hdr.data_size, fp) != hdr.data_size) {
        dtFree(data);
        fclose(fp);
        return false;
    }
    fclose(fp);

    ReleaseNavMesh();
    nav_mesh_ = dtAllocNavMesh();
    if (!nav_mesh_ || dtStatusFailed(nav_mesh_->init(data, hdr.data_size, DT_TILE_FREE_DATA))) {
        dtFree(data);
        if (nav_mesh_) { dtFreeNavMesh(nav_mesh_); nav_mesh_ = nullptr; }
        DEBUG_LOG_ERROR("[NavMesh] LoadNavMesh: dtNavMesh::init failed");
        return false;
    }

    nav_query_ = dtAllocNavMeshQuery();
    if (!nav_query_ || dtStatusFailed(nav_query_->init(nav_mesh_, 2048))) {
        ReleaseNavMesh();
        return false;
    }
    DEBUG_LOG_INFO("[NavMesh] loaded {} ({} bytes)", path, hdr.data_size);
    return true;
}

// ============================================================
// Floating Origin
// ============================================================
void NavMeshSystem::RebaseOrigin(const glm::vec3& offset) {
    accumulated_offset_ += offset;
}

// ============================================================
// Tiled NavMesh
// ============================================================

bool NavMeshSystem::BuildTile(int tx, int tz,
                              const float* tile_bmin, const float* tile_bmax,
                              const float* verts, int nverts,
                              const int* tris, int ntris,
                              const NavMeshBuildConfig& cfg) {
    SilentContext ctx;

    rcConfig rcfg{};
    rcfg.cs                     = cfg.cell_size;
    rcfg.ch                     = cfg.cell_height;
    rcfg.walkableSlopeAngle     = cfg.agent_max_slope;
    rcfg.walkableHeight         = (int)ceilf(cfg.agent_height / cfg.cell_height);
    rcfg.walkableClimb          = (int)floorf(cfg.agent_max_climb / cfg.cell_height);
    rcfg.walkableRadius         = (int)ceilf(cfg.agent_radius / cfg.cell_size);
    rcfg.maxEdgeLen             = (int)(cfg.edge_max_len / cfg.cell_size);
    rcfg.maxSimplificationError = cfg.edge_max_error;
    rcfg.minRegionArea          = (int)rcSqr(cfg.region_min_size);
    rcfg.mergeRegionArea        = (int)rcSqr(cfg.region_merge_size);
    rcfg.maxVertsPerPoly        = cfg.verts_per_poly;
    rcfg.detailSampleDist       = cfg.detail_sample_dist < 0.9f ? 0.0f : cfg.cell_size * cfg.detail_sample_dist;
    rcfg.detailSampleMaxError   = cfg.cell_height * cfg.detail_sample_max_error;

    // Expand tile bounds by border to avoid seam artifacts
    const int border_size = rcfg.walkableRadius + 3;
    rcfg.borderSize = border_size;
    const float border_expand = border_size * cfg.cell_size;

    rcfg.bmin[0] = tile_bmin[0] - border_expand;
    rcfg.bmin[1] = tile_bmin[1];
    rcfg.bmin[2] = tile_bmin[2] - border_expand;
    rcfg.bmax[0] = tile_bmax[0] + border_expand;
    rcfg.bmax[1] = tile_bmax[1];
    rcfg.bmax[2] = tile_bmax[2] + border_expand;
    rcCalcGridSize(rcfg.bmin, rcfg.bmax, rcfg.cs, &rcfg.width, &rcfg.height);

    // 1. Heightfield
    HeightfieldPtr solid(rcAllocHeightfield());
    if (!solid || !rcCreateHeightfield(&ctx, *solid, rcfg.width, rcfg.height,
                                        rcfg.bmin, rcfg.bmax, rcfg.cs, rcfg.ch)) {
        return false;
    }

    std::vector<unsigned char> areas(ntris, 0);
    rcMarkWalkableTriangles(&ctx, rcfg.walkableSlopeAngle, verts, nverts,
                            tris, ntris, areas.data());
    if (!rcRasterizeTriangles(&ctx, verts, nverts, tris, areas.data(), ntris,
                              *solid, rcfg.walkableClimb)) {
        return false;
    }

    // 2. Filter
    rcFilterLowHangingWalkableObstacles(&ctx, rcfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, rcfg.walkableHeight, rcfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, rcfg.walkableHeight, *solid);

    // 3. Compact heightfield
    CompactHFPtr chf(rcAllocCompactHeightfield());
    if (!chf || !rcBuildCompactHeightfield(&ctx, rcfg.walkableHeight,
                                            rcfg.walkableClimb, *solid, *chf)) {
        return false;
    }
    solid.reset();

    if (!rcErodeWalkableArea(&ctx, rcfg.walkableRadius, *chf)) {
        return false;
    }

    if (!rcBuildDistanceField(&ctx, *chf) ||
        !rcBuildRegions(&ctx, *chf, rcfg.borderSize, rcfg.minRegionArea, rcfg.mergeRegionArea)) {
        return false;
    }

    // 4. Contours
    ContourSetPtr cset(rcAllocContourSet());
    if (!cset || !rcBuildContours(&ctx, *chf, rcfg.maxSimplificationError,
                                   rcfg.maxEdgeLen, *cset)) {
        return false;
    }

    // 5. PolyMesh
    PolyMeshPtr pmesh(rcAllocPolyMesh());
    if (!pmesh || !rcBuildPolyMesh(&ctx, *cset, rcfg.maxVertsPerPoly, *pmesh)) {
        return false;
    }

    PolyMeshDetPtr dmesh(rcAllocPolyMeshDetail());
    if (!dmesh || !rcBuildPolyMeshDetail(&ctx, *pmesh, *chf,
                                          rcfg.detailSampleDist,
                                          rcfg.detailSampleMaxError, *dmesh)) {
        return false;
    }

    if (pmesh->npolys == 0) {
        return true; // empty tile is fine
    }

    for (int i = 0; i < pmesh->npolys; ++i) {
        if (pmesh->areas[i] == RC_WALKABLE_AREA) {
            pmesh->flags[i] = 0x01;
        }
    }

    // 6. Detour tile data
    dtNavMeshCreateParams params{};
    params.verts            = pmesh->verts;
    params.vertCount        = pmesh->nverts;
    params.polys            = pmesh->polys;
    params.polyAreas        = pmesh->areas;
    params.polyFlags        = pmesh->flags;
    params.polyCount        = pmesh->npolys;
    params.nvp              = pmesh->nvp;
    params.detailMeshes     = dmesh->meshes;
    params.detailVerts      = dmesh->verts;
    params.detailVertsCount = dmesh->nverts;
    params.detailTris       = dmesh->tris;
    params.detailTriCount   = dmesh->ntris;
    params.walkableHeight   = cfg.agent_height;
    params.walkableRadius   = cfg.agent_radius;
    params.walkableClimb    = cfg.agent_max_climb;
    memcpy(params.bmin, pmesh->bmin, sizeof(params.bmin));
    memcpy(params.bmax, pmesh->bmax, sizeof(params.bmax));
    params.cs               = rcfg.cs;
    params.ch               = rcfg.ch;
    params.buildBvTree      = true;
    params.tileX            = tx;
    params.tileY            = tz;
    params.tileLayer        = 0;

    unsigned char* nav_data = nullptr;
    int nav_data_size = 0;
    if (!dtCreateNavMeshData(&params, &nav_data, &nav_data_size)) {
        return false;
    }

    dtStatus status = nav_mesh_->addTile(nav_data, nav_data_size, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(status)) {
        dtFree(nav_data);
        return false;
    }
    return true;
}

bool NavMeshSystem::BakeTiledFromTriangles(const float* verts, int nverts,
                                            const int* tris, int ntris,
                                            float tile_size,
                                            const NavMeshBuildConfig& cfg) {
    if (!initialized_) {
        DEBUG_LOG_ERROR("[NavMesh] BakeTiledFromTriangles called before Init()");
        return false;
    }
    if (!ValidateInput(verts, nverts, tris, ntris)) return false;

    ReleaseNavMesh();
    tiled_mode_ = true;
    tile_size_ = tile_size;
    tiled_cfg_ = cfg;

    // Calculate overall bounds
    float bmin[3], bmax[3];
    rcCalcBounds(verts, nverts, bmin, bmax);
    memcpy(bmin_, bmin, sizeof(bmin_));

    // Calculate tile grid dimensions
    const int tw = (int)((bmax[0] - bmin[0]) / tile_size) + 1;
    const int th = (int)((bmax[2] - bmin[2]) / tile_size) + 1;

    DEBUG_LOG_INFO("[NavMesh] tiled bake: verts={}, tris={}, tiles={}x{}, tile_size={}",
                   nverts, ntris, tw, th, tile_size);

    // Initialize multi-tile navmesh
    dtNavMeshParams navParams;
    memset(&navParams, 0, sizeof(navParams));
    navParams.orig[0] = bmin[0];
    navParams.orig[1] = bmin[1];
    navParams.orig[2] = bmin[2];
    navParams.tileWidth = tile_size;
    navParams.tileHeight = tile_size;
    navParams.maxTiles = tw * th;
    navParams.maxPolys = tw * th * 256;

    nav_mesh_ = dtAllocNavMesh();
    if (!nav_mesh_ || dtStatusFailed(nav_mesh_->init(&navParams))) {
        DEBUG_LOG_ERROR("[NavMesh] dtNavMesh multi-tile init failed");
        ReleaseNavMesh();
        return false;
    }

    // Build each tile
    int built = 0;
    for (int tz = 0; tz < th; ++tz) {
        for (int tx = 0; tx < tw; ++tx) {
            float tile_bmin[3], tile_bmax[3];
            tile_bmin[0] = bmin[0] + tx * tile_size;
            tile_bmin[1] = bmin[1];
            tile_bmin[2] = bmin[2] + tz * tile_size;
            tile_bmax[0] = bmin[0] + (tx + 1) * tile_size;
            tile_bmax[1] = bmax[1];
            tile_bmax[2] = bmin[2] + (tz + 1) * tile_size;

            if (BuildTile(tx, tz, tile_bmin, tile_bmax, verts, nverts, tris, ntris, cfg)) {
                ++built;
            }
        }
    }

    // Initialize query
    nav_query_ = dtAllocNavMeshQuery();
    if (!nav_query_ || dtStatusFailed(nav_query_->init(nav_mesh_, 2048))) {
        DEBUG_LOG_ERROR("[NavMesh] dtNavMeshQuery::init failed (tiled)");
        ReleaseNavMesh();
        return false;
    }

    DEBUG_LOG_INFO("[NavMesh] tiled bake OK: {}/{} tiles built", built, tw * th);
    return true;
}

bool NavMeshSystem::RebakeTileAt(float world_x, float world_z,
                                  const float* verts, int nverts,
                                  const int* tris, int ntris,
                                  const NavMeshBuildConfig& cfg) {
    if (!tiled_mode_ || !nav_mesh_) {
        DEBUG_LOG_ERROR("[NavMesh] RebakeTileAt: not in tiled mode");
        return false;
    }

    const int tx = (int)((world_x - bmin_[0]) / tile_size_);
    const int tz = (int)((world_z - bmin_[2]) / tile_size_);

    // Remove existing tile(s) at this location
    RemoveTile(tx, tz);

    // Calculate tile bounds
    float tile_bmin[3], tile_bmax[3];
    tile_bmin[0] = bmin_[0] + tx * tile_size_;
    tile_bmin[1] = bmin_[1];
    tile_bmin[2] = bmin_[2] + tz * tile_size_;
    tile_bmax[0] = bmin_[0] + (tx + 1) * tile_size_;
    tile_bmax[1] = bmin_[1] + 1000.0f; // large Y range
    tile_bmax[2] = bmin_[2] + (tz + 1) * tile_size_;

    bool ok = BuildTile(tx, tz, tile_bmin, tile_bmax, verts, nverts, tris, ntris, cfg);

    // Reinit query to pick up changes
    if (nav_query_) {
        nav_query_->init(nav_mesh_, 2048);
    }
    return ok;
}

void NavMeshSystem::RemoveTile(int tx, int tz) {
    if (!nav_mesh_) return;
    dtTileRef ref = nav_mesh_->getTileRefAt(tx, tz, 0);
    if (ref) {
        nav_mesh_->removeTile(ref, nullptr, nullptr);
    }
}

} // namespace dse::navigation

#endif // DSE_ENABLE_NAVMESH
