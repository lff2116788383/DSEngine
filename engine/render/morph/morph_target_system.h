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
    ///
    /// @param base_vertices  Interleaved mesh vertex floats (the mesh's CPU
    ///        vertex buffer): position at [0..2], normal at [3..5], and, when
    ///        stride_floats >= 10, tangent at [6..9]. Required to (re)create the
    ///        base-vertex SSBO; may be null on later calls once the base buffer
    ///        exists and only weights are dirty.
    /// @param vertex_stride_floats  Number of floats per vertex in base_vertices
    ///        (20 for .dmesh v1, 24 for v2). Ignored when base_vertices is null.
    void UploadIfDirty(MorphTargetComponent& comp,
                       const float* base_vertices = nullptr,
                       int vertex_stride_floats = 0);

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
