#include <gtest/gtest.h>

#include "engine/render/shader_graph/shader_graph_asset.h"

using namespace dse::shadergraph;

namespace {

ShaderGraphAsset MakeSampleGraph() {
    ShaderGraphAsset g;
    g.next_id = 200;

    NodeDesc out_node;
    out_node.id = 100;
    out_node.name = "PBR Output";
    out_node.category = "Output";
    out_node.pos[0] = 500.0f;
    out_node.pos[1] = 100.0f;
    out_node.header_color = 0xFF3C3CB4u;
    PinDesc base_color;
    base_color.id = 101;
    base_color.name = "Base Color";
    base_color.type = PinType::Color;
    base_color.kind = PinKind::Input;
    base_color.default_value[0] = 0.8f;
    base_color.default_value[1] = 0.7f;
    base_color.default_value[2] = 0.6f;
    base_color.default_value[3] = 1.0f;
    out_node.inputs.push_back(base_color);

    NodeDesc color_node;
    color_node.id = 110;
    color_node.name = "Color";
    color_node.category = "Constant";
    PinDesc color_out;
    color_out.id = 111;
    color_out.name = "Color";
    color_out.type = PinType::Color;
    color_out.kind = PinKind::Output;
    color_node.outputs.push_back(color_out);

    g.nodes.push_back(out_node);
    g.nodes.push_back(color_node);
    g.links.push_back({120, 111, 101});
    return g;
}

}  // namespace

TEST(ShaderGraphAsset, RoundTripPreservesGraph) {
    ShaderGraphAsset src = MakeSampleGraph();
    std::string json = SerializeShaderGraph(src);

    ShaderGraphAsset dst;
    ShaderGraphDiagnostics diag;
    ASSERT_TRUE(DeserializeShaderGraph(json, dst, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_FALSE(diag.migrated);
    EXPECT_EQ(diag.source_version, kShaderGraphSchemaVersion);

    ASSERT_EQ(dst.nodes.size(), 2u);
    EXPECT_EQ(dst.next_id, 200);
    EXPECT_EQ(dst.nodes[0].name, "PBR Output");
    EXPECT_EQ(dst.nodes[0].category, "Output");
    EXPECT_EQ(dst.nodes[0].header_color, 0xFF3C3CB4u);
    ASSERT_EQ(dst.nodes[0].inputs.size(), 1u);
    EXPECT_EQ(dst.nodes[0].inputs[0].type, PinType::Color);
    EXPECT_FLOAT_EQ(dst.nodes[0].inputs[0].default_value[0], 0.8f);
    EXPECT_FLOAT_EQ(dst.nodes[0].pos[0], 500.0f);

    ASSERT_EQ(dst.nodes[1].outputs.size(), 1u);
    EXPECT_EQ(dst.nodes[1].outputs[0].kind, PinKind::Output);

    ASSERT_EQ(dst.links.size(), 1u);
    EXPECT_EQ(dst.links[0].from_pin, 111);
    EXPECT_EQ(dst.links[0].to_pin, 101);
}

TEST(ShaderGraphAsset, SerializedFormHasVersionEnvelope) {
    std::string json = SerializeShaderGraph(MakeSampleGraph());
    EXPECT_NE(json.find("\"version\""), std::string::npos);
    EXPECT_NE(json.find("\"graph\""), std::string::npos);
}

TEST(ShaderGraphAsset, LegacyUnversionedGraphMigrates) {
    // Old editor format: bare body, no version/graph envelope.
    const char* legacy =
        "{\"next_id\":150,"
        "\"nodes\":[{\"id\":100,\"name\":\"Float\",\"category\":\"Constant\","
        "\"pos\":[10,20],\"color\":123,\"inputs\":[],"
        "\"outputs\":[{\"id\":101,\"name\":\"Value\",\"type\":\"Float\",\"kind\":1,"
        "\"default\":[0.5,0,0,0]}]}],"
        "\"links\":[]}";

    ShaderGraphAsset dst;
    ShaderGraphDiagnostics diag;
    ASSERT_TRUE(DeserializeShaderGraph(legacy, dst, diag));
    EXPECT_TRUE(diag.migrated);
    EXPECT_EQ(diag.source_version, 0);
    ASSERT_EQ(dst.nodes.size(), 1u);
    ASSERT_EQ(dst.nodes[0].outputs.size(), 1u);
    EXPECT_EQ(dst.nodes[0].outputs[0].name, "Value");
    EXPECT_FLOAT_EQ(dst.nodes[0].outputs[0].default_value[0], 0.5f);
}

TEST(ShaderGraphAsset, NewerVersionWarnsButReadsBestEffort) {
    std::string json = SerializeShaderGraph(MakeSampleGraph());
    // Bump the version above what we support.
    auto pos = json.find("\"version\":1");
    ASSERT_NE(pos, std::string::npos);
    json.replace(pos, std::string("\"version\":1").size(), "\"version\":999");

    ShaderGraphAsset dst;
    ShaderGraphDiagnostics diag;
    ASSERT_TRUE(DeserializeShaderGraph(json, dst, diag));
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.warnings.empty());
    EXPECT_EQ(dst.nodes.size(), 2u);
}

TEST(ShaderGraphAsset, ValidateRejectsDanglingLink) {
    ShaderGraphAsset g = MakeSampleGraph();
    g.links.push_back({130, 999, 101});  // 999 is not an output pin
    std::string error;
    EXPECT_FALSE(ValidateShaderGraph(g, error));
    EXPECT_FALSE(error.empty());
}

TEST(ShaderGraphAsset, ValidateRejectsDuplicatePinIds) {
    ShaderGraphAsset g = MakeSampleGraph();
    g.nodes[1].outputs[0].id = 101;  // collide with output node's input pin
    std::string error;
    EXPECT_FALSE(ValidateShaderGraph(g, error));
}

TEST(ShaderGraphAsset, ValidateAcceptsWellFormedGraph) {
    std::string error;
    EXPECT_TRUE(ValidateShaderGraph(MakeSampleGraph(), error)) << error;
}
