#ifndef DSE_NAV_MESH_H
#define DSE_NAV_MESH_H

#include <vector>
#include <glm/glm.hpp>
#include "component/component.h"

struct NavMeshPoly {
    std::vector<glm::vec3> vertices;
    // Neighbors, etc.
};

class NavMesh : public Component {
public:
    NavMesh();
    ~NavMesh();

    void Build();
    
    std::vector<glm::vec3> FindPath(const glm::vec3& start, const glm::vec3& end);

    virtual void OnRender(); // Debug draw

private:
    std::vector<NavMeshPoly> polygons_;
};

#endif // DSE_NAV_MESH_H
