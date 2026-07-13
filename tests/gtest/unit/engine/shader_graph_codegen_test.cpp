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

TEST(ShaderGraphCodegen, ExtendedNodesHaveCodegenOnBothTargets) {
    ShaderGraphAsset g;
    g.next_id = 300;
    NodeDesc vor;
    vor.id = 1;
    vor.name = "Noise Voronoi";
    vor.inputs.push_back(MakePin(2, "UV", PinType::Vec2, PinKind::Input));
    vor.inputs.push_back(MakePin(3, "Scale", PinType::Float, PinKind::Input));
    vor.inputs[1].default_value[0] = 5.0f;
    vor.outputs.push_back(MakePin(4, "Dist", PinType::Float, PinKind::Output));

    NodeDesc rmp;
    rmp.id = 10;
    rmp.name = "Remap";
    for (int i = 0; i < 5; ++i)
        rmp.inputs.push_back(MakePin(11 + i, "in", PinType::Float, PinKind::Input));
    rmp.inputs[2].default_value[0] = 1.0f;
    rmp.inputs[4].default_value[0] = 1.0f;
    rmp.outputs.push_back(MakePin(20, "Out", PinType::Float, PinKind::Output));

    g.nodes = {vor, rmp};

    ShaderCodegenResult gl = GenerateShader(g, ShaderTarget::GLSL);
    ASSERT_TRUE(gl.ok);
    EXPECT_TRUE(gl.warnings.empty());
    EXPECT_TRUE(Contains(gl.fragment, "fract("));

    ShaderCodegenResult hl = GenerateShader(g, ShaderTarget::HLSL);
    ASSERT_TRUE(hl.ok);
    EXPECT_TRUE(hl.warnings.empty());
    EXPECT_TRUE(Contains(hl.fragment, "frac("));   // remapped intrinsic
    EXPECT_TRUE(Contains(hl.fragment, "float2"));
}

TEST(ShaderGraphCodegen, ScreenPositionWarnsOnHlsl) {
    ShaderGraphAsset g;
    g.next_id = 100;
    NodeDesc sp;
    sp.id = 1;
    sp.name = "Screen Position";
    sp.outputs.push_back(MakePin(2, "Out", PinType::Vec2, PinKind::Output));
    g.nodes = {sp};

    ShaderCodegenResult gl = GenerateShader(g, ShaderTarget::GLSL);
    EXPECT_TRUE(gl.ok);
    EXPECT_TRUE(gl.warnings.empty());
    EXPECT_TRUE(Contains(gl.fragment, "gl_FragCoord"));

    ShaderCodegenResult hl = GenerateShader(g, ShaderTarget::HLSL);
    EXPECT_TRUE(hl.ok);
    EXPECT_FALSE(hl.warnings.empty());
    EXPECT_FALSE(Contains(hl.fragment, "gl_FragCoord"));
}

TEST(ShaderGraphCodegen, VulkanTargetProducesVulkanGlsl) {
    ShaderCodegenResult r = GenerateShader(MakeGraph(), ShaderTarget::GLSL_VULKAN);
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());

    // Vulkan GLSL: #version 450, explicit varying/output locations, UBO-wrapped u_time,
    // explicit sampler binding (binding 0 reserved for the PerDraw UBO).
    EXPECT_TRUE(Contains(r.fragment, "#version 450"));
    EXPECT_TRUE(Contains(r.fragment, "layout(location = 0) in vec2 v_uv"));
    EXPECT_TRUE(Contains(r.fragment, "layout(location = 5) in vec4 v_color"));
    EXPECT_TRUE(Contains(r.fragment, "layout(location = 0) out vec4 FragColor"));
    EXPECT_TRUE(Contains(r.fragment, "uniform PerDraw { float u_time; }"));
    EXPECT_TRUE(Contains(r.fragment, "layout(binding = 1) uniform sampler2D u_tex0"));
    EXPECT_TRUE(Contains(r.fragment, "texture(u_tex0, v_uv)"));
    // u_time must not leak as a free uniform on Vulkan.
    EXPECT_FALSE(Contains(r.fragment, "uniform float u_time"));

    EXPECT_TRUE(Contains(r.vertex, "#version 450"));
    EXPECT_TRUE(Contains(r.vertex, "layout(location = 0) out vec2 v_uv"));
}

TEST(ShaderGraphCodegen, WebGl2TargetProducesEsGlsl) {
    ShaderCodegenResult r = GenerateShader(MakeGraph(), ShaderTarget::GLSL_ES);
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());

    // WebGL2 == GLSL ES 3.00: version pragma + precision, name-matched varyings
    // (no explicit varying locations), free uniforms are legal.
    EXPECT_TRUE(Contains(r.fragment, "#version 300 es"));
    EXPECT_TRUE(Contains(r.fragment, "precision highp float;"));
    EXPECT_TRUE(Contains(r.fragment, "uniform float u_time;"));
    EXPECT_TRUE(Contains(r.fragment, "uniform sampler2D u_tex0;"));
    EXPECT_TRUE(Contains(r.fragment, "texture(u_tex0, v_uv)"));
    EXPECT_FALSE(Contains(r.fragment, "layout(location = 0) in vec2 v_uv"));

    EXPECT_TRUE(Contains(r.vertex, "#version 300 es"));
    EXPECT_TRUE(Contains(r.vertex, "precision highp float;"));
    // ES 3.00 has no binding layout qualifier on uniform blocks.
    EXPECT_TRUE(Contains(r.vertex, "layout(std140) uniform PerFrame"));
}

TEST(ShaderGraphCodegen, WgslTargetProducesWgslSource) {
    ShaderCodegenResult r = GenerateShader(MakeGraph(), ShaderTarget::WGSL);
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());

    // WGSL: struct I/O with @location attributes, @group/@binding resources,
    // textureSample(tex, tex_sampler, uv), var-form declarations, returns FragColor.
    EXPECT_TRUE(Contains(r.fragment, "@group(0) @binding(0) var<uniform> uni : PerDraw;"));
    EXPECT_TRUE(Contains(r.fragment, "u_time : f32,"));
    EXPECT_TRUE(Contains(r.fragment, "@group(0) @binding(1) var u_tex0 : texture_2d<f32>;"));
    EXPECT_TRUE(Contains(r.fragment, "@group(0) @binding(2) var u_tex0_sampler : sampler;"));
    EXPECT_TRUE(Contains(r.fragment, "@fragment"));
    EXPECT_TRUE(Contains(r.fragment, "fn main(fs : FSIn) -> @location(0) vec4<f32>"));
    EXPECT_TRUE(Contains(r.fragment, "textureSample(u_tex0, u_tex0_sampler, v_uv)"));
    EXPECT_TRUE(Contains(r.fragment, "var u_time = uni.u_time;"));
    EXPECT_TRUE(Contains(r.fragment, "return FragColor;"));
    // WGSL has no mod() intrinsic: checkerboard must be expanded, not call mod(/fmod(.
    EXPECT_FALSE(Contains(r.fragment, "mod("));
    EXPECT_FALSE(Contains(r.fragment, "fmod("));
    // No GLSL/HLSL-isms leaked in.
    EXPECT_FALSE(Contains(r.fragment, "#version"));
    EXPECT_FALSE(Contains(r.fragment, "gl_FragCoord"));
    EXPECT_FALSE(Contains(r.fragment, "SamplerState"));

    EXPECT_TRUE(Contains(r.vertex, "@vertex"));
    EXPECT_TRUE(Contains(r.vertex, "@group(0) @binding(0) var<uniform> frame : PerFrame;"));
    EXPECT_TRUE(Contains(r.vertex, "o.pos = frame.vp * vec4<f32>(vin.a_pos, 1.0);"));
}

// Nodes needing WGSL-specific lowering (multi-component swizzle assign / C-style
// loops) must warn + emit a safe default rather than emit invalid WGSL.
TEST(ShaderGraphCodegen, WgslDivergentNodesWarnAndDegrade) {
    ShaderGraphAsset g;
    g.next_id = 200;
    NodeDesc vor;
    vor.id = 1;
    vor.name = "Noise Voronoi";
    vor.inputs.push_back(MakePin(2, "UV", PinType::Vec2, PinKind::Input));
    vor.inputs.push_back(MakePin(3, "Scale", PinType::Float, PinKind::Input));
    vor.inputs[1].default_value[0] = 5.0f;
    vor.outputs.push_back(MakePin(4, "Dist", PinType::Float, PinKind::Output));
    g.nodes = {vor};

    ShaderCodegenResult r = GenerateShader(g, ShaderTarget::WGSL);
    ASSERT_TRUE(r.ok);
    ASSERT_FALSE(r.warnings.empty());
    EXPECT_TRUE(Contains(r.warnings[0], "Noise Voronoi"));
    // Degraded path stays valid WGSL: no C-style for-loop / int cast leaked in.
    EXPECT_FALSE(Contains(r.fragment, "for (int"));
    EXPECT_FALSE(Contains(r.fragment, "float("));

    // The same graph is fully supported (no warning) on GLSL.
    ShaderCodegenResult gl = GenerateShader(g, ShaderTarget::GLSL);
    EXPECT_TRUE(gl.warnings.empty());
}

TEST(ShaderGraphCodegen, TargetNames) {
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::GLSL), "GLSL");
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::HLSL), "HLSL");
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::GLSL_VULKAN), "GLSL-Vulkan");
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::GLSL_ES), "GLSL-ES");
    EXPECT_STREQ(ShaderTargetName(ShaderTarget::WGSL), "WebGPU-WGSL");
}
