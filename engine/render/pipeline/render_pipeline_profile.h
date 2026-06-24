#ifndef DSE_RENDER_PIPELINE_RENDER_PIPELINE_PROFILE_H
#define DSE_RENDER_PIPELINE_RENDER_PIPELINE_PROFILE_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace dse {
namespace render {

class IRenderPass;
struct RenderPassContext;

using PipelineValue = std::variant<bool, int, double, std::string>;

struct RenderPipelinePassConfig {
    std::string name;
    bool enabled = true;
    std::unordered_map<std::string, PipelineValue> params;
};

struct RenderPipelineSettings {
    bool gpu_driven = true;
    bool shadows = true;
    std::string shadow_quality = "default";
    std::string postprocess_quality = "default";
};

struct RenderPipelineProfile {
    std::string name = "ForwardPlusDefault";
    std::string source_path;
    bool loaded_from_lua = false;
    RenderPipelineSettings settings;
    std::vector<RenderPipelinePassConfig> passes;
};

struct RenderPassMetadata {
    std::string name;
    bool required = false;
    bool runtime_only = false;
    bool requires_hiz = false;
    bool requires_gpu_driven = false;
    bool requires_compute = false;
    bool requires_ssbo = false;
    bool requires_mrt = false;
};

class RenderPipelineRegistry {
public:
    using PassFactory = std::function<std::unique_ptr<IRenderPass>(RenderPassContext&)>;

    void Register(RenderPassMetadata metadata, PassFactory factory);
    void RegisterAlias(std::string alias, std::string canonical_name);

    const RenderPassMetadata* FindMetadata(const std::string& name) const;
    bool Contains(const std::string& name) const;
    std::string ResolveName(const std::string& name) const;
    std::unique_ptr<IRenderPass> Create(const std::string& name, RenderPassContext& context) const;
    const std::vector<std::string>& Order() const { return order_; }

private:
    std::unordered_map<std::string, RenderPassMetadata> metadata_;
    std::unordered_map<std::string, PassFactory> factories_;
    std::unordered_map<std::string, std::string> aliases_;
    std::vector<std::string> order_;
};

struct RenderPipelineLoadResult {
    RenderPipelineProfile profile;
    bool used_fallback = false;
    std::string message;
};

struct RenderPipelineValidationContext {
    bool editor_mode = false;
    bool hiz_available = false;
    bool gpu_driven_supported = false;
    bool compute_supported = true;
    bool ssbo_supported = true;
    int max_color_attachments = 8;
};

// Capability-declarative auto-prune (WEB_3D_BACKEND.md §2): given a pass's
// declared capability requirements and the runtime device capabilities, decide
// whether the pass must be pruned. Returns a short reason token (e.g.
// "no_compute") when the pass should be skipped, or nullptr when it can run.
// Shared by the pipeline assembly (frame_pipeline) and the diagnostics dump so
// the prune decision lives in exactly one place.
const char* RenderPassCapabilityPruneReason(const RenderPassMetadata& metadata,
                                            const RenderPipelineValidationContext& context);

const RenderPipelineRegistry& BuiltinRenderPipelineRegistry();
RenderPipelineProfile MakeForwardPlusDefaultProfile();
RenderPipelineProfile MakeForwardPlusLiteProfile();
RenderPipelineProfile MakeForward2DProfile();
RenderPipelineProfile MakeForward3DProfile();
RenderPipelineProfile MakeDebugDepthProfile();
RenderPipelineLoadResult ResolveRenderPipelineProfileFromEnvironment(const std::string& backend_name,
                                                                    const std::string& data_root);
bool ValidateRenderPipelineProfile(const RenderPipelineProfile& profile,
                                   const RenderPipelineRegistry& registry,
                                   const RenderPipelineValidationContext& context,
                                   std::string& error_message);
bool IsRenderPipelinePassEnabled(const RenderPipelineProfile& profile,
                                 const RenderPipelineRegistry& registry,
                                 const std::string& pass_name);
const PipelineValue* FindRenderPipelinePassParam(const RenderPipelineProfile& profile,
                                                 const RenderPipelineRegistry& registry,
                                                 const std::string& pass_name,
                                                 const std::string& param_name);
std::string DumpRenderPipelineProfile(const RenderPipelineProfile& profile,
                                      const RenderPipelineRegistry& registry,
                                      const RenderPipelineValidationContext& context);

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PIPELINE_RENDER_PIPELINE_PROFILE_H
