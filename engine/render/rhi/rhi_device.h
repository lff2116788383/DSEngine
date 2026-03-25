#ifndef DSE_RHI_DEVICE_H
#define DSE_RHI_DEVICE_H

#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>

struct SpriteDrawItem {
    unsigned int texture_handle = 0;
    unsigned int material_instance_id = 0;
    unsigned int shader_variant_key = 0;
    unsigned int blend_mode = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int sorting_layer = 0;
    int order_in_layer = 0;
};

using DrawBatchItem = SpriteDrawItem;

struct RenderStats {
    int sprite_count = 0;
    int draw_calls = 0;
    int max_batch_sprites = 0;
    int render_passes = 0;
};

struct RenderTargetDesc {
    int width = 0;
    int height = 0;
    bool has_depth = false;
};

struct RenderPassDesc {
    unsigned int render_target = 0;
    glm::vec4 clear_color = glm::vec4(0.0f);
    bool clear_color_enabled = false;
};

struct PipelineStateDesc {
    bool blend_enabled = true;
    unsigned int blend_src = 0x0302;
    unsigned int blend_dst = 0x0303;
};

class OpenGLRhiDevice;

// Simple Command Buffer abstraction for Phase 1
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    
    virtual void BeginRenderPass(const RenderPassDesc& render_pass) = 0;
    virtual void EndRenderPass() = 0;
    virtual void SetPipelineState(unsigned int pipeline_state_handle) = 0;
    virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
    virtual void DrawBatch(const std::vector<DrawBatchItem>& items) = 0;
    virtual void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) = 0;
    virtual void ClearColor(const glm::vec4& color) = 0;
};

class OpenGLCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override;
    void EndRenderPass() override;
    void SetPipelineState(unsigned int pipeline_state_handle) override;
    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override;
    void DrawBatch(const std::vector<DrawBatchItem>& items) override;
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    
    // For internal use by OpenGLRhiDevice
    void Execute(OpenGLRhiDevice* device);
    
private:
    struct ClearCmd { uint64_t order; glm::vec4 color; };
    struct BeginRenderPassCmd { uint64_t order; RenderPassDesc render_pass; };
    struct EndRenderPassCmd { uint64_t order; };
    struct SetPipelineStateCmd { uint64_t order; unsigned int pipeline_state_handle; };
    struct DrawBatchCmd { uint64_t order; std::vector<SpriteDrawItem> items; };
    struct CommandRef {
        uint64_t order = 0;
        int type = 0;
        size_t index = 0;
    };
    
    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 projection_ = glm::mat4(1.0f);
    uint64_t next_cmd_order_ = 0;
    std::vector<BeginRenderPassCmd> begin_render_pass_cmds_;
    std::vector<EndRenderPassCmd> end_render_pass_cmds_;
    std::vector<SetPipelineStateCmd> set_pipeline_state_cmds_;
    std::vector<ClearCmd> clear_cmds_;
    std::vector<DrawBatchCmd> draw_batch_cmds_;
};

class RhiDevice {
public:
    virtual ~RhiDevice() = default;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual unsigned int CreateRenderTarget(const RenderTargetDesc& desc) = 0;
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const = 0;
    virtual unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) = 0;
    virtual unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) = 0;
    virtual unsigned int CreatePipelineState(const PipelineStateDesc& desc) = 0;
    
    virtual unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) = 0;
    virtual void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) = 0;
    virtual void DeleteBuffer(unsigned int handle) = 0;
    virtual unsigned int CreateVertexArray() = 0;
    virtual void DeleteVertexArray(unsigned int handle) = 0;

    virtual std::shared_ptr<CommandBuffer> CreateCommandBuffer() = 0;
    virtual void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) = 0;
    virtual void EndFrame() = 0;
    virtual const RenderStats& LastFrameStats() const = 0;
};

class OpenGLRhiDevice final : public RhiDevice {
public:
    void Shutdown() override;
    void BeginFrame() override;
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index);
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index);
    void DeleteBuffer(unsigned int handle);
    unsigned int CreateVertexArray();
    void DeleteVertexArray(unsigned int handle);

    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    const RenderStats& LastFrameStats() const override;
    
    // These are kept public temporarily for the OpenGLCommandBuffer to use
    void RealBeginRenderPass(const RenderPassDesc& render_pass);
    void RealEndRenderPass();
    void RealSetPipelineState(unsigned int pipeline_state_handle);
    void RealClearColor(const glm::vec4& color);
    void RealSubmitDrawBatch(const std::vector<DrawBatchItem>& items, const glm::mat4& view, const glm::mat4& projection);
    
private:
    struct ResourceLedger {
        std::size_t textures_created = 0;
        std::size_t textures_destroyed = 0;
        std::size_t framebuffers_created = 0;
        std::size_t framebuffers_destroyed = 0;
        std::size_t shader_programs_created = 0;
        std::size_t shader_programs_destroyed = 0;
        std::size_t vertex_arrays_created = 0;
        std::size_t vertex_arrays_destroyed = 0;
        std::size_t buffers_created = 0;
        std::size_t buffers_destroyed = 0;
        std::size_t render_targets_created = 0;
        std::size_t render_targets_destroyed = 0;
        std::size_t pipeline_states_created = 0;
        std::size_t pipeline_states_destroyed = 0;
    };

    void LogResourceLedger() const;
    struct RenderTargetResource {
        RenderTargetDesc desc;
        unsigned int fbo_handle = 0;
        unsigned int color_texture_handle = 0;
        unsigned int depth_texture_handle = 0;
    };

    void EnsureInitialized();
    unsigned int next_render_target_handle_ = 320000;
    unsigned int next_texture_handle_ = 340000;
    unsigned int next_fbo_handle_ = 350000;
    unsigned int next_pipeline_state_handle_ = 330000;
    std::unordered_map<unsigned int, RenderTargetResource> render_targets_;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;
    unsigned int active_pipeline_state_ = 0;
    unsigned int active_render_target_ = 0;
    unsigned int shader_handle_ = 0;
    unsigned int vao_handle_ = 0;
    unsigned int vbo_handle_ = 0;
    unsigned int ebo_handle_ = 0;
    unsigned int white_texture_handle_ = 0;
    int uniform_texture_loc_ = -1;
    int uniform_tint_loc_ = -1;
    int uniform_vp_loc_ = -1;
    bool initialized_ = false;
    RenderStats current_frame_stats_;
    RenderStats last_frame_stats_;
    ResourceLedger resource_ledger_;
};


#endif
