#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/assets/compiler/importer.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>
#include <iostream>
#include <fstream>
#include <limits>
#include <queue>
#include <unordered_map>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cctype>
#include <cstdio>
#include <cstring>



namespace dse {
namespace asset {
namespace compiler {

namespace {



std::string DecodeBase64Data(const std::string& encoded) {
    static const std::string kChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string clean;
    clean.reserve(encoded.size());
    for (unsigned char ch : encoded) {
        if (std::isalnum(ch) || ch == '+' || ch == '/' || ch == '=') {
            clean.push_back(static_cast<char>(ch));
        }
    }

    std::string out;
    out.reserve((clean.size() * 3) / 4);
    int val = 0;
    int valb = -8;
    for (unsigned char ch : clean) {
        if (ch == '=') {
            break;
        }
        const auto idx = kChars.find(static_cast<char>(ch));
        if (idx == std::string::npos) {
            return {};
        }
        val = (val << 6) + static_cast<int>(idx);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

glm::mat4 AiMatrix4x4ToGlm(const aiMatrix4x4& matrix) {
    glm::mat4 result(1.0f);
    result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
    result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
    result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
    result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
    return result;
}

bool MaterializeAsciiGltfBufferDataUris(std::string& source, const std::filesystem::path& source_file, std::vector<std::filesystem::path>& temp_files) {
    constexpr const char* kPrefix = "data:application/octet-stream;base64,";
    bool changed = false;
    std::size_t search_from = 0;
    int buffer_index = 0;
    while (true) {
        std::size_t pos = source.find(kPrefix, search_from);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t uri_begin = pos;
        const std::size_t data_begin = pos + std::strlen(kPrefix);
        const std::size_t data_end = source.find('"', data_begin);
        if (data_end == std::string::npos) {
            break;
        }

        const std::string encoded = source.substr(data_begin, data_end - data_begin);
        const std::string decoded = DecodeBase64Data(encoded);
        if (decoded.empty()) {
            search_from = data_end;
            continue;
        }

        const std::filesystem::path temp_path = source_file.parent_path() / (source_file.stem().string() + ".buffer_" + std::to_string(buffer_index++) + ".bin");
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(decoded.data(), static_cast<std::streamsize>(decoded.size()));
        if (!out.good()) {
            return false;
        }
        temp_files.push_back(temp_path);

        const std::string replacement = temp_path.filename().generic_string();
        source.replace(uri_begin, data_end - uri_begin, replacement);
        search_from = uri_begin + replacement.size();
        changed = true;
    }
    return changed;
}


std::string ResolveAssimpTexturePath(const std::filesystem::path& source_file, const aiMaterial* material, aiTextureType type) {
    if (!material) {
        return {};
    }
    aiString texture_path;
    if (material->GetTextureCount(type) == 0 || material->GetTexture(type, 0, &texture_path) != AI_SUCCESS) {
        return {};
    }
    const std::filesystem::path raw_path = texture_path.C_Str();
    if (raw_path.empty()) {
        return {};
    }
    if (raw_path.is_absolute()) {
        return raw_path.generic_string();
    }
    return (source_file.parent_path() / raw_path).lexically_normal().generic_string();
}

void BuildAssimpBoneHierarchy(const aiNode* node, int parent_index, std::unordered_map<std::string, int>& bone_index_map, RawSceneData& out_scene) {
    if (!node) {
        return;
    }
    auto it = bone_index_map.find(node->mName.C_Str());
    int current_parent_index = parent_index;
    if (it != bone_index_map.end()) {
        RawBone& bone = out_scene.skeleton[it->second];
        bone.parent_index = parent_index;
        bone.local_transform = AiMatrix4x4ToGlm(node->mTransformation);
        current_parent_index = it->second;
    }
    for (unsigned int child_index = 0; child_index < node->mNumChildren; ++child_index) {
        BuildAssimpBoneHierarchy(node->mChildren[child_index], current_parent_index, bone_index_map, out_scene);
    }
}

} // namespace

bool GltfImporter::Import(const std::string& file_path, RawSceneData& out_scene) {

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    bool ret = false;
    if (file_path.find(".glb") != std::string::npos) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, file_path);
    } else {
        std::ifstream input(file_path, std::ios::in | std::ios::binary);
        if (!input.is_open()) {
            err = "Failed to read file: " + file_path + ": File open error : " + file_path;
            ret = false;
        } else {
            const std::filesystem::path source_path = std::filesystem::path(file_path);
            std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            std::vector<std::filesystem::path> temp_files;
            const bool materialized = MaterializeAsciiGltfBufferDataUris(source, source_path, temp_files);
            const std::string base_dir = source_path.parent_path().generic_string();
            ret = loader.LoadASCIIFromString(&model, &err, &warn, source.c_str(), static_cast<unsigned int>(source.size()), base_dir.empty() ? std::string(".") : base_dir);
            for (const auto& temp_file : temp_files) {
                std::error_code ec;
                std::filesystem::remove(temp_file, ec);
            }
            (void)materialized;
        }
    }

    
    if (!warn.empty()) {
        std::cout << "GLTF Warn: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "GLTF Error: " << err << std::endl;
    }
    if (!ret) {
        return false;
    }
    
    // 提取 Materials
    for (const auto& mat : model.materials) {
        RawMaterial raw_mat;
        raw_mat.name = mat.name;
        auto base_color = mat.pbrMetallicRoughness.baseColorFactor;
        if (base_color.size() == 4) {
            raw_mat.base_color_factor = glm::vec4(base_color[0], base_color[1], base_color[2], base_color[3]);
        }
        raw_mat.metallic_factor = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
        raw_mat.roughness_factor = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);
        auto emissive = mat.emissiveFactor;
        if (emissive.size() == 3) {
            raw_mat.emissive_factor = glm::vec3(emissive[0], emissive[1], emissive[2]);
        }
        raw_mat.normal_scale = static_cast<float>(mat.normalTexture.scale);
        raw_mat.occlusion_strength = static_cast<float>(mat.occlusionTexture.strength);
        raw_mat.alpha_cutoff = static_cast<float>(mat.alphaCutoff);
        raw_mat.double_sided = mat.doubleSided;
        raw_mat.alpha_test = mat.alphaMode == "MASK";

        // 提取纹理
        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            int tex_idx = mat.pbrMetallicRoughness.baseColorTexture.index;
            if (tex_idx < model.textures.size()) {
                int img_idx = model.textures[tex_idx].source;
                if (img_idx >= 0 && img_idx < model.images.size()) {
                    raw_mat.base_color_texture = model.images[img_idx].uri;
                }
            }
        }
        if (mat.normalTexture.index >= 0) {
            int tex_idx = mat.normalTexture.index;
            if (tex_idx < model.textures.size()) {
                int img_idx = model.textures[tex_idx].source;
                if (img_idx >= 0 && img_idx < model.images.size()) {
                    raw_mat.normal_texture = model.images[img_idx].uri;
                }
            }
        }
        if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            int tex_idx = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (tex_idx < model.textures.size()) {
                int img_idx = model.textures[tex_idx].source;
                if (img_idx >= 0 && img_idx < model.images.size()) {
                    raw_mat.metallic_roughness_texture = model.images[img_idx].uri;
                }
            }
        }
        if (mat.emissiveTexture.index >= 0) {
            int tex_idx = mat.emissiveTexture.index;
            if (tex_idx < model.textures.size()) {
                int img_idx = model.textures[tex_idx].source;
                if (img_idx >= 0 && img_idx < model.images.size()) {
                    raw_mat.emissive_texture = model.images[img_idx].uri;
                }
            }
        }
        if (mat.occlusionTexture.index >= 0) {
            int tex_idx = mat.occlusionTexture.index;
            if (tex_idx < model.textures.size()) {
                int img_idx = model.textures[tex_idx].source;
                if (img_idx >= 0 && img_idx < model.images.size()) {
                    raw_mat.occlusion_texture = model.images[img_idx].uri;
                }
            }
        }

        out_scene.materials.push_back(raw_mat);
    }
    
    // 提取 Meshes
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            RawSubMesh raw_submesh;
            raw_submesh.name = mesh.name;
            raw_submesh.material_index = primitive.material >= 0 ? primitive.material : 0;
            
            // Positions
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* positions = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                
                raw_submesh.positions.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i) {
                    raw_submesh.positions.push_back(glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]));
                }
            }
            
            // Normals
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* normals = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                
                raw_submesh.normals.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i) {
                    raw_submesh.normals.push_back(glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]));
                }
            }

            // Tangents
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TANGENT")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* tangents = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                
                raw_submesh.tangents.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i) {
                    raw_submesh.tangents.push_back(glm::vec4(tangents[i * 4 + 0], tangents[i * 4 + 1], tangents[i * 4 + 2], tangents[i * 4 + 3]));
                }
            }
            
            // TexCoords
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* texcoords = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                
                raw_submesh.texcoords.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i) {
                    raw_submesh.texcoords.push_back(glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]));
                }
            }
            
            // Weights
            if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* weights = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                
                raw_submesh.joint_weights.reserve(accessor.count);
                for (size_t i = 0; i < accessor.count; ++i) {
                    raw_submesh.joint_weights.push_back(glm::vec4(weights[i * 4 + 0], weights[i * 4 + 1], weights[i * 4 + 2], weights[i * 4 + 3]));
                }
            }
            
            // Joints
            if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("JOINTS_0")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                
                raw_submesh.joint_indices.reserve(accessor.count);
                if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* joints = reinterpret_cast<const uint8_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i) {
                        raw_submesh.joint_indices.push_back(glm::ivec4(joints[i * 4 + 0], joints[i * 4 + 1], joints[i * 4 + 2], joints[i * 4 + 3]));
                    }
                } else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* joints = reinterpret_cast<const uint16_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i) {
                        raw_submesh.joint_indices.push_back(glm::ivec4(joints[i * 4 + 0], joints[i * 4 + 1], joints[i * 4 + 2], joints[i * 4 + 3]));
                    }
                }
            }
            
            // Indices
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                
                raw_submesh.indices.reserve(accessor.count);
                if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* indices = reinterpret_cast<const uint16_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i) {
                        raw_submesh.indices.push_back(indices[i]);
                    }
                } else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
                    const uint32_t* indices = reinterpret_cast<const uint32_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i) {
                        raw_submesh.indices.push_back(indices[i]);
                    }
                }
            } else {
                // Generate sequential indices
                for (size_t i = 0; i < raw_submesh.positions.size(); ++i) {
                    raw_submesh.indices.push_back(static_cast<uint32_t>(i));
                }
            }
            
            out_scene.meshes.push_back(raw_submesh);
        }
    }
    
    // 提取 Skeleton (Nodes & Inverse Bind Matrices)
    // 简单起见，我们平铺提取所有有关节数据的节点
    if (!model.skins.empty()) {
        const auto& skin = model.skins[0]; // 默认取第一个蒙皮
        out_scene.skeleton.resize(skin.joints.size());
        
        // 提取 Inverse Bind Matrices
        if (skin.inverseBindMatrices >= 0) {
            const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const float* matrixData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            
            for (size_t i = 0; i < skin.joints.size(); ++i) {
                glm::mat4 invBindMat;
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) {
                        invBindMat[c][r] = matrixData[i * 16 + c * 4 + r];
                    }
                }
                out_scene.skeleton[i].inverse_bind_matrix = invBindMat;
            }
        }
        
        // 提取节点树结构
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int node_idx = skin.joints[i];
            const auto& node = model.nodes[node_idx];
            out_scene.skeleton[i].name = node.name;
            
            // local transform
            glm::mat4 local_transform{1.0f};
            if (!node.matrix.empty()) {
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) {
                        local_transform[c][r] = static_cast<float>(node.matrix[c * 4 + r]);
                    }
                }
            } else {
                glm::vec3 t_vec{0.0f};
                glm::quat r_quat{1.0f, 0.0f, 0.0f, 0.0f};
                glm::vec3 s_vec{1.0f};
                if (!node.translation.empty()) t_vec = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
                if (!node.rotation.empty()) r_quat = glm::quat(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
                if (!node.scale.empty()) s_vec = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
                
                glm::mat4 t = glm::translate(glm::mat4(1.0f), t_vec);
                glm::mat4 r = glm::mat4_cast(r_quat);
                glm::mat4 s = glm::scale(glm::mat4(1.0f), s_vec);
                local_transform = t * r * s;
            }
            out_scene.skeleton[i].local_transform = local_transform;
            
            // 找父节点
            out_scene.skeleton[i].parent_index = -1;
            for (size_t j = 0; j < skin.joints.size(); ++j) {
                int potential_parent_idx = skin.joints[j];
                const auto& parent_node = model.nodes[potential_parent_idx];
                auto it_child = std::find(parent_node.children.begin(), parent_node.children.end(), node_idx);
                if (it_child != parent_node.children.end()) {
                    out_scene.skeleton[i].parent_index = static_cast<int>(j);
                    break;
                }
            }
        }
    }
    
    // 提取 Animations
    for (const auto& anim : model.animations) {
        RawAnimation raw_anim;
        raw_anim.name = anim.name;
        float max_time = 0.0f;
        
        for (const auto& channel : anim.channels) {
            RawAnimationChannel raw_channel;
            
            // 查找对应的骨骼索引 (target_node_index)
            int node_idx = channel.target_node;
            raw_channel.target_node_index = -1;
            if (!model.skins.empty()) {
                const auto& skin = model.skins[0];
                auto it_joint = std::find(skin.joints.begin(), skin.joints.end(), node_idx);
                if (it_joint != skin.joints.end()) {
                    raw_channel.target_node_index = static_cast<int>(std::distance(skin.joints.begin(), it_joint));
                }
            }
            
            if (raw_channel.target_node_index == -1) continue; // 不是蒙皮骨骼的动画先跳过
            
            const auto& sampler = anim.samplers[channel.sampler];
            
            // Input (Time)
            const tinygltf::Accessor& input_accessor = model.accessors[sampler.input];
            const tinygltf::BufferView& input_bufferView = model.bufferViews[input_accessor.bufferView];
            const tinygltf::Buffer& input_buffer = model.buffers[input_bufferView.buffer];
            const float* times = reinterpret_cast<const float*>(&input_buffer.data[input_bufferView.byteOffset + input_accessor.byteOffset]);
            
            raw_channel.time_keys.reserve(input_accessor.count);
            for (size_t i = 0; i < input_accessor.count; ++i) {
                raw_channel.time_keys.push_back(times[i]);
                max_time = std::max(max_time, times[i]);
            }
            
            // Output (Data)
            const tinygltf::Accessor& output_accessor = model.accessors[sampler.output];
            const tinygltf::BufferView& output_bufferView = model.bufferViews[output_accessor.bufferView];
            const tinygltf::Buffer& output_buffer = model.buffers[output_bufferView.buffer];
            const float* out_data = reinterpret_cast<const float*>(&output_buffer.data[output_bufferView.byteOffset + output_accessor.byteOffset]);
            
            if (channel.target_path == "translation") {
                raw_channel.position_keys.reserve(output_accessor.count);
                for (size_t i = 0; i < output_accessor.count; ++i) {
                    raw_channel.position_keys.push_back(glm::vec3(out_data[i * 3 + 0], out_data[i * 3 + 1], out_data[i * 3 + 2]));
                }
            } else if (channel.target_path == "rotation") {
                raw_channel.rotation_keys.reserve(output_accessor.count);
                for (size_t i = 0; i < output_accessor.count; ++i) {
                    // glTF is x, y, z, w, glm::quat constructor is w, x, y, z
                    raw_channel.rotation_keys.push_back(glm::quat(out_data[i * 4 + 3], out_data[i * 4 + 0], out_data[i * 4 + 1], out_data[i * 4 + 2]));
                }
            } else if (channel.target_path == "scale") {
                raw_channel.scale_keys.reserve(output_accessor.count);
                for (size_t i = 0; i < output_accessor.count; ++i) {
                    raw_channel.scale_keys.push_back(glm::vec3(out_data[i * 3 + 0], out_data[i * 3 + 1], out_data[i * 3 + 2]));
                }
            }
            
            raw_anim.channels.push_back(raw_channel);
        }
        
        raw_anim.duration = max_time;
        if (!raw_anim.channels.empty()) {
            out_scene.animations.push_back(raw_anim);
        }
    }
    
    return true;
}

// Interleaved vertex format for runtime
struct RuntimeVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec4 weights;
    glm::ivec4 joints;
    glm::vec4 tangent;
};

bool FbxImporter::Import(const std::string& file_path, RawSceneData& out_scene) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        file_path,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType
    );
    if (!scene || !scene->mRootNode) {
        std::cerr << "FBX Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    const std::filesystem::path source_path(file_path);
    out_scene = RawSceneData{};

    out_scene.materials.reserve(scene->mNumMaterials);
    for (unsigned int material_index = 0; material_index < scene->mNumMaterials; ++material_index) {
        const aiMaterial* material = scene->mMaterials[material_index];
        RawMaterial raw_material;
        aiString name;
        if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            raw_material.name = name.C_Str();
        }
        aiColor4D diffuse_color(1.0f, 1.0f, 1.0f, 1.0f);
        if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &diffuse_color) != AI_SUCCESS) {
            aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse_color);
        }
        raw_material.base_color_factor = glm::vec4(diffuse_color.r, diffuse_color.g, diffuse_color.b, diffuse_color.a);

        float metallic = 0.0f;
        if (aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR, &metallic) == AI_SUCCESS) {
            raw_material.metallic_factor = metallic;
        }
        float roughness = 0.5f;
        if (aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS) {
            raw_material.roughness_factor = roughness;
        }
        aiColor4D emissive_color(0.0f, 0.0f, 0.0f, 1.0f);
        if (aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissive_color) == AI_SUCCESS) {
            raw_material.emissive_factor = glm::vec3(emissive_color.r, emissive_color.g, emissive_color.b);
        }

        raw_material.base_color_texture = ResolveAssimpTexturePath(source_path, material, aiTextureType_BASE_COLOR);
        if (raw_material.base_color_texture.empty()) {
            raw_material.base_color_texture = ResolveAssimpTexturePath(source_path, material, aiTextureType_DIFFUSE);
        }
        raw_material.normal_texture = ResolveAssimpTexturePath(source_path, material, aiTextureType_NORMALS);
        raw_material.metallic_roughness_texture = ResolveAssimpTexturePath(source_path, material, aiTextureType_UNKNOWN);
        raw_material.emissive_texture = ResolveAssimpTexturePath(source_path, material, aiTextureType_EMISSIVE);
        raw_material.occlusion_texture = ResolveAssimpTexturePath(source_path, material, aiTextureType_LIGHTMAP);
        out_scene.materials.push_back(raw_material);
    }

    std::unordered_map<std::string, int> bone_index_map;
    out_scene.meshes.reserve(scene->mNumMeshes);
    for (unsigned int mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
        const aiMesh* mesh = scene->mMeshes[mesh_index];
        if (!mesh || mesh->mNumVertices == 0) {
            continue;
        }
        RawSubMesh raw_mesh;
        raw_mesh.name = mesh->mName.C_Str();
        raw_mesh.material_index = mesh->mMaterialIndex;
        raw_mesh.positions.reserve(mesh->mNumVertices);
        raw_mesh.normals.reserve(mesh->mNumVertices);
        raw_mesh.tangents.reserve(mesh->mNumVertices);
        raw_mesh.texcoords.reserve(mesh->mNumVertices);
        raw_mesh.joint_indices.assign(mesh->mNumVertices, glm::ivec4(0));
        raw_mesh.joint_weights.assign(mesh->mNumVertices, glm::vec4(0.0f));

        for (unsigned int vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index) {
            const aiVector3D& position = mesh->mVertices[vertex_index];
            raw_mesh.positions.emplace_back(position.x, position.y, position.z);
            if (mesh->HasNormals()) {
                const aiVector3D& normal = mesh->mNormals[vertex_index];
                raw_mesh.normals.emplace_back(normal.x, normal.y, normal.z);
            }
            if (mesh->HasTangentsAndBitangents()) {
                const aiVector3D& tangent = mesh->mTangents[vertex_index];
                raw_mesh.tangents.emplace_back(tangent.x, tangent.y, tangent.z, 1.0f);
            }
            if (mesh->HasTextureCoords(0)) {
                const aiVector3D& texcoord = mesh->mTextureCoords[0][vertex_index];
                raw_mesh.texcoords.emplace_back(texcoord.x, texcoord.y);
            }
        }

        for (unsigned int face_index = 0; face_index < mesh->mNumFaces; ++face_index) {
            const aiFace& face = mesh->mFaces[face_index];
            for (unsigned int index = 0; index < face.mNumIndices; ++index) {
                raw_mesh.indices.push_back(face.mIndices[index]);
            }
        }

        for (unsigned int bone_offset = 0; bone_offset < mesh->mNumBones; ++bone_offset) {
            const aiBone* bone = mesh->mBones[bone_offset];
            if (!bone) {
                continue;
            }
            const std::string bone_name = bone->mName.C_Str();
            int bone_index = -1;
            auto it = bone_index_map.find(bone_name);
            if (it == bone_index_map.end()) {
                bone_index = static_cast<int>(out_scene.skeleton.size());
                bone_index_map.emplace(bone_name, bone_index);
                RawBone raw_bone;
                raw_bone.name = bone_name;
                raw_bone.inverse_bind_matrix = AiMatrix4x4ToGlm(bone->mOffsetMatrix);
                out_scene.skeleton.push_back(raw_bone);
            } else {
                bone_index = it->second;
            }
            for (unsigned int weight_index = 0; weight_index < bone->mNumWeights; ++weight_index) {
                const aiVertexWeight& weight = bone->mWeights[weight_index];
                glm::ivec4& joint_indices = raw_mesh.joint_indices[weight.mVertexId];
                glm::vec4& joint_weights = raw_mesh.joint_weights[weight.mVertexId];
                for (int slot = 0; slot < 4; ++slot) {
                    if (joint_weights[slot] == 0.0f) {
                        joint_indices[slot] = bone_index;
                        joint_weights[slot] = weight.mWeight;
                        break;
                    }
                }
            }
        }

        out_scene.meshes.push_back(std::move(raw_mesh));
    }

    BuildAssimpBoneHierarchy(scene->mRootNode, -1, bone_index_map, out_scene);

    out_scene.animations.reserve(scene->mNumAnimations);
    for (unsigned int animation_index = 0; animation_index < scene->mNumAnimations; ++animation_index) {
        const aiAnimation* animation = scene->mAnimations[animation_index];
        if (!animation) {
            continue;
        }
        RawAnimation raw_animation;
        raw_animation.name = animation->mName.C_Str();
        const float ticks_per_second = animation->mTicksPerSecond > 0.0 ? static_cast<float>(animation->mTicksPerSecond) : 1.0f;
        raw_animation.duration = static_cast<float>(animation->mDuration / ticks_per_second);
        for (unsigned int channel_index = 0; channel_index < animation->mNumChannels; ++channel_index) {
            const aiNodeAnim* channel = animation->mChannels[channel_index];
            if (!channel) {
                continue;
            }
            auto bone_it = bone_index_map.find(channel->mNodeName.C_Str());
            if (bone_it == bone_index_map.end()) {
                continue;
            }
            RawAnimationChannel raw_channel;
            raw_channel.target_node_index = bone_it->second;
            for (unsigned int key_index = 0; key_index < channel->mNumPositionKeys; ++key_index) {
                raw_channel.time_keys.push_back(static_cast<float>(channel->mPositionKeys[key_index].mTime / ticks_per_second));
                const aiVector3D& value = channel->mPositionKeys[key_index].mValue;
                raw_channel.position_keys.emplace_back(value.x, value.y, value.z);
            }
            for (unsigned int key_index = 0; key_index < channel->mNumRotationKeys; ++key_index) {
                const aiQuaternion& value = channel->mRotationKeys[key_index].mValue;
                raw_channel.rotation_keys.emplace_back(value.w, value.x, value.y, value.z);
            }
            for (unsigned int key_index = 0; key_index < channel->mNumScalingKeys; ++key_index) {
                const aiVector3D& value = channel->mScalingKeys[key_index].mValue;
                raw_channel.scale_keys.emplace_back(value.x, value.y, value.z);
            }
            raw_animation.channels.push_back(std::move(raw_channel));
        }
        if (!raw_animation.channels.empty()) {
            out_scene.animations.push_back(std::move(raw_animation));
        }
    }

    return !out_scene.meshes.empty();
}


bool MeshCooker::CookToDmesh(const RawSceneData& scene, const std::string& output_path) {

    if (scene.meshes.empty()) return false;
    
    std::ofstream out(output_path, std::ios::binary);
    if (!out) return false;
    
    MeshHeader header;
    header.version = 1;
    header.attribute_mask = static_cast<uint32_t>(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::Tangent | VertexAttribute::TexCoord | VertexAttribute::Joints | VertexAttribute::Weights);
    header.submesh_count = static_cast<uint32_t>(scene.meshes.size());
    
    std::vector<RuntimeVertex> all_vertices;
    std::vector<uint32_t> all_indices;
    std::vector<SubMeshDesc> submeshes;
    
    for (const auto& mesh : scene.meshes) {
        SubMeshDesc desc;
        desc.index_start = static_cast<uint32_t>(all_indices.size());
        desc.index_count = static_cast<uint32_t>(mesh.indices.size());
        desc.base_vertex = static_cast<uint32_t>(all_vertices.size());
        desc.material_id = mesh.material_index;
        
        glm::vec3 bmin(std::numeric_limits<float>::max());
        glm::vec3 bmax(std::numeric_limits<float>::lowest());
        
        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            RuntimeVertex v;
            v.position = mesh.positions[i];
            bmin = glm::min(bmin, v.position);
            bmax = glm::max(bmax, v.position);
            
            if (i < mesh.normals.size()) v.normal = mesh.normals[i];
            else v.normal = glm::vec3(0, 1, 0);
            
            if (i < mesh.texcoords.size()) v.texcoord = mesh.texcoords[i];
            else v.texcoord = glm::vec2(0, 0);
            
            if (i < mesh.joint_weights.size()) v.weights = mesh.joint_weights[i];
            else v.weights = glm::vec4(1, 0, 0, 0);
            
            if (i < mesh.joint_indices.size()) v.joints = mesh.joint_indices[i];
            else v.joints = glm::ivec4(0, 0, 0, 0);

            if (i < mesh.tangents.size()) v.tangent = mesh.tangents[i];
            else v.tangent = glm::vec4(1, 0, 0, 1); // Default tangent X-axis
            
            all_vertices.push_back(v);
        }
        
        for (auto idx : mesh.indices) {
            all_indices.push_back(idx);
        }
        
        desc.bounding_box_min = bmin;
        desc.bounding_box_max = bmax;
        submeshes.push_back(desc);
    }
    
    header.vertex_count = static_cast<uint32_t>(all_vertices.size());
    header.index_count = static_cast<uint32_t>(all_indices.size());
    
    uint64_t current_offset = sizeof(MeshHeader);
    header.submesh_data_offset = current_offset;
    current_offset += submeshes.size() * sizeof(SubMeshDesc);
    
    header.vertex_data_offset = current_offset;
    current_offset += all_vertices.size() * sizeof(RuntimeVertex);
    
    header.index_data_offset = current_offset;
    
    // Write
    out.write(reinterpret_cast<const char*>(&header), sizeof(MeshHeader));
    out.write(reinterpret_cast<const char*>(submeshes.data()), submeshes.size() * sizeof(SubMeshDesc));
    out.write(reinterpret_cast<const char*>(all_vertices.data()), all_vertices.size() * sizeof(RuntimeVertex));
    out.write(reinterpret_cast<const char*>(all_indices.data()), all_indices.size() * sizeof(uint32_t));
    
    return true;
}

bool MeshCooker::CookToDmat(const RawSceneData& scene, const std::string& output_dir, const std::string& base_name) {
    if (scene.materials.empty()) return false;
    
    // We'll write a simple JSON array of materials for runtime
    std::string dmat_path = output_dir + "/" + base_name + ".dmat";
    std::ofstream out(dmat_path);
    if (!out) return false;
    
    out << "{\n  \"materials\": [\n";
    for (size_t i = 0; i < scene.materials.size(); ++i) {
        const auto& mat = scene.materials[i];
        out << "    {\n";
        out << "      \"name\": \"" << mat.name << "\",\n";
        out << "      \"base_color\": [" << mat.base_color_factor.r << ", " 
                                       << mat.base_color_factor.g << ", " 
                                       << mat.base_color_factor.b << ", " 
                                       << mat.base_color_factor.a << "],\n";
        out << "      \"metallic\": " << mat.metallic_factor << ",\n";
        out << "      \"roughness\": " << mat.roughness_factor << ",\n";
        out << "      \"emissive\": [" << mat.emissive_factor.r << ", "
                                     << mat.emissive_factor.g << ", "
                                     << mat.emissive_factor.b << "],\n";
        out << "      \"normal_scale\": " << mat.normal_scale << ",\n";
        out << "      \"occlusion_strength\": " << mat.occlusion_strength << ",\n";
        out << "      \"alpha_cutoff\": " << mat.alpha_cutoff << ",\n";
        out << "      \"double_sided\": " << (mat.double_sided ? "true" : "false") << ",\n";
        out << "      \"alpha_test\": " << (mat.alpha_test ? "true" : "false") << ",\n";
        out << "      \"base_color_texture\": \"" << mat.base_color_texture << "\",\n";
        out << "      \"normal_texture\": \"" << mat.normal_texture << "\",\n";
        out << "      \"metallic_roughness_texture\": \"" << mat.metallic_roughness_texture << "\",\n";
        out << "      \"emissive_texture\": \"" << mat.emissive_texture << "\",\n";
        out << "      \"occlusion_texture\": \"" << mat.occlusion_texture << "\"\n";
        out << "    }";
        if (i < scene.materials.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    
    return true;
}

bool MeshCooker::CookToDanim(const RawSceneData& scene, const std::string& output_dir, const std::string& base_name) {
    if (scene.animations.empty()) return false;
    
    // 我们目前只烘焙第一个动画作为示例
    const auto& anim = scene.animations[0];
    std::string danim_path = output_dir + "/" + base_name + ".danim";
    std::ofstream out(danim_path, std::ios::binary);
    if (!out) return false;
    
    AnimHeader header;
    header.version = 1;
    header.duration = anim.duration;
    header.channel_count = static_cast<uint32_t>(anim.channels.size());
    
    std::vector<AnimChannelDesc> channel_descs;
    std::vector<float> all_times;
    std::vector<glm::vec3> all_positions;
    std::vector<glm::quat> all_rotations;
    std::vector<glm::vec3> all_scales;
    
    for (const auto& ch : anim.channels) {
        AnimChannelDesc desc_item;
        desc_item.target_node_index = ch.target_node_index;
        
        desc_item.position_key_count = static_cast<uint32_t>(ch.position_keys.size());
        desc_item.rotation_key_count = static_cast<uint32_t>(ch.rotation_keys.size());
        desc_item.scale_key_count = static_cast<uint32_t>(ch.scale_keys.size());
        
        desc_item.time_offset = all_times.size() * sizeof(float);
        for (float t : ch.time_keys) all_times.push_back(t);
        
        desc_item.position_offset = all_positions.size() * sizeof(glm::vec3);
        for (const auto& p : ch.position_keys) all_positions.push_back(p);
        
        desc_item.rotation_offset = all_rotations.size() * sizeof(glm::quat);
        for (const auto& r : ch.rotation_keys) all_rotations.push_back(r);
        
        desc_item.scale_offset = all_scales.size() * sizeof(glm::vec3);
        for (const auto& s : ch.scale_keys) all_scales.push_back(s);
        
        channel_descs.push_back(desc_item);
    }
    
    // 修正偏移量，加上前面的 Header 和 Descs 大小
    uint64_t base_offset = sizeof(AnimHeader) + channel_descs.size() * sizeof(AnimChannelDesc);
    uint64_t current_offset = base_offset;
    
    for (auto& desc_item : channel_descs) {
        if (desc_item.time_offset > 0 || !all_times.empty()) desc_item.time_offset += current_offset;
    }
    current_offset += all_times.size() * sizeof(float);
    
    for (auto& desc_item : channel_descs) {
        if (desc_item.position_offset > 0 || !all_positions.empty()) desc_item.position_offset += current_offset;
    }
    current_offset += all_positions.size() * sizeof(glm::vec3);
    
    for (auto& desc_item : channel_descs) {
        if (desc_item.rotation_offset > 0 || !all_rotations.empty()) desc_item.rotation_offset += current_offset;
    }
    current_offset += all_rotations.size() * sizeof(glm::quat);
    
    for (auto& desc_item : channel_descs) {
        if (desc_item.scale_offset > 0 || !all_scales.empty()) desc_item.scale_offset += current_offset;
    }
    
    // Write
    out.write(reinterpret_cast<const char*>(&header), sizeof(AnimHeader));
    out.write(reinterpret_cast<const char*>(channel_descs.data()), channel_descs.size() * sizeof(AnimChannelDesc));
    if (!all_times.empty()) out.write(reinterpret_cast<const char*>(all_times.data()), all_times.size() * sizeof(float));
    if (!all_positions.empty()) out.write(reinterpret_cast<const char*>(all_positions.data()), all_positions.size() * sizeof(glm::vec3));
    if (!all_rotations.empty()) out.write(reinterpret_cast<const char*>(all_rotations.data()), all_rotations.size() * sizeof(glm::quat));
    if (!all_scales.empty()) out.write(reinterpret_cast<const char*>(all_scales.data()), all_scales.size() * sizeof(glm::vec3));
    
    return true;
}

bool MeshCooker::CookToDskel(const RawSceneData& scene, const std::string& output_dir, const std::string& base_name) {
    if (scene.skeleton.empty()) return false;
    
    std::string dskel_path = output_dir + "/" + base_name + ".dskel";
    std::ofstream out(dskel_path, std::ios::binary);
    if (!out) return false;
    
    SkelHeader header;
    header.version = 1;
    header.bone_count = static_cast<uint32_t>(scene.skeleton.size());
    
    std::vector<BoneDesc> bone_descs;
    bone_descs.reserve(scene.skeleton.size());
    for (const auto& bone : scene.skeleton) {
        BoneDesc desc;
        desc.parent_index = bone.parent_index;
        desc.inverse_bind_matrix = bone.inverse_bind_matrix;
        desc.local_transform = bone.local_transform;
        bone_descs.push_back(desc);
    }
    
    out.write(reinterpret_cast<const char*>(&header), sizeof(SkelHeader));
    out.write(reinterpret_cast<const char*>(bone_descs.data()), bone_descs.size() * sizeof(BoneDesc));
    
    return true;
}

} // namespace compiler
} // namespace asset
} // namespace dse