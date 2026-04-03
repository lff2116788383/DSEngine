/**
 * @file render_profiler.h
 * @brief 渲染性能分析器，统计 Draw Call、顶点数、纹理内存等渲染指标
 */

#ifndef DSE_RENDER_PROFILER_H
#define DSE_RENDER_PROFILER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstddef>

namespace dse {
namespace profiler {

/**
 * @struct RenderFrameStats
 * @brief 单帧渲染统计
 */
struct RenderFrameStats {
    int draw_calls = 0;             ///< Draw Call 次数
    int triangle_count = 0;         ///< 三角形数量
    int vertex_count = 0;           ///< 顶点数量
    int sprite_count = 0;           ///< 精灵数量
    int batch_count = 0;            ///< 批次数量
    size_t texture_memory = 0;      ///< 纹理内存使用（字节）
    int texture_binds = 0;          ///< 纹理绑定次数
    int shader_switches = 0;        ///< Shader 切换次数
};

/**
 * @struct RenderAccumulatedStats
 * @brief 累计渲染统计
 */
struct RenderAccumulatedStats {
    double avg_draw_calls = 0.0;
    double avg_triangles = 0.0;
    double avg_vertices = 0.0;
    int peak_draw_calls = 0;
    int peak_triangles = 0;
    int peak_vertices = 0;
    int frame_count = 0;
    long long total_draw_calls = 0;
    long long total_triangles = 0;
    long long total_vertices = 0;
};

/**
 * @class RenderProfiler
 * @brief 渲染性能分析器
 */
class RenderProfiler {
public:
    RenderProfiler() = default;
    ~RenderProfiler() = default;

    /**
     * @brief 帧开始时重置当前帧计数器
     */
    void BeginFrame();

    /**
     * @brief 帧结束时汇总统计
     */
    void EndFrame();

    /**
     * @brief 记录一次 Draw Call
     * @param vertex_count 顶点数
     * @param triangle_count 三角形数
     */
    void RecordDrawCall(int vertex_count, int triangle_count);

    /**
     * @brief 记录一次精灵批渲染
     * @param sprite_count 精灵数量
     */
    void RecordSpriteBatch(int sprite_count);

    /**
     * @brief 记录纹理绑定
     */
    void RecordTextureBind();

    /**
     * @brief 记录 Shader 切换
     */
    void RecordShaderSwitch();

    /**
     * @brief 设置当前纹理内存使用量
     * @param bytes 纹理内存（字节）
     */
    void SetTextureMemory(size_t bytes);

    /**
     * @brief 获取当前帧统计
     */
    const RenderFrameStats& GetCurrentFrameStats() const { return current_frame_; }

    /**
     * @brief 获取累计统计
     */
    const RenderAccumulatedStats& GetAccumulatedStats() const { return accumulated_; }

    /**
     * @brief 重置所有统计
     */
    void Reset();

    /**
     * @brief 导出为 CSV
     */
    std::string ExportCSV() const;

private:
    RenderFrameStats current_frame_;            ///< 当前帧统计
    RenderFrameStats last_frame_;               ///< 上一帧统计
    RenderAccumulatedStats accumulated_;         ///< 累计统计
    mutable std::mutex mutex_;
};

} // namespace profiler
} // namespace dse

#endif
