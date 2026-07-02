#pragma once

#include "dssl_material_instance.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AssetManager;

namespace dse {
namespace render {

class DSSLMaterialLoader {
public:
    static DSSLMaterialLoader& Instance();

    // 从 .dssl 文件创建材质实例
    std::shared_ptr<DSSLMaterialInstance> LoadFromFile(const std::string& dssl_path,
                                                        AssetManager* asset_mgr = nullptr);

    // 从已有模板克隆一个实例（共享 uniform 信息，独立值）
    std::shared_ptr<DSSLMaterialInstance> CreateInstance(const std::string& dssl_path,
                                                          AssetManager* asset_mgr = nullptr);

    // 从 .dmat JSON 文件加载所有材质实例（自动选择 DSSL 模板并绑定 PBR 参数）
    std::vector<std::shared_ptr<DSSLMaterialInstance>> LoadFromDmat(
        const std::string& dmat_path, const std::string& dssl_search_dir = "",
        AssetManager* asset_mgr = nullptr);

    // 根据 ID 查找
    std::shared_ptr<DSSLMaterialInstance> GetInstance(unsigned int id);

    // 释放所有实例
    void Clear();

private:
    DSSLMaterialLoader() = default;

    unsigned int next_id_ = 900000;
    std::unordered_map<unsigned int, std::shared_ptr<DSSLMaterialInstance>> instances_;

    // 缓存已解析的模板（uniform 信息 + 默认值）
    struct DSSLTemplate {
        DSSLShaderType shader_type;
        std::vector<DSSLUniformInfo> uniform_infos;
        DSSLMaterialInstance::RenderModes render_modes;
        // 默认值
        std::unordered_map<std::string, float> default_floats;
        std::unordered_map<std::string, glm::vec4> default_vec4s;
    };
    std::unordered_map<std::string, DSSLTemplate> template_cache_;

    bool ParseDSSLForTemplate(const std::string& source, const std::string& filepath,
                               DSSLTemplate& out);
};

} // namespace render
} // namespace dse
