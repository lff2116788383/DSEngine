#include "editor_scene_camera.h"

#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace dse::editor {

glm::vec3 EditorCamera::GetPosition() const {
    float cos_pitch = std::cos(pitch);
    float sin_pitch = std::sin(pitch);
    float cos_yaw = std::cos(yaw);
    float sin_yaw = std::sin(yaw);

    glm::vec3 offset(
        cos_pitch * sin_yaw * distance,
        sin_pitch * distance,
        cos_pitch * cos_yaw * distance
    );
    return focal_point + offset;
}

glm::vec3 EditorCamera::GetForward() const {
    return glm::normalize(focal_point - GetPosition());
}

glm::vec3 EditorCamera::GetRight() const {
    glm::vec3 forward = GetForward();
    glm::vec3 world_up(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(forward, world_up));
}

glm::vec3 EditorCamera::GetUp() const {
    return glm::normalize(glm::cross(GetRight(), GetForward()));
}

glm::mat4 EditorCamera::GetViewMatrix() const {
    glm::vec3 position = GetPosition();
    return glm::lookAt(position, focal_point, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 EditorCamera::GetProjectionMatrix(float aspect_ratio) const {
    float safe_aspect = std::max(0.001f, aspect_ratio);
    return glm::perspective(glm::radians(fov), safe_aspect, near_clip, far_clip);
}

EditorCamera& GetEditorCamera() {
    static EditorCamera camera;
    return camera;
}

void ProcessEditorCameraInput(EditorCamera& camera) {
    ImGuiIO& io = ImGui::GetIO();

    // Only process input when Scene viewport is hovered
    if (!ImGui::IsWindowHovered()) {
        return;
    }

    // Right-click drag → Orbit
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        ImVec2 delta = io.MouseDelta;
        camera.yaw -= delta.x * camera.orbit_speed;
        camera.pitch += delta.y * camera.orbit_speed;
        // Clamp pitch to avoid gimbal lock
        camera.pitch = std::clamp(camera.pitch, -1.5f, 1.5f);
    }

    // Middle-click drag → Pan
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = io.MouseDelta;
        glm::vec3 right = camera.GetRight();
        glm::vec3 up = camera.GetUp();
        float pan_factor = camera.pan_speed * camera.distance * 0.1f;
        camera.focal_point -= right * delta.x * pan_factor;
        camera.focal_point += up * delta.y * pan_factor;
    }

    // Alt + Left-click drag → Orbit (Maya-style)
    if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = io.MouseDelta;
        camera.yaw -= delta.x * camera.orbit_speed;
        camera.pitch += delta.y * camera.orbit_speed;
        camera.pitch = std::clamp(camera.pitch, -1.5f, 1.5f);
    }

    // Scroll wheel → Zoom
    if (std::abs(io.MouseWheel) > 0.0f) {
        float zoom_factor = 1.0f - io.MouseWheel * 0.1f;
        camera.distance *= zoom_factor;
        camera.distance = std::clamp(camera.distance, 0.1f, 500.0f);
    }
}

void FocusEditorCamera(EditorCamera& camera, const glm::vec3& target) {
    camera.focal_point = target;
    camera.distance = 5.0f;
}

} // namespace dse::editor
