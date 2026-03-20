#ifndef DSE_PHASE1_RHI_DEVICE_H
#define DSE_PHASE1_RHI_DEVICE_H

#include <vector>
#include <glm/glm.hpp>
#include <memory>

struct Phase1SpriteDrawItem {
    unsigned int texture_handle = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int sorting_layer = 0;
    int order_in_layer = 0;
};

// Simple Command Buffer abstraction for Phase 1
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    
    virtual void DrawSpriteBatch(const std::vector<Phase1SpriteDrawItem>& items) = 0;
    virtual void ClearColor(const glm::vec4& color) = 0;
};

class OpenGLCommandBuffer final : public CommandBuffer {
public:
    void DrawSpriteBatch(const std::vector<Phase1SpriteDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    
    // For internal use by OpenGLRhiDevice
    void Execute(OpenGLRhiDevice* device);
    
private:
    struct ClearCmd { glm::vec4 color; };
    struct DrawBatchCmd { std::vector<Phase1SpriteDrawItem> items; };
    
    std::vector<ClearCmd> clear_cmds_;
    std::vector<DrawBatchCmd> draw_batch_cmds_;
};

class RhiDevice {
public:
    virtual ~RhiDevice() = default;
    virtual void BeginFrame() = 0;
    virtual std::shared_ptr<CommandBuffer> CreateCommandBuffer() = 0;
    virtual void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) = 0;
    virtual void EndFrame() = 0;
};

class OpenGLRhiDevice final : public RhiDevice {
public:
    void BeginFrame() override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    
    // These are kept public temporarily for the OpenGLCommandBuffer to use
    void RealClearColor(const glm::vec4& color);
    void RealSubmitSpriteBatch(const std::vector<Phase1SpriteDrawItem>& items);
    
private:
    void EnsureInitialized();
    unsigned int shader_handle_ = 310001;
    unsigned int vao_handle_ = 310002;
    unsigned int vbo_handle_ = 310003;
    unsigned int white_texture_handle_ = 310004;
    bool initialized_ = false;
};

#endif
