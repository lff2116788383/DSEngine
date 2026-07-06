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
    if (is_ortho) {
        float half_h = ortho_size;
        float half_w = half_h * safe_aspect;
        return glm::ortho(-half_w, half_w, -half_h, half_h, near_clip, far_clip);
    }
    return glm::perspective(glm::radians(fov), safe_aspect, near_clip, far_clip);
}

void EditorCamera::Toggle2DMode() {
    is_ortho = !is_ortho;
    if (is_ortho) {
        pitch = 1.5707963f;
        yaw = 0.0f;
        ortho_size = distance * 0.5f;
    }
}

void EditorCamera::SetViewPreset(ViewPreset preset) {
    is_ortho = true;
    switch (preset) {
        case ViewPreset::Top:    pitch =  1.5707963f; yaw = 0.0f;        break;
        case ViewPreset::Bottom: pitch = -1.5707963f; yaw = 0.0f;        break;
        case ViewPreset::Front:  pitch =  0.0f;       yaw = 0.0f;        break;
        case ViewPreset::Back:   pitch =  0.0f;       yaw = 3.1415926f;  break;
        case ViewPreset::Left:   pitch =  0.0f;       yaw = 1.5707963f;  break;
        case ViewPreset::Right:  pitch =  0.0f;       yaw = -1.5707963f; break;
    }
    ortho_size = distance * 0.5f;
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

    // Right-click drag → Orbit (disabled in ortho/2D mode)
    if (!camera.is_ortho && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
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

    // Alt + Left-click drag → Orbit (Maya-style, disabled in ortho/2D mode)
    if (!camera.is_ortho && io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
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
        if (camera.is_ortho) {
            camera.ortho_size *= zoom_factor;
            camera.ortho_size = std::clamp(camera.ortho_size, 0.1f, 500.0f);
        }
    }
}

void FocusEditorCamera(EditorCamera& camera, const glm::vec3& target) {
    camera.focal_point = target;
    camera.distance = 5.0f;
}

} // namespace dse::editor
