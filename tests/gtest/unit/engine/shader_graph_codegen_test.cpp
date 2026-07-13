#include <gtest/gtest.h>

#include "engine/render/shader_graph/shader_graph_codegen.h"

using namespace dse::shadergraph;

namespace {

PinDesc MakePin(int id, const char* name, PinType type, PinKind kind) {
    PinDesc p;
    p.id = id;
    p.name = name;
    p.type = type;
    p.kind = kind;
    return p;
}

// UV -> Texture Sample -> PBR Output, plus a Checkerboard node (exercises the
// mod/fmod intrinsic remap) and a Time node.
ShaderGraphAsset MakeGraph() {
    ShaderGraphAsset g;
    g.next_id = 500;

    NodeDesc uv;
    uv.id = 10;
    uv.name = "UV";
    uv.category = "Input";
    uv.outputs.push_back(MakePin(11, "UV", PinType::Vec2, PinKind::Output));

    NodeDesc tex;
    tex.id = 20;
    tex.name = "Texture Sample";
    tex.category = "Texture";
    tex.inputs.push_back(MakePin(21, "UV", PinType::Vec2, PinKind::Input));
    tex.outputs.push_back(MakePin(22, "RGBA", PinType::Color, PinKind::Output));
    tex.outputs.push_back(MakePin(23, "R", PinType::Float, PinKind::Output));

    NodeDesc out;
    out.id = 30;
    out.name = "PBR Output";
    out.category = "Output";
    out.inputs.push_back(MakePin(31, "Base Color", PinType::Color, PinKind::Input));

    NodeDesc chk;
    chk.id = 40;
    chk.name = "Checkerboard";
    chk.category = "Procedural";
    chk.inputs.push_back(MakePin(41, "UV", PinType::Vec2, PinKind::Input));
    chk.inputs.push_back(MakePin(42, "Scale", PinType::Float, PinKind::Input));
    chk.inputs[1].default_value[0] = 4.0f;
    chk.outputs.push_back(MakePin(43, "Out", PinType::Float, PinKind::Output));

    NodeDesc t;
    t.id = 50;
    t.name = "Time";
    t.category = "Input";
    t.outputs.push_back(MakePin(51, "Time", PinType::Float, PinKind::Output));
    t.outputs.push_back(MakePin(52, "Sin", PinType::Float, PinKind::Output));

    g.nodes = {uv, tex, out, chk, t};
    g.links.push_back({60, 11, 21});   // UV -> Texture Sample.UV
    g.links.push_back({61, 22, 31});   // Texture Sample.RGBA -> PBR Output.Base Color
    return g;
}

bool Contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

}  // namespace

TEST(ShaderGraphCodegen, GlslTargetProducesGlslSource) {
    ShaderCodegenResult r = GenerateShader(MakeGraph(), ShaderTarget::GLSL);
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());

    EXPECT_TRUE(Contains(r.fragment, "#version 430"));
    EXPECT_TRUE(Contains(r.fragment, "out vec4 FragColor"));
    EXPECT_TRUE(Contains(r.fragment, "uniform sampler2D u_tex0"));
    EXPECT_TRUE(Contains(r.fragment, "texture(u_tex0, v_uv)"));
    EXPECT_TRUE(Contains(r.fragment, "FragColor ="));
    // mod for checkerboard on GLSL
    EXPECT_TRUE(Contains(r.fragment, "mod("));

    EXPECT_TRUE(Contains(r.vertex, "#version 430"));
    EXPECT_TRUE(Contains(r.vertex, "gl_Position = vp * vec4(a_pos, 1.0)"));
}

TEST(ShaderGraphCodegen, HlslTargetProducesHlslSource) {
    ShaderCodegenResult r = GenerateShader(MakeGraph(), ShaderTarget::HLSL);
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());

    EXPECT_TRUE(Contains(r.fragment, "cbuffer PerDraw : register(b0)"));
    EXPECT_TRUE(Contains(r.fragment, "Texture2D u_tex0 : register(t0)"));
    EXPECT_TRUE(Contains(r.fragment, "SamplerState u_tex0_s : register(s0)"));
    EXPECT_TRUE(Contains(r.fragment, "float4 PSMain(PSIn i) : SV_Target"));
    EXPECT_TRUE(Contains(r.fragment, "u_tex0.Sample(u_tex0_s, v_uv)"));
    EXPECT_TRUE(Contains(r.fragment, "return FragColor;"));
    // fmod for checkerboard on HLSL, and float2/float4 vector types
    EXPECT_TRUE(Contains(r.fragment, "fmod("));
    EXPECT_TRUE(Contains(r.fragment, "float4"));
    // no GLSL-isms leaked in
    EXPECT_FALSE(Contains(r.fragment, "#version"));
    EXPECT_FALSE(Contains(r.fragment, "gl_Position"));

    EXPECT_TRUE(Contains(r.vertex, "VSOut VSMain(VSIn i)"));
    EXPECT_TRUE(Contains(r.vertex, "mul(vp, float4(i.a_pos, 1.0))"));
}

TEST(ShaderGraphCodegen, UnsupportedNodeEmitsWarningNotSilentZero) {
    ShaderGraphAsset g;
    g.next_id = 100;
    NodeDesc weird;
    weird.id = 1;
    weird.name = "TotallyMadeUpNode";
    weird.category = "Custom";
    weird.outputs.push_back(MakePin(2, "Out", PinType::Float, PinKind::Output));
    g.nodes = {weird};

    ShaderCodegenResult r = GenerateShader(g, ShaderTarget::GLSL);
    EXPECT_TRUE(r.ok);
    ASSERT_FALSE(r.warnings.empty());
    EXPECT_TRUE(Contains(r.warnings[0], "TotallyMadeUpNode"));
}

TEST(ShaderGraphCodegen, InvalidGraphReportsError) {
    ShaderGraphAsset g = MakeGraph();
    g.links.push_back({70, 999, 31});  // dangling from_pin
    ShaderCodegenResult r = GenerateShader(g, ShaderTarget::GLSL);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errors.empty());
}

TEST(ShaderGraphCodegen, TargetNames) {
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::GLSL), "GLSL");
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::HLSL), "HLSL");
}
