#ifndef DSE_PHASE1_COMPONENTS_2D_H
#define DSE_PHASE1_COMPONENTS_2D_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declaration for Box2D
class b2Body;
class b2Fixture;

struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::mat4 local_to_world = glm::mat4(1.0f);
    bool dirty = true;
};

struct SpriteRendererComponent {
    unsigned int texture_handle = 0;
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int sorting_layer = 0;
    int order_in_layer = 0;
    bool visible = true;
};

struct CameraComponent {
    bool orthographic = true;
    float orthographic_size = 5.0f;
    float near_clip = -1.0f;
    float far_clip = 1.0f;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

enum class RigidBody2DType {
    Static,
    Kinematic,
    Dynamic
};

struct RigidBody2DComponent {
    RigidBody2DType type = RigidBody2DType::Dynamic;
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    float gravity_scale = 1.0f;
    bool fixed_rotation = false;
    
    // Internal Box2D body pointer
    b2Body* runtime_body = nullptr;
};

struct BoxCollider2DComponent {
    glm::vec2 size = glm::vec2(1.0f, 1.0f);
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;
    bool is_trigger = false;
    
    // Internal Box2D fixture pointer
    b2Fixture* runtime_fixture = nullptr;
};

#endif
