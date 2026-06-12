#pragma once

// 可视化脚本（蓝图）→ Lua 编译器（不依赖 ImGui）。
//
// 从 editor_visual_script.cpp 的 ImGui 节点图中抽离出纯数据编译核心，便于无头单元测试。
// 支持：
//   - 事件节点（On Init / On Update）作为执行入口，沿 Flow（执行）连线生成函数体
//   - 控制流：Branch（if/else）、For Loop（数值 for + Body/Done 续接）
//   - 数据流：Math / ECS 取值 / 常量等纯数据节点以内联表达式按需求值
//   - Flow 语句节点（Set Position / Print / Create Entity 等）按执行顺序生成语句
//   - Flow 节点的数据输出（如 Create Entity 的 Entity、For Loop 的 Index）赋给局部变量

#include <string>
#include <vector>

namespace dse::editor::vs {

enum class PinType { Flow, Float, Vec3, String, Bool, Entity, Any };
enum class PinKind { Input, Output };

struct PinData {
    int id = 0;
    std::string name;
    PinType type = PinType::Any;
    PinKind kind = PinKind::Input;
    float default_float = 0.0f;
    float default_vec3[3] = {0.0f, 0.0f, 0.0f};
    std::string default_string;
    bool default_bool = false;
};

struct NodeData {
    int id = 0;
    std::string name;
    std::string category;  // Event, Math, ECS, Flow, Utility, Variable
    std::vector<PinData> inputs;
    std::vector<PinData> outputs;
    std::string code_template;  // 含 {inputN}/{outputN} 占位的 Lua 模板
};

struct LinkData {
    int id = 0;
    int from_pin = 0;  // 输出端 pin id
    int to_pin = 0;    // 输入端 pin id
};

struct Graph {
    std::vector<NodeData> nodes;
    std::vector<LinkData> links;
};

/// 将可视化脚本图编译为 Lua 源码。纯函数，无副作用，可无头测试。
std::string CompileVisualScript(const Graph& graph);

}  // namespace dse::editor::vs
