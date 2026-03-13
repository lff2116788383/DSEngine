#ifndef DSE_BATCH_RENDERER_2D_H
#define DSE_BATCH_RENDERER_2D_H

#include "component/component.h"
#include <vector>
#include <glm/glm.hpp>
#include <array>

class Texture2D;
class Shader;

struct Vertex2D {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
    float texture_slot;
};

class BatchRenderer2D : public Component {
public:
    BatchRenderer2D();
    ~BatchRenderer2D();

    static void Init();
    static void Shutdown();

    static void BeginScene();
    static void EndScene();
    static void Flush();

    static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture2D* texture, const glm::vec4& color = glm::vec4(1.0f));
    static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
    static void DrawQuad(const glm::mat4& transform, Texture2D* texture, const glm::vec4& color = glm::vec4(1.0f));

    // Override Component methods
    virtual void Awake() override;
    virtual void Update() override;

private:
    static void StartBatch();
    static void NextBatch();

private:
    static const int MAX_QUADS = 10000;
    static const int MAX_VERTICES = MAX_QUADS * 4;
    static const int MAX_INDICES = MAX_QUADS * 6;
    static const int MAX_TEXTURE_SLOTS = 32;

    struct RendererData {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ibo = 0;

        unsigned int index_count = 0;
        Vertex2D* vertex_buffer_base = nullptr;
        Vertex2D* vertex_buffer_ptr = nullptr;

        std::array<Texture2D*, MAX_TEXTURE_SLOTS> texture_slots;
        unsigned int texture_slot_index = 1; // 0 = white texture

        Texture2D* white_texture = nullptr;
        Shader* shader = nullptr;

        int draw_calls = 0;
    };

    static RendererData s_Data;
};

#endif // DSE_BATCH_RENDERER_2D_H
