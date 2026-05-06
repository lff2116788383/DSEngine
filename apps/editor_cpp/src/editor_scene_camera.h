#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dse::editor {

struct EditorCamera {
    glm::vec3 focal_point = glm::vec3(0.0f);
    float distance = 10.0f;
    float yaw = 0.0f;     // radians
    float pitch = 0.3f;   // radians

    float fov = 45.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;

    float orbit_speed = 0.003f;
    float pan_speed = 0.01f;
    float zoom_speed = 1.2f;

    glm::vec3 GetPosition() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect_ratio) const;

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;
};

/// Global editor camera instance
EditorCamera& GetEditorCamera();

/// Process Scene viewport input (call while Scene window is hovered/focused)
void ProcessEditorCameraInput(EditorCamera& camera);

/// Focus editor camera on a world position
void FocusEditorCamera(EditorCamera& camera, const glm::vec3& target);

} // namespace dse::editor
