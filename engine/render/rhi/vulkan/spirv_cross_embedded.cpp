/**
 * @file spirv_cross_embedded.cpp
 * @brief SPIRV-Cross 源码嵌入编译单元
 *
 * 将 spirv-cross 源码直接编译进 dse_engine，避免链接静态库导致
 * WINDOWS_EXPORT_ALL_SYMBOLS 符号数超过 65535 限制（LNK1189）。
 * spirv_cross_embedded.obj 已从 objects.txt 排除，其符号不会被导出。
 */

#ifdef DSE_HAS_SPIRV_CROSS

#include <spirv_cross.cpp>
#include <spirv_cross_parsed_ir.cpp>
#include <spirv_cfg.cpp>
#include <spirv_parser.cpp>
#include <spirv_glsl.cpp>

#include "engine/base/debug.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include <map>

namespace dse {
namespace render {
namespace spirv_reflect_impl {

bool ReflectSpirvRuntime(
    const std::vector<uint32_t>& vert_spirv,
    const std::vector<uint32_t>& frag_spirv,
    std::vector<DescriptorBindingInfo>& out_bindings,
    uint32_t& out_push_constant_size,
    VkShaderStageFlags& out_push_constant_stages) {

    out_bindings.clear();
    out_push_constant_size = 0;
    out_push_constant_stages = 0;

    std::map<uint64_t, DescriptorBindingInfo> binding_map;

    auto reflect_stage = [&](const std::vector<uint32_t>& spirv, VkShaderStageFlags stage) -> bool {
        if (spirv.empty()) return true;
        try {
            spirv_cross::CompilerGLSL compiler(spirv);
            auto resources = compiler.get_shader_resources();

            auto add_binding = [&](spirv_cross::ID id, VkDescriptorType dtype, uint32_t array_count = 1) {
                uint32_t s = compiler.get_decoration(id, spv::DecorationDescriptorSet);
                uint32_t b = compiler.get_decoration(id, spv::DecorationBinding);
                uint64_t key = (uint64_t(s) << 32) | b;
                auto it = binding_map.find(key);
                if (it != binding_map.end()) {
                    it->second.stage_flags |= stage;
                } else {
                    binding_map[key] = {s, b, dtype, stage, array_count};
                }
            };

            for (auto& r : resources.uniform_buffers)
                add_binding(r.id, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            for (auto& r : resources.storage_buffers)
                add_binding(r.id, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            for (auto& r : resources.sampled_images) {
                auto& type = compiler.get_type(r.type_id);
                uint32_t cnt = type.array.empty() ? 1 : type.array[0];
                add_binding(r.id, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cnt);
            }
            for (auto& r : resources.separate_images) {
                auto& type = compiler.get_type(r.type_id);
                uint32_t cnt = type.array.empty() ? 1 : type.array[0];
                add_binding(r.id, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cnt);
            }
            for (auto& r : resources.separate_samplers)
                add_binding(r.id, VK_DESCRIPTOR_TYPE_SAMPLER);
            for (auto& pc : resources.push_constant_buffers) {
                auto& type = compiler.get_type(pc.base_type_id);
                out_push_constant_size = std::max(out_push_constant_size,
                    static_cast<uint32_t>(compiler.get_declared_struct_size(type)));
                out_push_constant_stages |= stage;
            }
            return true;
        } catch (const spirv_cross::CompilerError& e) {
            DEBUG_LOG_ERROR("[Vulkan] spirv-cross reflection error: {}", e.what());
            return false;
        }
    };

    if (!reflect_stage(vert_spirv, VK_SHADER_STAGE_VERTEX_BIT)) return false;
    if (!reflect_stage(frag_spirv, VK_SHADER_STAGE_FRAGMENT_BIT)) return false;

    for (auto& [key, info] : binding_map)
        out_bindings.push_back(info);
    return true;
}

} // namespace spirv_reflect_impl
} // namespace render
} // namespace dse

#endif // DSE_HAS_SPIRV_CROSS
