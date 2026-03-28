# DSEngine 3D 资产导入管线技术设计草案

## 1. 核心思想与架构分层
**核心原则**：拥抱开源解析器，自研核心数据格式。
不把外部格式（FBX/GLTF）的复杂性带入引擎运行时。所有外部资产必须经过离线/开发期转换，变成引擎专属的 `.dmesh`, `.dmat`, `.danim` 格式。

### 架构分层
整个管线分为四个核心阶段（Stage）：
1. **Importer (解析层)**
   - 职责：读取磁盘上的原始文件（OBJ/GLTF/FBX），将其解析为内存中的原始数据树。
   - 依赖：`tinygltf` (处理 .gltf/.glb)，`Assimp` (处理 .fbx/.obj 及其他遗留格式)。
2. **Normalizer (规范层 / 语义统一层)**
   - 职责：消除不同格式带来的坐标系差异（如 Y-up vs Z-up）、法线空间差异、材质模型差异（Phong 转 PBR），统一输出为引擎的 `RawSceneData` 中间结构。
3. **Cooker (烘焙层 / 序列化层)**
   - 职责：对网格进行顶点优化（交错、索引压缩）、计算切线空间、生成 LOD、烘焙为引擎私有的二进制格式。
4. **RuntimeLoader (运行时加载层)**
   - 职责：引擎 `AssetManager` 直接读取 Cook 后的二进制文件，零拷贝（或极少拷贝）直接上 GPU。

---

## 2. 核心数据结构设计 (C++)

### 2.1 开发期统一中间格式 (RawSceneData)
存在于内存中，用于 Normalizer 和 Cooker 之间的数据传递。

```cpp
namespace dse::asset::compiler {

// 顶点格式掩码
enum class VertexAttribute : uint32_t {
    Position = 1 << 0,
    Normal   = 1 << 1,
    Tangent  = 1 << 2,
    TexCoord = 1 << 3,
    Color    = 1 << 4,
    Joints   = 1 << 5,
    Weights  = 1 << 6
};

// 单个子网格 (对应一个 DrawCall)
struct RawSubMesh {
    std::string name;
    uint32_t material_index;
    
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec4> colors;
    std::vector<glm::ivec4> joint_indices;
    std::vector<glm::vec4> joint_weights;
    
    std::vector<uint32_t> indices;
};

// 骨骼节点
struct RawBone {
    std::string name;
    int parent_index = -1;
    glm::mat4 inverse_bind_matrix;
    glm::mat4 local_transform;
};

// 基础 PBR 材质数据
struct RawMaterial {
    std::string name;
    glm::vec4 base_color_factor{1.0f};
    float metallic_factor{0.0f};
    float roughness_factor{0.5f};
    glm::vec3 emissive_factor{0.0f};
    
    std::string base_color_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
};

// 完整中间场景
struct RawSceneData {
    std::vector<RawSubMesh> meshes;
    std::vector<RawMaterial> materials;
    std::vector<RawBone> skeleton;
    // std::vector<RawAnimation> animations; // 后续扩展
};

} // namespace dse::asset::compiler
```

### 2.2 运行时私有格式 (.dmesh)
紧凑的二进制结构，适合直接映射到 RHI Buffer。

```cpp
// .dmesh 文件头 (Magic: 'D' 'S' 'E' 'M')
struct MeshHeader {
    char magic[4];
    uint32_t version;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t submesh_count;
    uint32_t attribute_mask; // 标识包含哪些顶点属性
    
    // 偏移量记录
    uint64_t vertex_data_offset;
    uint64_t index_data_offset;
    uint64_t submesh_data_offset;
};

// 运行时 SubMesh 描述
struct SubMeshDesc {
    uint32_t index_start;
    uint32_t index_count;
    uint32_t base_vertex;
    uint32_t material_id;
    glm::vec3 bounding_box_min;
    glm::vec3 bounding_box_max;
};
```

---

## 3. 模块划分与工作流

### 模块 1：AssetBuilder (离线工具/编辑器模块)
- 独立于引擎 Runtime 的可执行文件或模块。
- **输入**：`model.gltf` / `model.fbx`
- **处理**：
  1. 调用 `tinygltf` / `Assimp` 解析得到各自格式的 Tree。
  2. 提取数据填充到 `RawSceneData`（同时执行 Y-up 转换、缩放统一）。
  3. 执行 `MeshOptimizer`：如果缺少法线/切线则自动生成，并交叉化顶点数据（Interleaving）。
  4. 序列化为 `model.dmesh` 和配套的 `model.dmat`。

### 模块 2：AssetManager 扩展 (运行时)
- 移除原来对 `tinygltf` / `Assimp` 的直接依赖。
- 增加 `LoadMesh(const std::string& dmesh_path)`。
- **处理**：直接 `mmap` 或 fread 读取 `.dmesh`，将 vertex_data 和 index_data 直接提交给 `RhiDevice` 创建 VBO/IBO。

---

## 4. 第一批里程碑 (Milestones)

### M1：基础静态网格管线打通 (Static Mesh + PBR)
- **目标**：能将外部 GLTF/FBX 的静态网格和材质属性转为引擎内格式并渲染。
- **任务**：
  1. 引入并配置 `CMake` 使 `AssetBuilder` 工具能链接 `tinygltf` 和 `Assimp`。
  2. 定义并实现 `RawSceneData` 和 `.dmesh` 二进制文件的读写（C++ 序列化）。
  3. 实现 GLTF 的 `Position`, `Normal`, `TexCoord` 提取与交错打包。
  4. 引擎端 `MeshRenderSystem` 和 `AssetManager` 适配读取 `.dmesh`。

### M2：材质与纹理资源解耦
- **目标**：模型引用的贴图能自动分离处理，形成完整的材质链路。
- **任务**：
  1. 解析层提取纹理路径（处理嵌入式 base64 或外部图片）。
  2. 自动触发纹理压缩（如转为 DDS/KTX2 或暂时保持 PNG）。
  3. 生成 `.dmat` 描述文件，Lua 端可以通过加载 `.dmat` 自动挂载 PBR 参数和贴图到 `MeshRendererComponent`。

### M3：骨骼动画基础接入 (Skeletal Animation)
- **目标**：支持带蒙皮的模型渲染。
- **任务**：
  1. `RawSceneData` 扩展 `Joints` 和 `Weights` 提取。
  2. 解析层提取骨架层级（Hierarchy）和 Inverse Bind Matrices。
  3. RHI 层增加 `SkinnedMesh` 的 Shader Variant（支持 4 骨骼影响/顶点的矩阵调色板计算）。
  4. 暂不实现动画播放，只验证 T-Pose/A-Pose 的蒙皮绑定正确性。

### M4：动画片段与状态机
- **目标**：模型动起来。
- **任务**：
  1. 提取关键帧数据（Translation, Rotation, Scale），重采样为统一频率的 `.danim` 格式。
  2. ECS 端扩展 `Animator3DComponent`。
  3. 运行时实现双四元数/矩阵插值，并将结果更新至 GPU 的 Uniform Buffer。
