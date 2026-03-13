#include "nav_mesh.h"
#include "utils/debug.h"
#include "physics/box_collider.h" // For getting geometry
#include "component/game_object.h"
#include <queue>
#include <unordered_map>

NavMesh::NavMesh() {
}

NavMesh::~NavMesh() {
}

void NavMesh::Build() {
    DEBUG_LOG_INFO("Building NavMesh...");
    polygons_.clear();

    // 1. Voxelization / Rasterization (Simplified: Just find walkable surfaces)
    // 2. Region Partitioning
    // 3. Contour Generation
    // 4. Polygon Mesh Generation
    
    // For a simple 2D game, we might just grid the world or use manual polygons.
    // Let's assume we build a simple grid graph for A*
    
    // Placeholder: Add a big square as a walkable polygon
    NavMeshPoly poly;
    poly.vertices.push_back(glm::vec3(-10, -10, 0));
    poly.vertices.push_back(glm::vec3(10, -10, 0));
    poly.vertices.push_back(glm::vec3(10, 10, 0));
    poly.vertices.push_back(glm::vec3(-10, 10, 0));
    polygons_.push_back(poly);
}

// Simple A* Heuristic
float Heuristic(const glm::vec3& a, const glm::vec3& b) {
    return glm::distance(a, b);
}

std::vector<glm::vec3> NavMesh::FindPath(const glm::vec3& start, const glm::vec3& end) {
    std::vector<glm::vec3> path;
    
    // 1. Find start polygon
    // 2. Find end polygon
    // 3. Run A* on polygon graph (Portals)
    
    // Placeholder: Direct line
    path.push_back(start);
    path.push_back(end);
    
    return path;
}

void NavMesh::OnRender() {
    // Debug draw navmesh polygons
    // Gizmos::DrawLine(...)
}
