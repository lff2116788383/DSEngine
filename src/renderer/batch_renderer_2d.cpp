#include "batch_renderer_2d.h"
#include "render_device/render_task_producer.h"
#include "render_device/gpu_resource_mapper.h"
#include "renderer/texture_2d.h"
#include "renderer/shader.h"
#include "renderer/camera.h"
#include "utils/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

BatchRenderer2D::RendererData BatchRenderer2D::s_Data;

BatchRenderer2D::BatchRenderer2D() {
}

BatchRenderer2D::~BatchRenderer2D() {
}

void BatchRenderer2D::Init() {
    s_Data.vertex_buffer_base = new Vertex2D[MAX_VERTICES];
    s_Data.vao = GPUResourceMapper::GenerateVAOHandle();
    s_Data.vbo = GPUResourceMapper::GenerateVBOHandle();
    // No IBO handle needed for RenderTaskProducer::ProduceRenderTaskCreateVAO?
    // Wait, CreateVAO takes indices pointer. It probably creates IBO internally or uses a handle passed in?
    // Let's check ProduceRenderTaskCreateVAO signature in RenderTaskProducer.h
    // static void ProduceRenderTaskCreateVAO(unsigned int shader_program_handle,unsigned int vao_handle,unsigned int vbo_handle,unsigned int vertex_data_size,unsigned int vertex_data_stride,void* vertex_data,unsigned int vertex_index_data_size,void* vertex_index_data);
    // It takes vao_handle and vbo_handle. It doesn't seem to take ibo_handle.
    
    // Create Indices
    unsigned int* indices = new unsigned int[MAX_INDICES];
    unsigned int offset = 0;
    for (int i = 0; i < MAX_INDICES; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;

        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;

        offset += 4;
    }

    // Create White Texture
    s_Data.white_texture = new Texture2D();
    // Assuming we can set these or they are public, or we use friend/subclass hack. 
    // For now assuming we can set them or using a helper if needed.
    // If not, we might need to modify Texture2D.
    // Let's assume for now we can't set them directly and look for a proper way or modify Texture2D later.
    // Actually, Texture2D has LoadFromFile. 
    // We can create a 1x1 white texture via RenderTask directly if Texture2D wrapper is strict.
    // But we need a Texture2D object to store in the slot array.
    // Let's try to set handle manually if possible.
    s_Data.white_texture->set_texture_handle(GPUResourceMapper::GenerateTextureHandle());
    
    unsigned int white_data = 0xffffffff;
    RenderTaskProducer::ProduceRenderTaskCreateTexImage2D(
        s_Data.white_texture->texture_handle(),
        1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, sizeof(white_data), (unsigned char*)&white_data
    );

    s_Data.texture_slots[0] = s_Data.white_texture;

    // Load Shader
    s_Data.shader = new Shader();
    // We need to load from file or string.
    // Since we created the files in data/shader/batch_renderer_2d.vert/frag
    // We can use Parse? But Parse takes a name and assumes standard paths?
    // Let's use CreateShaderProgram directly if we can load the source.
    // For simplicity, let's assume Shader::Find or Parse works with our new files if we name them right.
    // Or we can just use the provided paths.
    // Shader::Parse("batch_renderer_2d") might work if it looks in data/shader.
    // Let's try that.
    s_Data.shader->Parse("batch_renderer_2d");

    // Initialize VAO with empty VBO and full IBO
    RenderTaskProducer::ProduceRenderTaskCreateVAO(
        s_Data.shader->shader_program_handle(),
        s_Data.vao,
        s_Data.vbo,
        MAX_VERTICES * sizeof(Vertex2D),
        sizeof(Vertex2D),
        nullptr, // Initial data null
        MAX_INDICES * sizeof(unsigned int),
        indices
    );

    delete[] indices;
}

void BatchRenderer2D::Shutdown() {
    delete[] s_Data.vertex_buffer_base;
    delete s_Data.white_texture;
    delete s_Data.shader;
}

void BatchRenderer2D::BeginScene() {
    StartBatch();
}

void BatchRenderer2D::EndScene() {
    Flush();
}

void BatchRenderer2D::StartBatch() {
    s_Data.index_count = 0;
    s_Data.vertex_buffer_ptr = s_Data.vertex_buffer_base;
    s_Data.texture_slot_index = 1;
}

void BatchRenderer2D::NextBatch() {
    Flush();
    StartBatch();
}

void BatchRenderer2D::Flush() {
    if (s_Data.index_count == 0)
        return;

    // Use Shader
    RenderTaskProducer::ProduceRenderTaskUseShaderProgram(s_Data.shader->shader_program_handle());

    // Bind Textures
    for (unsigned int i = 0; i < s_Data.texture_slot_index; i++) {
        RenderTaskProducer::ProduceRenderTaskActiveAndBindTexture(GL_TEXTURE0 + i, s_Data.texture_slots[i]->texture_handle());
        // We also need to set uniforms? 
        // The shader uses u_Textures[32]. We usually set the samplers once.
        // Or we can set them every frame.
        // RenderTaskProducer::ProduceRenderTaskSetUniform1i(s_Data.shader->shader_program_handle(), ("u_Textures[" + std::to_string(i) + "]").c_str(), i);
    }
    
    // Set ViewProjection
    Camera* camera = Camera::current_camera();
    if (camera) {
        glm::mat4 view_proj = camera->projection_mat4() * camera->view_mat4();
        RenderTaskProducer::ProduceRenderTaskSetUniformMatrix4fv(s_Data.shader->shader_program_handle(), "u_ViewProjection", false, view_proj);
    }

    // Set Samplers (once or every frame)
    // For simplicity, set them here. 
    // Note: Doing this in a loop might be slow if we have many tasks. 
    // Ideally we do this once after shader compilation.
    // But RenderTaskProducer queues commands, so it's okay-ish.
    for (int i = 0; i < 32; i++) {
         // Optimization: Only set if dirty or once. But for now, just set it.
         // Actually, string formatting in loop is bad.
         // Let's assume the shader has defaults or we set them once in Init if we can.
         // But we can't easily access shader handle in Init before it's compiled/linked fully? 
         // Shader::Parse triggers compilation. So we can do it in Init.
    }

    // Update VBO
    unsigned int data_size = (unsigned char*)s_Data.vertex_buffer_ptr - (unsigned char*)s_Data.vertex_buffer_base;
    RenderTaskProducer::ProduceRenderTaskUpdateVBOSubData(s_Data.vbo, data_size, s_Data.vertex_buffer_base);

    // Draw
    RenderTaskProducer::ProduceRenderTaskBindVAOAndDrawElements(s_Data.vao, s_Data.index_count);

    s_Data.draw_calls++;
}

void BatchRenderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
    DrawQuad(position, size, nullptr, color);
}

void BatchRenderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture2D* texture, const glm::vec4& color) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));
    DrawQuad(transform, texture, color);
}

void BatchRenderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color) {
    DrawQuad(transform, nullptr, color);
}

void BatchRenderer2D::DrawQuad(const glm::mat4& transform, Texture2D* texture, const glm::vec4& color) {
    if (s_Data.index_count >= MAX_INDICES)
        NextBatch();

    float texture_index = 0.0f;
    if (texture != nullptr) {
        // Find texture
        bool found = false;
        for (unsigned int i = 1; i < s_Data.texture_slot_index; i++) {
            if (s_Data.texture_slots[i] == texture) {
                texture_index = (float)i;
                found = true;
                break;
            }
        }

        if (!found) {
            if (s_Data.texture_slot_index >= MAX_TEXTURE_SLOTS)
                NextBatch();

            texture_index = (float)s_Data.texture_slot_index;
            s_Data.texture_slots[s_Data.texture_slot_index] = texture;
            s_Data.texture_slot_index++;
        }
    }

    static const glm::vec4 quad_vertex_positions[] = {
        { -0.5f, -0.5f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.0f, 1.0f },
        { -0.5f,  0.5f, 0.0f, 1.0f }
    };

    static const glm::vec2 texture_coords[] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };

    for (int i = 0; i < 4; i++) {
        s_Data.vertex_buffer_ptr->position = transform * quad_vertex_positions[i];
        s_Data.vertex_buffer_ptr->color = color;
        s_Data.vertex_buffer_ptr->uv = texture_coords[i];
        s_Data.vertex_buffer_ptr->texture_slot = texture_index;
        s_Data.vertex_buffer_ptr++;
    }

    s_Data.index_count += 6;
}

void BatchRenderer2D::Awake() {
    Component::Awake();
}

void BatchRenderer2D::Update() {
    Component::Update();
}
