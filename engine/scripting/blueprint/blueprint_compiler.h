#pragma once

/**
 * @file blueprint_compiler.h
 * @brief 运行时 .dbp 加载 + 图→字节码编译（引擎侧，无 ImGui 依赖）
 *
 * 复用编辑器 .dbp（JSON）资源格式。图结构为编辑器 dse::editor::bp 图的
 * 无 ImGui 精简镜像（丢弃仅用于渲染的坐标/颜色字段），字节码编译逻辑与
 * 编辑器 ByteCodeCompiler 对齐（按节点名生成指令）。
 */

#include <string>
#include <vector>

#include "engine/scripting/blueprint/blueprint_vm.h"

namespace dse::bp {

// ─── 变量 / 引脚 / 节点（无 ImGui 版本）─────────────────────────────────────

enum class BpVarType { Bool, Int, Float, String, Vec2, Vec3, Vec4, Entity, Array };
enum class BpPinType { Flow, Bool, Int, Float, String, Vec2, Vec3, Vec4, Entity, Array, Any, Wildcard };
enum class BpPinKind { Input, Output };

BpVarType BpVarTypeFromName(const char* name);

struct BpVariable {
    std::string name;
    BpVarType type = BpVarType::Float;
    bool   default_bool = false;
    int    default_int = 0;
    float  default_float = 0.0f;
    std::string default_string;
    float  default_vec[4] = {0, 0, 0, 0};
    bool   is_exposed = false;
};

struct BpPin {
    int id = 0;
    std::string name;
    BpPinType type = BpPinType::Any;
    BpPinKind kind = BpPinKind::Input;
    float default_float = 0.0f;
    int   default_int = 0;
    bool  default_bool = false;
};

struct BpNode {
    int id = 0;
    std::string name;
    std::string category;
    std::string comment;
    std::vector<BpPin> inputs;
    std::vector<BpPin> outputs;
};

struct BpLink {
    int id = 0;
    int from_pin = 0;
    int to_pin = 0;
};

struct BpFunctionGraph {
    std::string name;
    std::vector<BpNode> nodes;
    std::vector<BpLink> links;
    std::vector<BpPin> input_params;
    bool is_pure = false;
    int next_id = 1;
};

struct BlueprintAsset {
    std::string name;
    std::string file_path;
    int version = 1;
    std::vector<BpVariable> variables;
    std::vector<BpFunctionGraph> graphs;
    std::string description;
};

// ─── 公共 API ───────────────────────────────────────────────────────────────

/// 解析 .dbp（JSON）到内存图资源。失败返回 false。
bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path);

/// 将图资源编译为字节码。CompileGenericFlowNode 依赖已注册的 extern，
/// 故调用前应确保 BlueprintVM::Get() 已注册所需 extern 函数。
CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset);

}  // namespace dse::bp
