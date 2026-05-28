#ifndef DSE_MORPH_TARGET_SYSTEM_H
#define DSE_MORPH_TARGET_SYSTEM_H

#include <cstdint>

namespace dse {

struct MorphTargetComponent;

namespace render {

class RhiDevice;

/// GPU-driven morph target (blend shape) evaluation system.
/// Dispatches a compute shader that applies weighted deltas to base vertices.
/// The output buffer can be bound to the vertex shader (similar to GPU skinning).
class MorphTargetSystem {
public:
    MorphTargetSystem() = default;
    ~MorphTargetSystem() = default;

    /// Initialize the compute shader program. Call once during engine init.
    bool Init(RhiDevice* device);

    /// Shutdown and release GPU resources.
    void Shutdown();

    /// Returns true if the system was successfully initialized and has a valid compute program.
    bool IsAvailable() const { return available_; }

    /// Upload morph target data to GPU if dirty (base vertices, deltas, weights).
    /// Should be called once per frame before Dispatch.
    void UploadIfDirty(MorphTargetComponent& comp);

    /// Dispatch the compute shader to evaluate all active morph targets.
    /// After dispatch, comp.gpu_output_buffer contains deformed vertices.
    void Dispatch(MorphTargetComponent& comp);

    /// Get the SSBO binding point for the output buffer (for vertex shader consumption).
    static constexpr int kOutputSSBOBinding = 21;

private:
    RhiDevice* device_ = nullptr;
    unsigned int compute_program_ = 0;
    bool available_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_MORPH_TARGET_SYSTEM_H
