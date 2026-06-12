/**
 * @file shader_graph_compile_test.cpp
 * @brief Shader Graph 编译器逻辑测试
 *
 * 覆盖场景：
 * - 拓扑排序正确性
 * - 基本编译输出包含必要 GLSL 结构
 * - 常量节点编译
 * - 链接传播
 * - PBR Output 节点生成 FragColor
 *
 * 注意：直接包含 editor_shader_graph.cpp 中的命名空间匿名函数不可测。
 * 此测试文件复制核心编译逻辑的关键子集用于验证。
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace {

// ─── 最小化数据模型（与 editor_shader_graph.cpp 对齐）─────────────────────────

enum class PinType { Float, Vec2, Vec3, Vec4, Color, Texture2D, Sampler };
enum class PinKind { Input, Output };

struct Pin {
    int id;
    std::string name;
    PinType type;
    PinKind kind;
    float default_value[4] = {0, 0, 0, 1};
};

struct Node {
    int id;
    std::string name;
    std::string category;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
};

struct Link {
    int id;
    int from_pin;
    int to_pin;
};

struct GraphState {
    std::vector<Node> nodes;
    std::vector<Link> links;
};

// ─── 简化编译器（子集）──────────────────────────────────────────────────────────

std::string CompileTestGraph(const GraphState& s) {
    std::ostringstream o;
    o << "#version 330 core\n\n";
    o << "in vec2 v_uv;\n";
    o << "out vec4 FragColor;\n\n";
    o << "void main() {\n";

    // 拓扑排序
    std::unordered_map<int, int> pin_to_node;
    for (size_t i = 0; i < s.nodes.size(); ++i) {
        for (auto& p : s.nodes[i].inputs) pin_to_node[p.id] = static_cast<int>(i);
        for (auto& p : s.nodes[i].outputs) pin_to_node[p.id] = static_cast<int>(i);
    }
    std::unordered_map<int, std::unordered_set<int>> deps;
    for (auto& l : s.links) {
        auto it_to = pin_to_node.find(l.to_pin);
        auto it_from = pin_to_node.find(l.from_pin);
        if (it_to != pin_to_node.end() && it_from != pin_to_node.end()) {
            deps[it_to->second].insert(it_from->second);
        }
    }
    std::unordered_map<int, int> in_degree;
    for (size_t i = 0; i < s.nodes.size(); ++i) in_degree[static_cast<int>(i)] = 0;
    for (auto& [node_idx, dep_set] : deps) {
        in_degree[node_idx] = static_cast<int>(dep_set.size());
    }
    std::queue<int> q;
    for (auto& [idx, deg] : in_degree) {
        if (deg == 0) q.push(idx);
    }
    std::vector<int> topo_order;
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        topo_order.push_back(cur);
        for (size_t i = 0; i < s.nodes.size(); ++i) {
            auto it = deps.find(static_cast<int>(i));
            if (it != deps.end() && it->second.count(cur)) {
                it->second.erase(cur);
                --in_degree[static_cast<int>(i)];
                if (in_degree[static_cast<int>(i)] == 0) q.push(static_cast<int>(i));
            }
        }
    }

    std::unordered_map<int, std::string> pin_vars;
    std::unordered_map<int, int> input_source;
    for (auto& l : s.links) input_source[l.to_pin] = l.from_pin;

    for (int ni : topo_order) {
        auto& n = s.nodes[ni];
        std::string prefix = "n" + std::to_string(n.id) + "_";

        if (n.name == "Float") {
            std::string var = prefix + "val";
            o << "    float " << var << " = " << n.outputs[0].default_value[0] << ";\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Color") {
            std::string var = prefix + "col";
            o << "    vec4 " << var << " = vec4(" << n.outputs[0].default_value[0] << ", "
              << n.outputs[0].default_value[1] << ", " << n.outputs[0].default_value[2] << ", "
              << n.outputs[0].default_value[3] << ");\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Add") {
            std::string a_expr = "0.0", b_expr = "0.0";
            auto sa = input_source.find(n.inputs[0].id);
            if (sa != input_source.end() && pin_vars.count(sa->second)) a_expr = pin_vars[sa->second];
            auto sb = input_source.find(n.inputs[1].id);
            if (sb != input_source.end() && pin_vars.count(sb->second)) b_expr = pin_vars[sb->second];
            std::string var = prefix + "out";
            o << "    float " << var << " = " << a_expr << " + " << b_expr << ";\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "PBR Output") {
            auto get_input = [&](int idx, const char* fallback) -> std::string {
                if (idx >= static_cast<int>(n.inputs.size())) return fallback;
                auto src = input_source.find(n.inputs[idx].id);
                if (src != input_source.end() && pin_vars.count(src->second)) return pin_vars[src->second];
                return fallback;
            };
            std::string base_color = get_input(0, "vec4(0.8, 0.8, 0.8, 1.0)");
            o << "    FragColor = " << base_color << ";\n";
        }
    }

    o << "}\n";
    return o.str();
}

} // namespace

// ============================================================
// Tests
// ============================================================

// 测试 着色器图编译：空生成最小GLSL
TEST(ShaderGraphCompileTest, Empty_GenerateMinimalGLSL) {
    GraphState s;
    std::string glsl = CompileTestGraph(s);
    EXPECT_NE(glsl.find("#version 330 core"), std::string::npos);
    EXPECT_NE(glsl.find("void main()"), std::string::npos);
    EXPECT_NE(glsl.find("}"), std::string::npos);
}

// 测试 着色器图编译：单一Floatconstant节点生成
TEST(ShaderGraphCompileTest, SingleFloatconstantNode_Generate) {
    GraphState s;
    Node n;
    n.id = 1; n.name = "Float"; n.category = "Constant";
    Pin out; out.id = 10; out.name = "Value"; out.type = PinType::Float; out.kind = PinKind::Output;
    out.default_value[0] = 0.75f;
    n.outputs.push_back(out);
    s.nodes.push_back(n);

    std::string glsl = CompileTestGraph(s);
    EXPECT_NE(glsl.find("n1_val"), std::string::npos);
    EXPECT_NE(glsl.find("0.75"), std::string::npos);
}

// 测试 着色器图编译：Colorconstant节点Generatevec 4
TEST(ShaderGraphCompileTest, ColorconstantNode_Generatevec4) {
    GraphState s;
    Node n;
    n.id = 2; n.name = "Color"; n.category = "Constant";
    Pin out; out.id = 20; out.name = "Color"; out.type = PinType::Color; out.kind = PinKind::Output;
    out.default_value[0] = 1.0f; out.default_value[1] = 0.0f;
    out.default_value[2] = 0.0f; out.default_value[3] = 1.0f;
    n.outputs.push_back(out);
    s.nodes.push_back(n);

    std::string glsl = CompileTestGraph(s);
    EXPECT_NE(glsl.find("vec4"), std::string::npos);
    EXPECT_NE(glsl.find("n2_col"), std::string::npos);
}

// 测试 着色器图编译：添加节点Link两个浮点
TEST(ShaderGraphCompileTest, AddNode_LinkTwoFloat) {
    GraphState s;
    // Float A
    Node a; a.id = 1; a.name = "Float";
    Pin a_out; a_out.id = 10; a_out.name = "Value"; a_out.type = PinType::Float; a_out.kind = PinKind::Output;
    a_out.default_value[0] = 2.0f;
    a.outputs.push_back(a_out);
    s.nodes.push_back(a);

    // Float B
    Node b; b.id = 2; b.name = "Float";
    Pin b_out; b_out.id = 20; b_out.name = "Value"; b_out.type = PinType::Float; b_out.kind = PinKind::Output;
    b_out.default_value[0] = 3.0f;
    b.outputs.push_back(b_out);
    s.nodes.push_back(b);

    // Add
    Node add; add.id = 3; add.name = "Add";
    Pin in_a; in_a.id = 30; in_a.name = "A"; in_a.type = PinType::Float; in_a.kind = PinKind::Input;
    Pin in_b; in_b.id = 31; in_b.name = "B"; in_b.type = PinType::Float; in_b.kind = PinKind::Input;
    Pin add_out; add_out.id = 32; add_out.name = "Result"; add_out.type = PinType::Float; add_out.kind = PinKind::Output;
    add.inputs.push_back(in_a);
    add.inputs.push_back(in_b);
    add.outputs.push_back(add_out);
    s.nodes.push_back(add);

    // Links
    s.links.push_back({100, 10, 30}); // Float A -> Add.A
    s.links.push_back({101, 20, 31}); // Float B -> Add.B

    std::string glsl = CompileTestGraph(s);
    EXPECT_NE(glsl.find("n1_val"), std::string::npos);
    EXPECT_NE(glsl.find("n2_val"), std::string::npos);
    EXPECT_NE(glsl.find("n3_out"), std::string::npos);
    EXPECT_NE(glsl.find("n1_val + n2_val"), std::string::npos);
}

// 测试 着色器图编译：PBR输出默认颜色无Links
TEST(ShaderGraphCompileTest, PBROutput_DefaultColorForNoLinks) {
    GraphState s;
    Node pbr; pbr.id = 5; pbr.name = "PBR Output";
    Pin base_in; base_in.id = 50; base_in.name = "Base Color"; base_in.type = PinType::Color; base_in.kind = PinKind::Input;
    pbr.inputs.push_back(base_in);
    s.nodes.push_back(pbr);

    std::string glsl = CompileTestGraph(s);
    EXPECT_NE(glsl.find("FragColor"), std::string::npos);
    EXPECT_NE(glsl.find("vec4(0.8, 0.8, 0.8, 1.0)"), std::string::npos);
}

// 测试 着色器图编译：PBR输出Link颜色Spread颜色
TEST(ShaderGraphCompileTest, PBROutput_LinkColor_SpreadColor) {
    GraphState s;
    // Color node
    Node col; col.id = 1; col.name = "Color";
    Pin col_out; col_out.id = 10; col_out.name = "Color"; col_out.type = PinType::Color; col_out.kind = PinKind::Output;
    col_out.default_value[0] = 0.5f; col_out.default_value[1] = 0.5f;
    col_out.default_value[2] = 0.5f; col_out.default_value[3] = 1.0f;
    col.outputs.push_back(col_out);
    s.nodes.push_back(col);

    // PBR Output
    Node pbr; pbr.id = 2; pbr.name = "PBR Output";
    Pin base_in; base_in.id = 20; base_in.name = "Base Color"; base_in.type = PinType::Color; base_in.kind = PinKind::Input;
    pbr.inputs.push_back(base_in);
    s.nodes.push_back(pbr);

    // Link Color -> PBR.BaseColor
    s.links.push_back({100, 10, 20});

    std::string glsl = CompileTestGraph(s);
    EXPECT_NE(glsl.find("FragColor = n1_col"), std::string::npos);
}

// 测试 着色器图编译：情形存在Beforegenerate
TEST(ShaderGraphCompileTest, Case_ExistBeforegenerate) {
    // Color -> PBR Output
    GraphState s;
    // PBR Output 先加入 nodes（测试拓扑排序是否正确排在后面）
    Node pbr; pbr.id = 2; pbr.name = "PBR Output";
    Pin base_in; base_in.id = 20; base_in.name = "Base Color"; base_in.type = PinType::Color; base_in.kind = PinKind::Input;
    pbr.inputs.push_back(base_in);
    s.nodes.push_back(pbr);

    Node col; col.id = 1; col.name = "Color";
    Pin col_out; col_out.id = 10; col_out.name = "Color"; col_out.type = PinType::Color; col_out.kind = PinKind::Output;
    col_out.default_value[0] = 1.0f; col_out.default_value[1] = 0.0f;
    col_out.default_value[2] = 0.0f; col_out.default_value[3] = 1.0f;
    col.outputs.push_back(col_out);
    s.nodes.push_back(col);

    s.links.push_back({100, 10, 20});

    std::string glsl = CompileTestGraph(s);
    // n1_col 声明应出现在 FragColor 赋值之前
    auto pos_col = glsl.find("n1_col");
    auto pos_frag = glsl.find("FragColor =");
    ASSERT_NE(pos_col, std::string::npos);
    ASSERT_NE(pos_frag, std::string::npos);
    EXPECT_LT(pos_col, pos_frag);
}
