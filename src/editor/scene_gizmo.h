#ifndef DSE_SCENE_GIZMO_H
#define DSE_SCENE_GIZMO_H

#include "component/game_object.h"
#include <glm/glm.hpp>

class SceneGizmo {
public:
    enum class Operation {
        Translate,
        Rotate,
        Scale
    };

    static void DrawGrid();
    static void DrawIcons();
    
    // Draw manipulation handles for selected object
    static void Manipulate(GameObject* target, const glm::mat4& view, const glm::mat4& projection, Operation op);

private:
    static bool RaycastHandle(const glm::vec3& origin, const glm::vec3& direction);
};

#endif // DSE_SCENE_GIZMO_H
