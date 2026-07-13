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
    float default_vec[4] = {0, 0, 0, 0};
    std::string default_string;
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
    // 编辑器画布坐标（授权/可视元数据，运行时不使用，但作为共享 .dbp 契约的一部分
    // 参与序列化以支持编辑器单源往返）。
    float pos_x = 0.0f;
    float pos_y = 0.0f;
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
    std::vector<BpPin> output_params;
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
    // 授权元数据（共享 .dbp 契约的一部分，运行时不使用）。
    std::string author;
    std::vector<std::string> implemented_interfaces;
};

// ─── 公共 API ───────────────────────────────────────────────────────────────

/// 解析 .dbp（JSON）到内存图资源。失败返回 false。
bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path);

/// 将图资源编译为字节码。外部函数按名称编码并在 VM 执行时解析。
CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset);

CompiledFunction CompileFunctionGraph(const BpFunctionGraph& graph,
                                      const std::vector<BpVariable>& variables);

/// 当前字节码格式版本。CompileToByteCode 写入 CompiledBlueprint::version 的补充
/// 校验；VM 在执行前应拒绝更高的字节码版本。
constexpr int kBytecodeVersion = 1;

/// 静态校验单个编译函数：确保每条指令引用的寄存器/常量索引在界内，且相对跳转
/// 目标落在 [0, code.size()] 内。失败时填充 error 并返回 false。
/// 与 VM 的运行时越界保护互补，作为编译期安全网。
bool ValidateCompiledFunction(const CompiledFunction& fn, std::string& error);

/// 校验整个编译蓝图（版本 + 每个函数）。失败时填充 error。
bool ValidateCompiledBlueprint(const CompiledBlueprint& bp, std::string& error);

}  // namespace dse::bp
