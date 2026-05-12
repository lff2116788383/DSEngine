#pragma once
#include <string>
#include <vector>
#include <map>

namespace dssl {

// DSSL shader 类型
enum class ShaderType { Surface, Unlit, Particle, Sky, Postprocess, Canvas };

// Uniform 条目
struct UniformDecl {
    std::string type;          // float, vec2, vec3, vec4, int, bool, sampler2D, samplerCube
    std::string name;
    std::vector<std::string> hints;  // color, range(0,1), filter_linear_mipmap, ...
    std::string default_value;       // vec4(1.0), 0.5, etc. (empty = no default)
    bool is_sampler = false;
};

// render_mode 标志
struct RenderModes {
    std::string blend       = "blend_mix";
    std::string cull        = "cull_back";
    std::string depth_draw  = "depth_draw_opaque";
    bool depth_test         = true;
    std::string diffuse     = "diffuse_burley";
    std::string specular    = "specular_schlick_ggx";
    bool shadows_enabled    = true;
    bool alpha_test         = false;
    bool wireframe          = false;
};

// 解析结果
struct DSSLModule {
    ShaderType shader_type = ShaderType::Surface;
    RenderModes render_modes;
    std::vector<UniformDecl> uniforms;
    std::string vertex_body;     // void vertex() { ... } 的函数体内容
    std::string surface_body;    // void surface() { ... }
    std::string light_body;      // void light() { ... }
    std::string postprocess_body; // void postprocess() { ... }
    std::string source_path;     // 原始 .dssl 文件路径
    std::string error;           // 解析错误信息（空 = 成功）
};

// 解析 .dssl 文件
DSSLModule Parse(const std::string& source, const std::string& filepath = "");

} // namespace dssl
