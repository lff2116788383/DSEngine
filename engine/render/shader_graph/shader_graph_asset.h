/**
 * @file shader_graph_asset.h
 * @brief 着色器图（Shader Graph）的版本化 .dshadergraph 共享资产契约
 *
 * 与编辑器 Shader Graph 面板共用同一份磁盘格式：编辑器保存的 .dshadergraph
 * 即为运行时/构建期消费的资产。此处只描述与后端无关的节点图数据（节点/引脚/
 * 连线 + 编辑器布局提示），具体 GLSL/HLSL/SPIR-V 代码生成由各后端在其之上完成。
 *
 * 序列化采用与 .dcutscene/.dasm 一致的版本包裹：{ "version": N, "graph": {...} }。
 * 旧版无包裹的裸图（version 0）按迁移读取，保持前向/后向兼容。
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/asset_diagnostics.h"
#include "engine/core/dse_export.h"

namespace dse {
namespace shadergraph {

constexpr int kShaderGraphSchemaVersion = 1;

enum class PinType { Float, Vec2, Vec3, Vec4, Color, Texture2D, Sampler };
enum class PinKind { Input, Output };

struct PinDesc {
    int id = 0;
    std::string name;
    PinType type = PinType::Float;
    PinKind kind = PinKind::Input;
    float default_value[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct NodeDesc {
    int id = 0;
    std::string name;
    std::string category;
    float pos[2] = {0.0f, 0.0f};      // 编辑器布局提示（运行时忽略）
    uint32_t header_color = 0;        // 编辑器布局提示（运行时忽略）
    std::vector<PinDesc> inputs;
    std::vector<PinDesc> outputs;
};

struct LinkDesc {
    int id = 0;
    int from_pin = 0;
    int to_pin = 0;
};

struct ShaderGraphAsset {
    int next_id = 100;
    std::vector<NodeDesc> nodes;
    std::vector<LinkDesc> links;
};

/// 收敛到共享资产诊断 DTO（形状与其他内容格式一致）。
using ShaderGraphDiagnostics = dse::assets::AssetDiagnostics;

DSE_EXPORT const char* PinTypeName(PinType type);
DSE_EXPORT PinType PinTypeFromName(const char* name);

/// 结构完整性校验：引脚 id 唯一、连线端点可解析且方向合法。
DSE_EXPORT bool ValidateShaderGraph(const ShaderGraphAsset& graph, std::string& error);

DSE_EXPORT std::string SerializeShaderGraph(const ShaderGraphAsset& graph);

DSE_EXPORT bool DeserializeShaderGraph(const std::string& json,
                                       ShaderGraphAsset& out,
                                       ShaderGraphDiagnostics& diag);

DSE_EXPORT bool SaveShaderGraphToFile(const ShaderGraphAsset& graph,
                                      const std::string& path,
                                      ShaderGraphDiagnostics& diag);

DSE_EXPORT bool LoadShaderGraphFromFile(const std::string& path,
                                        ShaderGraphAsset& out,
                                        ShaderGraphDiagnostics& diag);

}  // namespace shadergraph
}  // namespace dse
