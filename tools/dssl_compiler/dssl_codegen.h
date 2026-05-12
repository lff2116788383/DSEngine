#pragma once
#include "dssl_parser.h"
#include <string>

namespace dssl {

struct CodeGenOutput {
    std::string vert_glsl;   // 生成的 GLSL 450 vertex shader
    std::string frag_glsl;   // 生成的 GLSL 450 fragment shader
    std::string meta_json;   // uniform 元数据 JSON（给运行时 MaterialInstance 用）
    std::string error;
};

// 从解析后的 DSSL 模块生成 GLSL 450 代码
CodeGenOutput Generate(const DSSLModule& mod);

} // namespace dssl
