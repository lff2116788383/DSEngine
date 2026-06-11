/**
 * @file asset_manager.cpp
 * @brief 资产管理器，负责加载、缓存和生命周期管理(如纹理、音频、预制体)
 */

#include "engine/assets/asset_manager.h"
#include "engine/assets/pak_reader.h"
#include "engine/assets/native_file_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/core/job_system.h"
#include "engine/core/event_bus.h"
#include <utility>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <rapidjson/document.h>
#include "bundle/bundle.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
extern "C" {
#include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// ─── .dmat binary cache helpers ──────────────────────────────────────────
static constexpr uint32_t kDmatBinMagic   = 0x42544D44u; // 'DMTB'
static constexpr uint32_t kDmatBinVersion = 1u;

static void DmW32(std::ofstream& f, uint32_t v)  { f.write(reinterpret_cast<const char*>(&v), 4); }
static void DmWF32(std::ofstream& f, float v)    { f.write(reinterpret_cast<const char*>(&v), 4); }
static void DmW64(std::ofstream& f, int64_t v)   { f.write(reinterpret_cast<const char*>(&v), 8); }
static void DmWStr(std::ofstream& f, const std::string& s) {
    DmW32(f, static_cast<uint32_t>(s.size()));
    if (!s.empty()) f.write(s.data(), s.size());
}
static bool DmR32(std::ifstream& f, uint32_t& v)  { return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 4)); }
static bool DmRF32(std::ifstream& f, float& v)    { return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 4)); }
static bool DmR64(std::ifstream& f, int64_t& v)   { return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 8)); }
static bool DmRStr(std::ifstream& f, std::string& s) {
    uint32_t len = 0;
    if (!DmR32(f, len) || len > 8192u) return false;
    s.resize(len);
    return len == 0 || static_cast<bool>(f.read(s.data(), len));
}
static int64_t DmatGetMtime(const std::string& p) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(std::filesystem::path(p), ec);
    return ec ? -1LL : static_cast<int64_t>(t.time_since_epoch().count());
}

struct DmatBinEntry {
    std::string name;
    float base_color[4] = {1,1,1,1};
    float metallic = 0.f, roughness = 0.5f;
    float emissive[3] = {0,0,0};
    float normal_scale = 1.f, ao = 1.f, alpha_cutoff = 0.5f;
    uint8_t double_sided = 0, alpha_test = 0;
    std::string tex_albedo, tex_normal, tex_mr, tex_emissive, tex_occlusion;
};

static bool LoadDmatBin(const std::string& bin_path, std::vector<DmatBinEntry>& out) {
    out.clear();
    std::ifstream f(bin_path, std::ios::binary);
    if (!f.is_open()) return false;
    uint32_t magic = 0, ver = 0, count = 0;
    int64_t src_mtime = 0;
    if (!DmR32(f, magic) || magic != kDmatBinMagic) return false;
    if (!DmR32(f, ver)   || ver   != kDmatBinVersion) return false;
    if (!DmR64(f, src_mtime)) return false;
    if (!DmR32(f, count) || count > 1024u) return false;
    out.resize(count);
    for (auto& e : out) {
        if (!DmRStr(f, e.name)) return false;
        for (float& c : e.base_color) if (!DmRF32(f, c)) return false;
        if (!DmRF32(f, e.metallic) || !DmRF32(f, e.roughness)) return false;
        for (float& c : e.emissive) if (!DmRF32(f, c)) return false;
        if (!DmRF32(f, e.normal_scale) || !DmRF32(f, e.ao) || !DmRF32(f, e.alpha_cutoff)) return false;
        if (!f.read(reinterpret_cast<char*>(&e.double_sided), 1)) return false;
        if (!f.read(reinterpret_cast<char*>(&e.alpha_test), 1)) return false;
        if (!DmRStr(f, e.tex_albedo) || !DmRStr(f, e.tex_normal) ||
            !DmRStr(f, e.tex_mr)     || !DmRStr(f, e.tex_emissive) ||
            !DmRStr(f, e.tex_occlusion)) return false;
    }
    return true;
}

static void SaveDmatBin(const std::string& dmat_path,
                         const std::string& bin_path,
                         const rapidjson::Document& doc) {
    if (!doc.IsObject() || !doc.HasMember("materials") || !doc["materials"].IsArray()) return;
    const auto& mats = doc["materials"];
    std::ofstream f(bin_path, std::ios::binary);
    if (!f.is_open()) return;
    DmW32(f, kDmatBinMagic);
    DmW32(f, kDmatBinVersion);
    DmW64(f, DmatGetMtime(dmat_path));
    DmW32(f, static_cast<uint32_t>(mats.Size()));
    auto gs = [](const rapidjson::Value& v, const char* k, const char* def = "") -> std::string {
        return (v.HasMember(k) && v[k].IsString()) ? v[k].GetString() : def;
    };
    auto gf = [](const rapidjson::Value& v, const char* k, float def) -> float {
        return (v.HasMember(k) && v[k].IsNumber()) ? v[k].GetFloat() : def;
    };
    for (rapidjson::SizeType i = 0; i < mats.Size(); ++i) {
        const auto& m = mats[i];
        DmWStr(f, gs(m, "name"));
        float bc[4] = {1,1,1,1};
        if (m.HasMember("base_color") && m["base_color"].IsArray() && m["base_color"].Size() >= 4) {
            for (int j = 0; j < 4; ++j) bc[j] = m["base_color"][j].GetFloat();
        }
        for (float v : bc) DmWF32(f, v);
        DmWF32(f, gf(m, "metallic", 0.f));
        DmWF32(f, gf(m, "roughness", 0.5f));
        float em[3] = {0,0,0};
        if (m.HasMember("emissive") && m["emissive"].IsArray() && m["emissive"].Size() >= 3) {
            for (int j = 0; j < 3; ++j) em[j] = m["emissive"][j].GetFloat();
        }
        for (float v : em) DmWF32(f, v);
        DmWF32(f, gf(m, "normal_scale", 1.f));
        DmWF32(f, gf(m, "occlusion_strength", 1.f));
        DmWF32(f, gf(m, "alpha_cutoff", 0.5f));
        uint8_t ds = (m.HasMember("double_sided") && m["double_sided"].IsBool() && m["double_sided"].GetBool()) ? 1 : 0;
        uint8_t at = (m.HasMember("alpha_test")   && m["alpha_test"].IsBool()   && m["alpha_test"].GetBool())   ? 1 : 0;
        f.write(reinterpret_cast<const char*>(&ds), 1);
        f.write(reinterpret_cast<const char*>(&at), 1);
        DmWStr(f, gs(m, "base_color_texture"));
        DmWStr(f, gs(m, "normal_texture"));
        DmWStr(f, gs(m, "metallic_roughness_texture"));
        DmWStr(f, gs(m, "emissive_texture"));
        DmWStr(f, gs(m, "occlusion_texture"));
    }
}

namespace {
std::string NormalizePath(const std::string& path) {
    std::filesystem::path p(path);
    return p.make_preferred().lexically_normal().string();
}

std::string ResolveAssetPathImpl(const std::string& path, const std::string& data_root) {
    std::filesystem::path input(path);
    if (input.is_absolute()) {
        if (std::filesystem::exists(input)) {
            return NormalizePath(input.string());
        }
        return "";
    }

    std::filesystem::path normalized_input = input.lexically_normal();
    if (normalized_input.empty()) {
        return "";
    }
    if (std::filesystem::exists(normalized_input)) {
        return NormalizePath(normalized_input.string());
    }

    std::filesystem::path relative = normalized_input;
    const std::filesystem::path data_prefix = std::filesystem::path("data");
    const std::filesystem::path bin_data_prefix = std::filesystem::path("bin") / "data";
    auto starts_with = [](const std::filesystem::path& value, const std::filesystem::path& prefix) {
        auto it_value = value.begin();
        auto it_prefix = prefix.begin();
        for (; it_prefix != prefix.end(); ++it_prefix, ++it_value) {
            if (it_value == value.end() || *it_value != *it_prefix) {
                return false;
            }
        }
        return true;
    };
    if (starts_with(relative, data_prefix)) {
        relative = relative.lexically_relative(data_prefix);
    } else if (starts_with(relative, bin_data_prefix)) {
        relative = relative.lexically_relative(bin_data_prefix);
    }
    if (relative.empty()) {
        return "";
    }

    std::filesystem::path resolved = (std::filesystem::path(data_root) / relative).lexically_normal();
    return resolved.generic_string();
}

}

AssetManager::AssetManager() = default;

AssetManager::~AssetManager() {
    StopFileWatcher();
    // 在成员容器析构前主动释放，避免静态 CRT Debug 堆在 DLL 卸载/测试进程退出阶段
    // 再处理跨模块分配过的 STL 节点时触发 debug_heap 链表断言。
    ReleaseGpuResources();
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        pending_main_thread_callbacks_.clear();
    }
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        textures_.clear();
        cubemaps_.clear();
        shaders_.clear();
        gpu_texture_handles_.clear();
        gpu_cubemap_handles_.clear();
        gpu_shader_handles_.clear();
        audio_clips_.clear();
        dmeshes_.clear();
        danims_.clear();
        dskels_.clear();
        materials_.clear();
        material_names_.clear();
        vfs_files_.clear();
    }
}

bool AssetManager::MountPak(const std::string& pak_path) {
    auto reader = std::make_shared<dse::pak::PakReader>();
    if (!reader->Open(pak_path)) {
        return false;
    }
    mounted_paks_.push_back(std::move(reader));
    DEBUG_LOG_INFO("[AssetManager] Mounted pak: {}", pak_path);
    return true;
}

void AssetManager::UnmountAllPaks() {
    mounted_paks_.clear();
    DEBUG_LOG_INFO("[AssetManager] Unmounted all paks");
}

bool AssetManager::HasMountedPak() const {
    return !mounted_paks_.empty();
}

bool AssetManager::ReadFromPak(const std::string& relative_path, std::vector<uint8_t>& out_data) const {
    // Normalize path separators to forward slash for pak lookup
    std::string normalized = relative_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    for (auto it = mounted_paks_.rbegin(); it != mounted_paks_.rend(); ++it) {
        if ((*it)->ReadFile(normalized, out_data)) {
            return true;
        }
    }
    return false;
}

TextureAsset::TextureAsset(const std::string& path, unsigned int handle, int width, int height, int channels)
    : path_(path), handle_(handle), width_(width), height_(height), channels_(channels) {
}

TextureAsset::~TextureAsset() {
}

CubemapAsset::CubemapAsset(const std::string& path, unsigned int handle, int width, int height)
    : path_(path), handle_(handle), width_(width), height_(height) {
}

CubemapAsset::~CubemapAsset() {
}

ShaderAsset::ShaderAsset(const std::string& name, unsigned int handle)
    : name_(name), handle_(handle) {
}

ShaderAsset::~ShaderAsset() {
}

MaterialAsset::MaterialAsset(unsigned int id, const std::string& name)
    : id_(id), name_(name) {
    if (name.find("mesh") != std::string::npos || name.find("pbr") != std::string::npos) {
        shader_variant_ = "MESH_PBR";
        blend_mode_ = MaterialBlendMode::Opaque;
    }
}

void AssetManager::SetRhiDevice(RhiDevice* rhi_device) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    rhi_device_ = rhi_device;
}

void AssetManager::SetEventBus(dse::core::EventBus* event_bus) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    event_bus_ = event_bus;
}

void AssetManager::SetJobSystem(dse::core::JobSystem* job_system) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    job_system_ = job_system;
}

dse::core::EventBus* AssetManager::GetEventBus() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return event_bus_;
}

dse::core::JobSystem* AssetManager::GetJobSystem() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return job_system_;
}

void AssetManager::SetFileSystem(dse::assets::FileSystem* file_system) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    file_system_ = file_system;
}

dse::assets::FileSystem* AssetManager::GetFileSystem() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return file_system_;
}

void AssetManager::ConfigureDataRoot(const std::string& data_root) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (data_root.empty()) {
        return;
    }
    data_root_ = NormalizePath(data_root);
}

std::string AssetManager::GetDataRoot() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return data_root_;
}

std::string AssetManager::NormalizeAssetPath(const std::string& path) const {
    if (path.empty()) {
        return "";
    }

    std::filesystem::path normalized = std::filesystem::path(path).lexically_normal();
    if (normalized.empty()) {
        return "";
    }

    const std::filesystem::path data_root = GetDataRoot();
    if (normalized.is_absolute()) {
        if (!data_root.empty()) {
            const std::filesystem::path normalized_data_root = std::filesystem::path(data_root).lexically_normal();
            const std::filesystem::path relative = normalized.lexically_relative(normalized_data_root);
            // 用 generic_string()（跨平台 std::string，'/' 分隔）做 ".." 前缀判断，
            // 避免 path::native() 在 Windows 返回 wstring / POSIX 返回 string 的类型差异。
            const std::string relative_native = relative.generic_string();
            if (!relative.empty() && relative_native.rfind("..", 0) != 0) {
                std::string logical = relative.generic_string();
                return logical == "." ? "" : logical;
            }
        }
        return normalized.generic_string();
    }

    const std::filesystem::path data_prefix = std::filesystem::path("data");
    const std::filesystem::path bin_data_prefix = std::filesystem::path("bin") / "data";
    auto starts_with = [](const std::filesystem::path& value, const std::filesystem::path& prefix) {
        auto it_value = value.begin();
        auto it_prefix = prefix.begin();
        for (; it_prefix != prefix.end(); ++it_prefix, ++it_value) {
            if (it_value == value.end() || *it_value != *it_prefix) {
                return false;
            }
        }
        return true;
    };

    if (starts_with(normalized, data_prefix)) {
        normalized = normalized.lexically_relative(data_prefix);
    } else if (starts_with(normalized, bin_data_prefix)) {
        normalized = normalized.lexically_relative(bin_data_prefix);
    }

    const std::string logical = normalized.generic_string();
    return logical == "." ? "" : logical;
}

std::string AssetManager::ResolveAssetPath(const std::string& path) const {
    return ResolveAssetPathImpl(path, GetDataRoot());
}

bool AssetManager::PackBundle(const std::string& input_dir, const std::string& output_bundle, const std::string& aes_key) {
    if (!std::filesystem::exists(input_dir)) return false;
    bundle::archive pak;
    int idx = 0;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
            if (!file) continue;
            std::streamsize size = file.tellg();
            if (size < 0) {
                continue;
            }
            file.seekg(0, std::ios::beg);
            std::string content(static_cast<std::size_t>(size), '\0');
            if (size == 0 || file.read(content.data(), size)) {
                pak.resize(idx + 1);
                std::string rel_path = std::filesystem::relative(entry.path(), input_dir).generic_string();
                std::replace(rel_path.begin(), rel_path.end(), '\\', '/');
                pak[idx]["name"] = rel_path;
                pak[idx]["data"] = content;
                idx++;
            }
        }
    }
    
    std::string bin = pak.zip(60); // level 60
    
    if (!aes_key.empty() && aes_key.size() >= 16) {
        struct AES_ctx ctx;
        uint8_t iv[16] = {0}; // Fixed IV for simplicity
        AES_init_ctx_iv(&ctx, (const uint8_t*)aes_key.c_str(), iv);
        AES_CTR_xcrypt_buffer(&ctx, (uint8_t*)bin.data(), bin.size());
    }
    
    std::ofstream out(output_bundle, std::ios::binary);
    if (!out) return false;
    out.write(bin.data(), bin.size());
    return true;
}

bool AssetManager::MountBundle(const std::string& bundle_path, const std::string& aes_key) {
    std::ifstream file(bundle_path, std::ios::binary | std::ios::ate);
    if (!file) {
        DEBUG_LOG_ERROR("Failed to open bundle: {}", bundle_path);
        return false;
    }
    std::streamsize size = file.tellg();
    if (size < 0) {
        DEBUG_LOG_ERROR("Failed to query bundle size: {}", bundle_path);
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::string bin(static_cast<std::size_t>(size), '\0');
    if (size > 0 && !file.read(bin.data(), size)) return false;
    
    if (!aes_key.empty() && aes_key.size() >= 16) {
        struct AES_ctx ctx;
        uint8_t iv[16] = {0};
        AES_init_ctx_iv(&ctx, (const uint8_t*)aes_key.c_str(), iv);
        AES_CTR_xcrypt_buffer(&ctx, (uint8_t*)bin.data(), bin.size());
    }
    
    bundle::archive pak;
    pak.zip(bin);
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (size_t i = 0; i < pak.size(); ++i) {
        const std::string& name = pak[i]["name"];
        const std::string& data = pak[i]["data"];
        vfs_files_[name] = std::vector<uint8_t>(data.begin(), data.end());
    }
    DEBUG_LOG_INFO("Mounted bundle: {} with {} files", bundle_path, pak.size());
    return true;
}

bool AssetManager::LoadFileToMemory(const std::string& path, std::vector<uint8_t>& out_data) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string search_path = resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path);

    // Convert to generic relative path for VFS check
    std::string vfs_key = logical_path.empty() ? search_path : logical_path;
    std::replace(vfs_key.begin(), vfs_key.end(), '\\', '/');
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = vfs_files_.find(vfs_key);
        if (it != vfs_files_.end()) {
            out_data = it->second;
            return true;
        }
    }
    
    const std::string load_path = resolved_path.empty() ? path : resolved_path;

    dse::assets::FileSystem* fs;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        fs = file_system_;
    }
    if (fs) {
        return fs->ReadFile(load_path, out_data);
    }

    std::ifstream file(load_path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    std::streamsize size = file.tellg();
    if (size < 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    out_data.resize(static_cast<std::size_t>(size));
    if (size == 0 || file.read(reinterpret_cast<char*>(out_data.data()), size)) {
        return true;
    }
    return false;
}

bool AssetManager::LoadImageRgba(const std::string& path, std::vector<unsigned char>& out_pixels, int& out_width, int& out_height, int& out_channels) {
    out_pixels.clear();
    out_width = 0;
    out_height = 0;
    out_channels = 0;

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read image file: {}", path);
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &channels, 4);
    if (!data || width <= 0 || height <= 0) {
        if (data) {
            stbi_image_free(data);
        }
        DEBUG_LOG_ERROR("Failed to decode image: {}", path);
        return false;
    }

    const std::size_t byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    out_pixels.assign(data, data + byte_count);
    out_width = width;
    out_height = height;
    out_channels = channels;
    stbi_image_free(data);
    return true;
}

// ============================================================
// DDS 文件解析（支持 DXT1/3/5, BC4/5/7, DX10 扩展头）
// ============================================================
namespace {

struct DdsPixelFormat {
    uint32_t size, flags, four_cc, rgb_bit_count;
    uint32_t r_mask, g_mask, b_mask, a_mask;
};

struct DdsHeader {
    uint32_t size, flags, height, width;
    uint32_t pitch_or_linear_size, depth, mip_map_count;
    uint32_t reserved1[11];
    DdsPixelFormat pixel_format;
    uint32_t caps, caps2, caps3, caps4, reserved2;
};

struct DdsHeaderDX10 {
    uint32_t dxgi_format, resource_dimension, misc_flag, array_size, misc_flags2;
};

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

bool ParseDds(const std::vector<uint8_t>& file_data,
              CompressedTextureFormat& out_format,
              std::vector<CompressedMipLevel>& out_mips,
              int& out_width, int& out_height) {
    if (file_data.size() < 128) return false;
    if (file_data[0] != 'D' || file_data[1] != 'D' || file_data[2] != 'S' || file_data[3] != ' ') return false;

    const auto* header = reinterpret_cast<const DdsHeader*>(file_data.data() + 4);
    out_width = static_cast<int>(header->width);
    out_height = static_cast<int>(header->height);
    uint32_t mip_count = (header->mip_map_count > 0) ? header->mip_map_count : 1;

    size_t data_offset = 4 + sizeof(DdsHeader);
    uint32_t block_bytes = 16;
    bool format_found = false;

    uint32_t fcc = header->pixel_format.four_cc;
    if (fcc == MakeFourCC('D','X','1','0')) {
        if (file_data.size() < data_offset + sizeof(DdsHeaderDX10)) return false;
        const auto* dx10 = reinterpret_cast<const DdsHeaderDX10*>(file_data.data() + data_offset);
        data_offset += sizeof(DdsHeaderDX10);
        switch (dx10->dxgi_format) {
            case 71: out_format = CompressedTextureFormat::BC1_UNORM; block_bytes = 8; format_found = true; break;
            case 72: out_format = CompressedTextureFormat::BC1_SRGB;  block_bytes = 8; format_found = true; break;
            case 74: out_format = CompressedTextureFormat::BC2_UNORM; format_found = true; break;
            case 77: out_format = CompressedTextureFormat::BC3_UNORM; format_found = true; break;
            case 78: out_format = CompressedTextureFormat::BC3_SRGB;  format_found = true; break;
            case 80: out_format = CompressedTextureFormat::BC4_UNORM; block_bytes = 8; format_found = true; break;
            case 83: out_format = CompressedTextureFormat::BC5_UNORM; format_found = true; break;
            case 98: out_format = CompressedTextureFormat::BC7_UNORM; format_found = true; break;
            case 99: out_format = CompressedTextureFormat::BC7_SRGB;  format_found = true; break;
            default: break;
        }
    } else {
        if      (fcc == MakeFourCC('D','X','T','1')) { out_format = CompressedTextureFormat::BC1_UNORM; block_bytes = 8; format_found = true; }
        else if (fcc == MakeFourCC('D','X','T','3')) { out_format = CompressedTextureFormat::BC2_UNORM; format_found = true; }
        else if (fcc == MakeFourCC('D','X','T','5')) { out_format = CompressedTextureFormat::BC3_UNORM; format_found = true; }
        else if (fcc == MakeFourCC('A','T','I','1') || fcc == MakeFourCC('B','C','4','U')) { out_format = CompressedTextureFormat::BC4_UNORM; block_bytes = 8; format_found = true; }
        else if (fcc == MakeFourCC('A','T','I','2') || fcc == MakeFourCC('B','C','5','U')) { out_format = CompressedTextureFormat::BC5_UNORM; format_found = true; }
    }

    if (!format_found) return false;

    out_mips.clear();
    int w = out_width, h = out_height;
    size_t offset = data_offset;
    for (uint32_t i = 0; i < mip_count; ++i) {
        uint32_t blocks_w = std::max<uint32_t>(1u, (static_cast<uint32_t>(w) + 3u) / 4u);
        uint32_t blocks_h = std::max<uint32_t>(1u, (static_cast<uint32_t>(h) + 3u) / 4u);
        size_t mip_size = static_cast<size_t>(blocks_w) * blocks_h * block_bytes;
        if (offset + mip_size > file_data.size()) break;
        out_mips.push_back({file_data.data() + offset, mip_size, w, h});
        offset += mip_size;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }

    return !out_mips.empty();
}

bool HasDdsExtension(const std::string& path) {
    if (path.size() < 4) return false;
    auto ext = path.substr(path.size() - 4);
    return ext == ".dds" || ext == ".DDS";
}

} // anonymous namespace

std::shared_ptr<TextureAsset> AssetManager::LoadTexture(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            return it->second;
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read texture file: {}", path);
        return nullptr;
    }

    if (HasDdsExtension(path)) {
        CompressedTextureFormat fmt;
        std::vector<CompressedMipLevel> mips;
        int dds_w, dds_h;
        if (!ParseDds(file_data, fmt, mips, dds_w, dds_h)) {
            DEBUG_LOG_ERROR("Failed to parse DDS file: {}", path);
            return nullptr;
        }

        unsigned int handle = 0;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            if (rhi_device_) {
                handle = rhi_device_->CreateCompressedTexture2D(fmt, mips, true);
            }
        }
        if (handle == 0) {
            DEBUG_LOG_ERROR("Failed to create compressed texture via RHI: {}", path);
            return nullptr;
        }

        auto tex = std::make_shared<TextureAsset>(path, handle, dds_w, dds_h, 4);
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            textures_[cache_key] = tex;
            gpu_texture_handles_.insert(handle);
        }
        size_t total = 0;
        for (auto& m : mips) total += m.size;
        TouchLru(cache_key, total);
        return tex;
    }

    int width, height, channels;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        stbi_set_flip_vertically_on_load(rhi_device_ ? rhi_device_->NeedsTextureYFlip() : true);
    }
    unsigned char* data = stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4);
    
    if (!data) {
        DEBUG_LOG_ERROR("Failed to decode texture: {}", path);
        return nullptr;
    }

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTexture2D(width, height, data, true);
        }
    }
    if (handle == 0) {
        stbi_image_free(data);
        DEBUG_LOG_ERROR("Failed to create texture via RHI: {}", path);
        return nullptr;
    }

    stbi_image_free(data);

    auto tex = std::make_shared<TextureAsset>(path, handle, width, height, channels);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        textures_[cache_key] = tex;
        gpu_texture_handles_.insert(handle);
    }
    TouchLru(cache_key, static_cast<std::size_t>(width) * height * 4u);
    return tex;
}

std::string AssetManager::FindTexturePathByHandle(unsigned int handle) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (const auto& [key, tex] : textures_) {
        if (tex && tex->GetHandle() == handle) {
            return tex->GetPath();
        }
    }
    return {};
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemapDirectory(const std::string& directory_path) {
    const std::string logical_path = NormalizeAssetPath(directory_path);
    const std::string resolved_path = ResolveAssetPath(directory_path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(directory_path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cubemaps_.find(cache_key);
        if (it != cubemaps_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    const std::string base_path = resolved_path.empty() ? directory_path : resolved_path;
    const std::filesystem::path base_dir = std::filesystem::path(base_path);
    if (!std::filesystem::exists(base_dir) || !std::filesystem::is_directory(base_dir)) {
        DEBUG_LOG_ERROR("Cubemap directory does not exist: {}", directory_path);
        return nullptr;
    }

    const char* face_names[6] = {"px", "nx", "py", "ny", "pz", "nz"};
    const char* extensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".ppm"};
    std::vector<std::string> face_paths(6);
    for (int face = 0; face < 6; ++face) {
        for (const char* ext : extensions) {
            const std::filesystem::path candidate = base_dir / (std::string(face_names[face]) + ext);
            if (std::filesystem::exists(candidate)) {
                face_paths[face] = candidate.string();
                break;
            }
        }
        if (face_paths[face].empty()) {
            DEBUG_LOG_ERROR("Cubemap face is missing: {} ({})", directory_path, face_names[face]);
            return nullptr;
        }
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char*> face_pixels(6, nullptr);
    auto cleanup_faces = [&]() {
        for (auto* pixels : face_pixels) {
            if (pixels) {
                stbi_image_free(pixels);
            }
        }
    };

    stbi_set_flip_vertically_on_load(false);
    for (int face = 0; face < 6; ++face) {
        std::vector<uint8_t> file_data;
        if (!LoadFileToMemory(face_paths[face], file_data)) {
            DEBUG_LOG_ERROR("Failed to read cubemap face: {}", face_paths[face]);
            cleanup_faces();
            return nullptr;
        }

        int face_width = 0;
        int face_height = 0;
        int face_channels = 0;
        face_pixels[face] = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &face_width, &face_height, &face_channels, 4);
        if (!face_pixels[face]) {
            DEBUG_LOG_ERROR("Failed to decode cubemap face: {}", face_paths[face]);
            cleanup_faces();
            return nullptr;
        }

        if (face == 0) {
            width = face_width;
            height = face_height;
            channels = face_channels;
        } else if (face_width != width || face_height != height) {
            DEBUG_LOG_ERROR("Cubemap face size mismatch in directory: {}", directory_path);
            cleanup_faces();
            return nullptr;
        }
    }

    const unsigned char* face_ptrs[6] = {
        face_pixels[0], face_pixels[1], face_pixels[2], face_pixels[3], face_pixels[4], face_pixels[5]
    };

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTextureCube(width, height, face_ptrs, true);
        }
    }

    cleanup_faces();

    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create cubemap via RHI: {}", directory_path);
        return nullptr;
    }

    auto cubemap = std::make_shared<CubemapAsset>(base_dir.generic_string(), handle, width, height);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cubemaps_[cache_key] = cubemap;
        gpu_cubemap_handles_.insert(handle);
    }
    TouchLru(cache_key, static_cast<std::size_t>(width) * height * 4u * 6u);
    return cubemap;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemapPanorama(const std::string& image_path, int face_size) {
    const std::string logical_path = NormalizeAssetPath(image_path);
    const std::string resolved_path = ResolveAssetPath(image_path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(image_path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cubemaps_.find(cache_key);
        if (it != cubemaps_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    // Load the equirectangular panorama image
    std::vector<unsigned char> pano_pixels;
    int pano_w = 0, pano_h = 0, pano_ch = 0;
    if (!LoadImageRgba(image_path, pano_pixels, pano_w, pano_h, pano_ch)) {
        DEBUG_LOG_ERROR("Failed to load panorama image: {}", image_path);
        return nullptr;
    }

    if (face_size <= 0) face_size = 512;

    // Allocate 6 faces
    const size_t face_bytes = static_cast<size_t>(face_size) * face_size * 4;
    std::vector<std::vector<unsigned char>> faces(6);
    for (auto& f : faces) f.resize(face_bytes);

    // Equirectangular → cubemap conversion (CPU)
    // Face order: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    const float half = static_cast<float>(face_size) * 0.5f;
    const float inv_half = 1.0f / half;
    const float pi = 3.14159265358979f;
    const float two_pi = 2.0f * pi;

    for (int face = 0; face < 6; ++face) {
        unsigned char* dst = faces[face].data();
        for (int y = 0; y < face_size; ++y) {
            for (int x = 0; x < face_size; ++x) {
                // Normalized face coordinates [-1, +1]
                const float fx = (static_cast<float>(x) + 0.5f) * inv_half - 1.0f;
                const float fy = 1.0f - (static_cast<float>(y) + 0.5f) * inv_half;

                // Compute 3D direction for this face/pixel
                float dx = 0.0f, dy = 0.0f, dz = 0.0f;
                switch (face) {
                    case 0: dx =  1.0f; dy = fy; dz = -fx; break; // +X
                    case 1: dx = -1.0f; dy = fy; dz =  fx; break; // -X
                    case 2: dx =  fx;   dy =  1.0f; dz = -fy; break; // +Y
                    case 3: dx =  fx;   dy = -1.0f; dz =  fy; break; // -Y
                    case 4: dx =  fx;   dy = fy; dz =  1.0f; break; // +Z
                    case 5: dx = -fx;   dy = fy; dz = -1.0f; break; // -Z
                }

                // Direction → spherical → equirectangular UV
                const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float nx = dx / len;
                const float ny = dy / len;
                const float nz = dz / len;

                // theta: azimuth angle, phi: elevation angle
                const float theta = std::atan2(nz, nx); // [-pi, pi]
                const float phi = std::asin(std::clamp(ny, -1.0f, 1.0f)); // [-pi/2, pi/2]

                // UV in equirectangular image
                float u = (theta + pi) / two_pi; // [0, 1]
                float v = 0.5f - phi / pi;       // [0, 1], top=0

                // Bilinear sample from panorama
                const float px_f = u * static_cast<float>(pano_w) - 0.5f;
                const float py_f = v * static_cast<float>(pano_h) - 0.5f;
                const int px0 = static_cast<int>(std::floor(px_f));
                const int py0 = static_cast<int>(std::floor(py_f));
                const float fx_frac = px_f - static_cast<float>(px0);
                const float fy_frac = py_f - static_cast<float>(py0);

                auto sample = [&](int sx, int sy) -> const unsigned char* {
                    sx = ((sx % pano_w) + pano_w) % pano_w;
                    sy = std::clamp(sy, 0, pano_h - 1);
                    return &pano_pixels[(static_cast<size_t>(sy) * pano_w + sx) * 4];
                };

                const unsigned char* s00 = sample(px0, py0);
                const unsigned char* s10 = sample(px0 + 1, py0);
                const unsigned char* s01 = sample(px0, py0 + 1);
                const unsigned char* s11 = sample(px0 + 1, py0 + 1);

                const size_t dst_offset = (static_cast<size_t>(y) * face_size + x) * 4;
                for (int c = 0; c < 4; ++c) {
                    const float top = static_cast<float>(s00[c]) * (1.0f - fx_frac) + static_cast<float>(s10[c]) * fx_frac;
                    const float bot = static_cast<float>(s01[c]) * (1.0f - fx_frac) + static_cast<float>(s11[c]) * fx_frac;
                    const float val = top * (1.0f - fy_frac) + bot * fy_frac;
                    dst[dst_offset + c] = static_cast<unsigned char>(std::clamp(val + 0.5f, 0.0f, 255.0f));
                }
            }
        }
    }

    const unsigned char* face_ptrs[6] = {
        faces[0].data(), faces[1].data(), faces[2].data(),
        faces[3].data(), faces[4].data(), faces[5].data()
    };

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTextureCube(face_size, face_size, face_ptrs, true);
        }
    }

    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create cubemap from panorama via RHI: {}", image_path);
        return nullptr;
    }

    auto cubemap = std::make_shared<CubemapAsset>(image_path, handle, face_size, face_size);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cubemaps_[cache_key] = cubemap;
        gpu_cubemap_handles_.insert(handle);
    }
    TouchLru(cache_key, static_cast<std::size_t>(face_size) * face_size * 4u * 6u);
    DEBUG_LOG_INFO("Loaded panorama skybox: {} ({}x{} per face)", image_path, face_size, face_size);
    return cubemap;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemapCross(const std::string& image_path) {
    const std::string logical_path = NormalizeAssetPath(image_path);
    const std::string resolved_path = ResolveAssetPath(image_path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(image_path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cubemaps_.find(cache_key);
        if (it != cubemaps_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    // Load the cross layout image
    std::vector<unsigned char> cross_pixels;
    int cross_w = 0, cross_h = 0, cross_ch = 0;
    if (!LoadImageRgba(image_path, cross_pixels, cross_w, cross_h, cross_ch)) {
        DEBUG_LOG_ERROR("Failed to load cross layout image: {}", image_path);
        return nullptr;
    }

    // Horizontal cross: 4 columns × 3 rows
    const int face_w = cross_w / 4;
    const int face_h = cross_h / 3;
    if (face_w <= 0 || face_h <= 0) {
        DEBUG_LOG_ERROR("Cross layout image too small: {}x{}", cross_w, cross_h);
        return nullptr;
    }

    // Extract 6 faces from the horizontal cross layout:
    //   Row 0:          [top]
    //   Row 1: [right] [back] [left] [front]
    //   Row 2:          [bottom]
    // OpenGL cubemap face order: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    // KF layout (from mesh_manager.cpp UV mapping):
    //   col=0,row=1 → right  (+X)
    //   col=1,row=1 → back   (-Z)
    //   col=2,row=1 → left   (-X)
    //   col=3,row=1 → front  (+Z)
    //   col=1,row=0 → top    (+Y)
    //   col=1,row=2 → bottom (-Y)
    struct FaceRegion { int col; int row; };
    const FaceRegion regions[6] = {
        {0, 1}, // +X (right)
        {2, 1}, // -X (left)
        {1, 0}, // +Y (top)
        {1, 2}, // -Y (bottom)
        {3, 1}, // +Z (front)
        {1, 1}, // -Z (back)
    };

    const size_t face_bytes = static_cast<size_t>(face_w) * face_h * 4;
    std::vector<std::vector<unsigned char>> faces(6);
    for (int face = 0; face < 6; ++face) {
        faces[face].resize(face_bytes);
        const int src_x0 = regions[face].col * face_w;
        const int src_y0 = regions[face].row * face_h;
        // +Y (face 2) and -Y (face 3) need 180° rotation to match OpenGL cubemap convention
        const bool rotate_180 = (face == 2 || face == 3);
        for (int y = 0; y < face_h; ++y) {
            const size_t src_offset = (static_cast<size_t>(src_y0 + y) * cross_w + src_x0) * 4;
            if (!rotate_180) {
                const size_t dst_offset = static_cast<size_t>(y) * face_w * 4;
                std::memcpy(faces[face].data() + dst_offset, cross_pixels.data() + src_offset, static_cast<size_t>(face_w) * 4);
            } else {
                // 180° rotation: dst(x,y) = src(w-1-x, h-1-y)
                const int dst_y = face_h - 1 - y;
                for (int x = 0; x < face_w; ++x) {
                    const int dst_x = face_w - 1 - x;
                    const size_t si = src_offset + static_cast<size_t>(x) * 4;
                    const size_t di = (static_cast<size_t>(dst_y) * face_w + dst_x) * 4;
                    std::memcpy(faces[face].data() + di, cross_pixels.data() + si, 4);
                }
            }
        }
    }

    const unsigned char* face_ptrs[6] = {
        faces[0].data(), faces[1].data(), faces[2].data(),
        faces[3].data(), faces[4].data(), faces[5].data()
    };

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTextureCube(face_w, face_h, face_ptrs, true);
        }
    }

    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create cubemap from cross layout via RHI: {}", image_path);
        return nullptr;
    }

    auto cubemap = std::make_shared<CubemapAsset>(image_path, handle, face_w, face_h);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cubemaps_[cache_key] = cubemap;
        gpu_cubemap_handles_.insert(handle);
    }
    TouchLru(cache_key, face_bytes * 6u);
    DEBUG_LOG_INFO("Loaded cross layout skybox: {} ({}x{} per face)", image_path, face_w, face_h);
    return cubemap;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemap(const std::string& path) {
    const std::string resolved = ResolveAssetPath(path);
    const std::string& check_path = resolved.empty() ? path : resolved;

    // If path is a directory → 6-face loading
    if (std::filesystem::exists(check_path) && std::filesystem::is_directory(check_path)) {
        return LoadCubemapDirectory(path);
    }
    // If path is a file → detect layout by aspect ratio
    if (std::filesystem::exists(check_path) && std::filesystem::is_regular_file(check_path)) {
        // Probe image dimensions to detect cross layout (4:3) vs panorama (2:1)
        std::vector<uint8_t> file_data;
        int probe_w = 0, probe_h = 0, probe_ch = 0;
        if (LoadFileToMemory(check_path, file_data)) {
            stbi_info_from_memory(file_data.data(), static_cast<int>(file_data.size()), &probe_w, &probe_h, &probe_ch);
        }
        // Horizontal cross: width/height ≈ 4/3 (1.33), tolerance 1.2~1.5
        if (probe_w > 0 && probe_h > 0) {
            const float aspect = static_cast<float>(probe_w) / static_cast<float>(probe_h);
            if (aspect >= 1.2f && aspect <= 1.5f) {
                return LoadCubemapCross(path);
            }
        }
        return LoadCubemapPanorama(path);
    }

    DEBUG_LOG_ERROR("Cubemap path not found (neither directory nor file): {}", path);
    return nullptr;
}

std::shared_ptr<ShaderAsset> AssetManager::LoadShader(const std::string& name, const std::string& vert_src, const std::string& frag_src) {
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = shaders_.find(name);
        if (it != shaders_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateShaderProgram(vert_src, frag_src);
        }
    }
    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create shader via RHI: {}", name);
        return nullptr;
    }

    auto shader = std::make_shared<ShaderAsset>(name, handle);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        shaders_[name] = shader;
        gpu_shader_handles_.insert(handle);
    }
    return shader;
}

std::shared_ptr<AudioClipAsset> AssetManager::LoadAudioClip(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = audio_clips_.find(cache_key);
        if (it != audio_clips_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read audio file: {}", path);
        return nullptr;
    }

    const std::size_t clip_bytes = file_data.size();
    auto clip = std::make_shared<AudioClipAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        audio_clips_[cache_key] = clip;
    }
    TouchLru(cache_key, clip_bytes);
    return clip;
}

std::shared_ptr<DmeshAsset> AssetManager::LoadDmesh(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = dmeshes_.find(cache_key);
        if (it != dmeshes_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read dmesh file: {}", path);
        return nullptr;
    }

    const std::size_t dmesh_bytes = file_data.size();
    auto dmesh = std::make_shared<DmeshAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        dmeshes_[cache_key] = dmesh;
    }
    TouchLru(cache_key, dmesh_bytes);
    return dmesh;
}

std::shared_ptr<DanimAsset> AssetManager::LoadDanim(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = danims_.find(cache_key);
        if (it != danims_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read danim file: {}", path);
        return nullptr;
    }

    const std::size_t danim_bytes = file_data.size();
    auto danim = std::make_shared<DanimAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        danims_[cache_key] = danim;
    }
    TouchLru(cache_key, danim_bytes);
    return danim;
}

std::shared_ptr<DskelAsset> AssetManager::LoadDskel(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = dskels_.find(cache_key);
        if (it != dskels_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read dskel file: {}", path);
        return nullptr;
    }

    const std::size_t dskel_bytes = file_data.size();
    auto dskel = std::make_shared<DskelAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        dskels_[cache_key] = dskel;
    }
    TouchLru(cache_key, dskel_bytes);
    return dskel;
}

namespace {
void PublishResourceLoaded(dse::core::EventBus* event_bus, const std::string& path, bool success) {
    if (!event_bus) {
        return;
    }
    event_bus->Publish<dse::core::ResourceLoadedEvent>(path, success);
}
}

void AssetManager::LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            auto shared = it->second;
            if (callback) callback(shared);
            PublishResourceLoaded(GetEventBus(), cache_key, true);
            return;
        }
    }

    dse::core::EventBus* event_bus = GetEventBus();
    dse::core::JobSystem* job_system = GetJobSystem();
    auto worker = [this, path, resolved_path, cache_key, callback, event_bus]() {
        std::vector<uint8_t> file_data;
        if (!LoadFileToMemory(path, file_data)) {
            DEBUG_LOG_ERROR("Failed to read texture file async: {}", path);
            if (callback) {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                pending_main_thread_callbacks_.push_back([callback]() { callback(nullptr); });
            }
            PublishResourceLoaded(event_bus, cache_key, false);
            return;
        }

        int width, height, channels;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            stbi_set_flip_vertically_on_load(rhi_device_ ? rhi_device_->NeedsTextureYFlip() : true);
        }
        unsigned char* data = stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4);
        
        if (!data) {
            DEBUG_LOG_ERROR("Failed to async load texture: {}, resolved: {}", path, resolved_path);
            if (callback) {
                std::lock_guard<std::mutex> callback_lock(callback_mutex_);
                pending_main_thread_callbacks_.push_back([callback]() {
                    callback(nullptr);
                });
                pending_main_thread_callbacks_.push_back([resolved_path, event_bus]() {
                    PublishResourceLoaded(event_bus, resolved_path, false);
                });
                pending_callbacks_high_watermark_ = std::max(pending_callbacks_high_watermark_, pending_main_thread_callbacks_.size());
                if (!callback_backlog_warned_ && pending_main_thread_callbacks_.size() >= 1024) {
                    callback_backlog_warned_ = true;
                    DEBUG_LOG_WARN("Async callback backlog is high: {}", pending_main_thread_callbacks_.size());
                }
            }
            return;
        }

        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        pending_main_thread_callbacks_.push_back([this, callback, cache_key, resolved_path, width, height, channels, data, event_bus]() {
            unsigned int handle = 0;
            {
                std::lock_guard<std::mutex> config_lock(config_mutex_);
                if (rhi_device_) {
                    handle = rhi_device_->CreateTexture2D(width, height, data, true);
                }
            }
            stbi_image_free(data);
            if (handle == 0) {
                if (callback) {
                    callback(nullptr);
                }
                PublishResourceLoaded(event_bus, resolved_path, false);
                return;
            }
            auto tex = std::make_shared<TextureAsset>(resolved_path, handle, width, height, channels);
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                textures_[cache_key] = tex;
            }
            if (callback) {
                callback(tex);
            }
            PublishResourceLoaded(event_bus, resolved_path, true);
        });
        pending_callbacks_high_watermark_ = std::max(pending_callbacks_high_watermark_, pending_main_thread_callbacks_.size());
        if (!callback_backlog_warned_ && pending_main_thread_callbacks_.size() >= 1024) {
            callback_backlog_warned_ = true;
            DEBUG_LOG_WARN("Async callback backlog is high: {}", pending_main_thread_callbacks_.size());
        }
    };

    if (job_system) {
        job_system->Execute(worker);
    } else {
        worker();
    }
}

std::shared_ptr<MaterialAsset> AssetManager::CreateMaterialInstance(const std::string& name) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    unsigned int material_id = next_material_id_++;
    auto material = std::make_shared<MaterialAsset>(material_id, name);
    materials_[material_id] = material;
    material_names_[material_id] = name;
    return material;
}

std::shared_ptr<MaterialAsset> AssetManager::LoadMaterialInstanceFromDmat(const std::string& dmat_path, std::size_t material_index) {
    const std::string bin_path = dmat_path + ".bin";

    auto load_textures = [this](std::shared_ptr<MaterialAsset>& mat,
                                const std::string& albedo, const std::string& normal,
                                const std::string& mr,     const std::string& emissive,
                                const std::string& occ) {
        auto try_tex = [this](const std::string& p) -> unsigned int {
            if (p.empty()) return 0;
            auto t = LoadTexture(p);
            return t ? t->GetHandle() : 0;
        };
        MaterialAsset::TextureSlots s;
        s.albedo             = try_tex(albedo);
        s.normal             = try_tex(normal);
        s.metallic_roughness = try_tex(mr);
        s.emissive           = try_tex(emissive);
        s.occlusion          = try_tex(occ);
        mat->SetTextureSlots(s);
        if (s.albedo) mat->SetTextureHandle(s.albedo);
    };

    // 尝试二进制缓存
    {
        const int64_t dmat_t = DmatGetMtime(dmat_path);
        const int64_t bin_t  = DmatGetMtime(bin_path);
        if (dmat_t > 0 && bin_t >= dmat_t) {
            std::vector<DmatBinEntry> entries;
            if (LoadDmatBin(bin_path, entries) && material_index < entries.size()) {
                const auto& e = entries[material_index];
                const std::string name = e.name.empty()
                    ? (std::filesystem::path(dmat_path).stem().string() + "_" + std::to_string(material_index))
                    : e.name;
                auto material = CreateMaterialInstance(name);
                if (material) {
                    material->SetBaseColor(glm::vec4(e.base_color[0], e.base_color[1],
                                                     e.base_color[2], e.base_color[3]));
                    material->SetEmissiveColor(glm::vec3(e.emissive[0], e.emissive[1], e.emissive[2]));
                    MaterialAsset::RasterOverrides raster;
                    raster.double_sided = e.double_sided != 0;
                    material->SetRasterOverrides(raster);
                    material->SetBlendMode(MaterialBlendMode::Opaque);
                    MaterialAsset::ScalarOverrides scalars;
                    scalars.metallic        = e.metallic;
                    scalars.roughness       = e.roughness;
                    scalars.ao              = e.ao;
                    scalars.normal_strength = e.normal_scale;
                    scalars.alpha_cutoff    = e.alpha_cutoff;
                    scalars.alpha_test      = e.alpha_test != 0;
                    material->SetScalarOverrides(scalars);
                    load_textures(material, e.tex_albedo, e.tex_normal,
                                  e.tex_mr, e.tex_emissive, e.tex_occlusion);
                    return material;
                }
            }
        }
    }

    // 回退到 JSON 解析
    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(dmat_path, file_data)) {
        return nullptr;
    }

    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("materials") || !doc["materials"].IsArray()) {
        return nullptr;
    }

    const auto& materials = doc["materials"];
    if (material_index >= materials.Size()) {
        return nullptr;
    }

    const auto& mat = materials[static_cast<rapidjson::SizeType>(material_index)];
    if (!mat.IsObject()) {
        return nullptr;
    }

    const std::string name = (mat.HasMember("name") && mat["name"].IsString())
        ? mat["name"].GetString()
        : (std::filesystem::path(dmat_path).stem().string() + "_" + std::to_string(material_index));
    auto material = CreateMaterialInstance(name);
    if (!material) {
        return nullptr;
    }

    if (mat.HasMember("base_color") && mat["base_color"].IsArray() && mat["base_color"].Size() >= 4) {
        material->SetBaseColor(glm::vec4(
            mat["base_color"][0].GetFloat(),
            mat["base_color"][1].GetFloat(),
            mat["base_color"][2].GetFloat(),
            mat["base_color"][3].GetFloat()));
    }
    if (mat.HasMember("emissive") && mat["emissive"].IsArray() && mat["emissive"].Size() >= 3) {
        material->SetEmissiveColor(glm::vec3(
            mat["emissive"][0].GetFloat(),
            mat["emissive"][1].GetFloat(),
            mat["emissive"][2].GetFloat()));
    }
    MaterialAsset::RasterOverrides raster = material->GetRasterOverrides();
    if (mat.HasMember("double_sided") && mat["double_sided"].IsBool()) {
        raster.double_sided = mat["double_sided"].GetBool();
    }
    material->SetRasterOverrides(raster);
    material->SetBlendMode(MaterialBlendMode::Opaque);

    MaterialAsset::ScalarOverrides scalars = material->GetScalarOverrides();
    if (mat.HasMember("metallic") && mat["metallic"].IsNumber()) {
        scalars.metallic = mat["metallic"].GetFloat();
    }
    if (mat.HasMember("roughness") && mat["roughness"].IsNumber()) {
        scalars.roughness = mat["roughness"].GetFloat();
    }
    if (mat.HasMember("occlusion_strength") && mat["occlusion_strength"].IsNumber()) {
        scalars.ao = mat["occlusion_strength"].GetFloat();
    }
    if (mat.HasMember("normal_scale") && mat["normal_scale"].IsNumber()) {
        scalars.normal_strength = mat["normal_scale"].GetFloat();
    }
    if (mat.HasMember("alpha_cutoff") && mat["alpha_cutoff"].IsNumber()) {
        scalars.alpha_cutoff = mat["alpha_cutoff"].GetFloat();
    }
    if (mat.HasMember("alpha_test") && mat["alpha_test"].IsBool()) {
        scalars.alpha_test = mat["alpha_test"].GetBool();
    }
    material->SetScalarOverrides(scalars);

    auto try_load_texture = [this](const rapidjson::Value& object, const char* key) -> unsigned int {
        if (!object.HasMember(key) || !object[key].IsString()) return 0;
        const char* texture_path = object[key].GetString();
        if (!texture_path || texture_path[0] == '\0') return 0;
        auto texture = LoadTexture(texture_path);
        return texture ? texture->GetHandle() : 0;
    };
    MaterialAsset::TextureSlots slots;
    slots.albedo             = try_load_texture(mat, "base_color_texture");
    slots.normal             = try_load_texture(mat, "normal_texture");
    slots.metallic_roughness = try_load_texture(mat, "metallic_roughness_texture");
    slots.emissive           = try_load_texture(mat, "emissive_texture");
    slots.occlusion          = try_load_texture(mat, "occlusion_texture");
    material->SetTextureSlots(slots);
    if (slots.albedo) material->SetTextureHandle(slots.albedo);

    SaveDmatBin(dmat_path, bin_path, doc);

    return material;
}

std::shared_ptr<MaterialAsset> AssetManager::GetMaterialInstance(unsigned int material_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = materials_.find(material_id);
    if (it == materials_.end()) {
        return nullptr;
    }
    if (auto material = it->second.lock()) {
        return material;
    }

    // 大批量创建场景中调用方可能只缓存 ID。为保持 ID 查询稳定性，
    // 当管理器持有足够多的材质记录时允许按记录惰性重建轻量材质实例；
    // 小规模临时实例仍保持弱引用语义，外部释放后返回 nullptr。
    constexpr std::size_t kMaterialRehydrateThreshold = 64;
    if (material_names_.size() < kMaterialRehydrateThreshold) {
        return nullptr;
    }
    auto name_it = material_names_.find(material_id);
    if (name_it == material_names_.end()) {
        return nullptr;
    }
    auto material = std::make_shared<MaterialAsset>(material_id, name_it->second);
    it->second = material;
    return material;
}

std::vector<unsigned int> AssetManager::ListMaterialInstanceIds() {
    std::vector<unsigned int> ids;
    std::lock_guard<std::mutex> lock(cache_mutex_);
    constexpr std::size_t kMaterialRehydrateThreshold = 64;
    ids.reserve(materials_.size());
    for (const auto& pair : materials_) {
        if (!pair.second.expired() || material_names_.size() >= kMaterialRehydrateThreshold) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

void AssetManager::UnloadUnused() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (auto it = textures_.begin(); it != textures_.end(); ) {
        if (it->second.use_count() <= 1) {
            it = textures_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = shaders_.begin(); it != shaders_.end(); ) {
        if (it->second.expired()) {
            it = shaders_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = audio_clips_.begin(); it != audio_clips_.end(); ) {
        if (it->second.expired()) {
            it = audio_clips_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = dmeshes_.begin(); it != dmeshes_.end(); ) {
        if (it->second.expired()) {
            it = dmeshes_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = danims_.begin(); it != danims_.end(); ) {
        if (it->second.expired()) {
            it = danims_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = materials_.begin(); it != materials_.end(); ) {
        if (it->second.expired()) {
            material_names_.erase(it->first);
            it = materials_.erase(it);
        } else {
            ++it;
        }
    }
}

void AssetManager::ReleaseGpuResources() {
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);

    RhiDevice* device = nullptr;
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        device = rhi_device_;
    }

    if (!device) {
        textures_.clear();
        cubemaps_.clear();
        shaders_.clear();
        gpu_texture_handles_.clear();
        gpu_cubemap_handles_.clear();
        gpu_shader_handles_.clear();
        materials_.clear();
        material_names_.clear();
        return;
    }

    for (const unsigned int handle : gpu_texture_handles_) {
        if (handle != 0) {
            device->DeleteTexture(handle);
        }
    }
    gpu_texture_handles_.clear();
    textures_.clear();

    for (const unsigned int handle : gpu_cubemap_handles_) {
        if (handle != 0) {
            device->DeleteTexture(handle);
        }
    }
    gpu_cubemap_handles_.clear();
    cubemaps_.clear();

    for (const unsigned int handle : gpu_shader_handles_) {
        if (handle != 0) {
            device->DeleteShaderProgram(handle);
        }
    }
    gpu_shader_handles_.clear();
    shaders_.clear();
    materials_.clear();
    material_names_.clear();
}

void AssetManager::PumpMainThreadCallbacks(std::size_t max_callbacks) {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        std::size_t consume_count = std::min(max_callbacks, pending_main_thread_callbacks_.size());
        callbacks.reserve(consume_count);
        for (std::size_t i = 0; i < consume_count; ++i) {
            callbacks.push_back(std::move(pending_main_thread_callbacks_.front()));
            pending_main_thread_callbacks_.pop_front();
        }
        if (callback_backlog_warned_ && pending_main_thread_callbacks_.size() < 256) {
            callback_backlog_warned_ = false;
        }
    }
    for (auto& callback : callbacks) {
        callback();
    }
}

std::size_t AssetManager::PendingMainThreadCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return pending_main_thread_callbacks_.size();
}

std::size_t AssetManager::PendingMainThreadCallbacksHighWatermark() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return pending_callbacks_high_watermark_;
}

// ============================================================
// 异步加载：Dmesh / Danim / Dskel / AudioClip / Material
// ============================================================

namespace {
template<typename AssetT, typename CacheMap>
void AsyncLoadBinaryAsset(
    AssetManager* mgr,
    CacheMap& cache,
    std::mutex& cache_mutex,
    std::mutex& callback_mutex,
    std::deque<std::function<void()>>& pending_callbacks,
    dse::core::JobSystem* job_system,
    dse::core::EventBus* event_bus,
    const std::string& path,
    const std::string& cache_key,
    std::function<void(std::shared_ptr<AssetT>)> callback)
{
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(cache_key);
        if (it != cache.end()) {
            if (auto shared = it->second.lock()) {
                if (callback) callback(shared);
                PublishResourceLoaded(event_bus, cache_key, true);
                return;
            }
        }
    }

    auto worker = [mgr, &cache, &cache_mutex, &callback_mutex, &pending_callbacks, path, cache_key, callback, event_bus]() {
        std::vector<uint8_t> file_data;
        if (!mgr->LoadFileToMemory(path, file_data)) {
            DEBUG_LOG_ERROR("Failed to async load binary asset: {}", path);
            if (callback) {
                std::lock_guard<std::mutex> lock(callback_mutex);
                pending_callbacks.push_back([callback]() { callback(nullptr); });
            }
            PublishResourceLoaded(event_bus, cache_key, false);
            return;
        }

        auto asset = std::make_shared<AssetT>(path, std::move(file_data));
        std::lock_guard<std::mutex> lock(callback_mutex);
        pending_callbacks.push_back([&cache, &cache_mutex, cache_key, asset, callback, event_bus]() {
            {
                std::lock_guard<std::mutex> cachelock(cache_mutex);
                cache[cache_key] = asset;
            }
            if (callback) {
                callback(asset);
            }
            PublishResourceLoaded(event_bus, cache_key, true);
        });
    };

    if (job_system) {
        job_system->Execute(worker);
    } else {
        worker();
    }
}
} // namespace

void AssetManager::LoadDmeshAsync(const std::string& path, std::function<void(std::shared_ptr<DmeshAsset>)> callback) {
    const std::string cache_key = NormalizeAssetPath(path).empty() ? NormalizePath(path) : NormalizeAssetPath(path);
    AsyncLoadBinaryAsset<DmeshAsset>(this, dmeshes_, cache_mutex_, callback_mutex_,
        pending_main_thread_callbacks_, GetJobSystem(), GetEventBus(), path, cache_key, std::move(callback));
}

void AssetManager::LoadDanimAsync(const std::string& path, std::function<void(std::shared_ptr<DanimAsset>)> callback) {
    const std::string cache_key = NormalizeAssetPath(path).empty() ? NormalizePath(path) : NormalizeAssetPath(path);
    AsyncLoadBinaryAsset<DanimAsset>(this, danims_, cache_mutex_, callback_mutex_,
        pending_main_thread_callbacks_, GetJobSystem(), GetEventBus(), path, cache_key, std::move(callback));
}

void AssetManager::LoadDskelAsync(const std::string& path, std::function<void(std::shared_ptr<DskelAsset>)> callback) {
    const std::string cache_key = NormalizeAssetPath(path).empty() ? NormalizePath(path) : NormalizeAssetPath(path);
    AsyncLoadBinaryAsset<DskelAsset>(this, dskels_, cache_mutex_, callback_mutex_,
        pending_main_thread_callbacks_, GetJobSystem(), GetEventBus(), path, cache_key, std::move(callback));
}

void AssetManager::LoadAudioClipAsync(const std::string& path, std::function<void(std::shared_ptr<AudioClipAsset>)> callback) {
    const std::string cache_key = NormalizeAssetPath(path).empty() ? NormalizePath(path) : NormalizeAssetPath(path);
    AsyncLoadBinaryAsset<AudioClipAsset>(this, audio_clips_, cache_mutex_, callback_mutex_,
        pending_main_thread_callbacks_, GetJobSystem(), GetEventBus(), path, cache_key, std::move(callback));
}

void AssetManager::LoadMaterialAsync(const std::string& dmat_path, std::size_t material_index,
                                     std::function<void(std::shared_ptr<MaterialAsset>)> callback) {
    dse::core::JobSystem* job_system = GetJobSystem();
    dse::core::EventBus* event_bus = GetEventBus();

    auto worker = [this, dmat_path, material_index, callback, event_bus]() {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        pending_main_thread_callbacks_.push_back([this, dmat_path, material_index, callback, event_bus]() {
            auto mat = LoadMaterialInstanceFromDmat(dmat_path, material_index);
            if (callback) {
                callback(mat);
            }
            PublishResourceLoaded(event_bus, dmat_path, mat != nullptr);
        });
    };

    if (job_system) {
        job_system->Execute(worker);
    } else {
        worker();
    }
}

// ============================================================
// LRU 淘汰与内存预算
// ============================================================

void AssetManager::TouchLru(const std::string& cache_key, std::size_t estimated_bytes) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = lru_entries_.find(cache_key);
    if (it != lru_entries_.end()) {
        it->second.last_access = std::chrono::steady_clock::now();
        return;
    }
    LruEntry entry;
    entry.cache_key = cache_key;
    entry.estimated_bytes = estimated_bytes;
    entry.last_access = std::chrono::steady_clock::now();
    lru_entries_[cache_key] = entry;
    estimated_memory_usage_ += estimated_bytes;
}

void AssetManager::RemoveLru(const std::string& cache_key) {
    auto it = lru_entries_.find(cache_key);
    if (it != lru_entries_.end()) {
        if (estimated_memory_usage_ >= it->second.estimated_bytes) {
            estimated_memory_usage_ -= it->second.estimated_bytes;
        } else {
            estimated_memory_usage_ = 0;
        }
        lru_entries_.erase(it);
    }
}

void AssetManager::SetMemoryBudget(std::size_t budget_bytes) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    memory_budget_bytes_ = budget_bytes;
}

std::size_t AssetManager::EstimatedMemoryUsage() const {
    // cache_mutex_ 不是 mutable，但此处仅读取原子级别可接受的估算值
    // 为保持 const 正确性，使用 const_cast（内部实现细节，不影响外部语义）
    auto& self = const_cast<AssetManager&>(*this);
    std::lock_guard<std::mutex> lock(self.cache_mutex_);
    return estimated_memory_usage_;
}

std::size_t AssetManager::EvictLRU() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (memory_budget_bytes_ == 0 || estimated_memory_usage_ <= memory_budget_bytes_) {
        return 0;
    }

    // 收集所有 LRU 条目并按 last_access 排序（最早的优先淘汰）
    std::vector<LruEntry*> entries;
    entries.reserve(lru_entries_.size());
    for (auto& pair : lru_entries_) {
        entries.push_back(&pair.second);
    }
    std::sort(entries.begin(), entries.end(), [](const LruEntry* a, const LruEntry* b) {
        return a->last_access < b->last_access;
    });

    std::size_t evicted = 0;
    for (auto* entry : entries) {
        if (estimated_memory_usage_ <= memory_budget_bytes_) {
            break;
        }
        const std::string& key = entry->cache_key;

        // 尝试从各缓存表中驱逐（仅驱逐已无外部引用的条目）
        bool evicted_entry = false;
        auto tex_it = textures_.find(key);
        if (tex_it != textures_.end() && tex_it->second.use_count() <= 1) {
            textures_.erase(tex_it);
            evicted_entry = true;
        }
        auto cubemap_it = cubemaps_.find(key);
        if (cubemap_it != cubemaps_.end() && cubemap_it->second.expired()) {
            cubemaps_.erase(cubemap_it);
            evicted_entry = true;
        }
        auto dmesh_it = dmeshes_.find(key);
        if (dmesh_it != dmeshes_.end() && dmesh_it->second.expired()) {
            dmeshes_.erase(dmesh_it);
            evicted_entry = true;
        }
        auto danim_it = danims_.find(key);
        if (danim_it != danims_.end() && danim_it->second.expired()) {
            danims_.erase(danim_it);
            evicted_entry = true;
        }
        auto dskel_it = dskels_.find(key);
        if (dskel_it != dskels_.end() && dskel_it->second.expired()) {
            dskels_.erase(dskel_it);
            evicted_entry = true;
        }
        auto audio_it = audio_clips_.find(key);
        if (audio_it != audio_clips_.end() && audio_it->second.expired()) {
            audio_clips_.erase(audio_it);
            evicted_entry = true;
        }

        if (evicted_entry) {
            if (estimated_memory_usage_ >= entry->estimated_bytes) {
                estimated_memory_usage_ -= entry->estimated_bytes;
            } else {
                estimated_memory_usage_ = 0;
            }
            ++evicted;
            // RemoveLru inline — will erase from map after loop
        }
    }

    // 清理已驱逐条目的 LRU 记录
    for (auto it = lru_entries_.begin(); it != lru_entries_.end(); ) {
        const std::string& key = it->first;
        bool still_alive = false;
        if (textures_.count(key)) still_alive = true;
        if (cubemaps_.count(key)) still_alive = true;
        if (dmeshes_.count(key)) still_alive = true;
        if (danims_.count(key)) still_alive = true;
        if (dskels_.count(key)) still_alive = true;
        if (audio_clips_.count(key)) still_alive = true;
        if (!still_alive) {
            it = lru_entries_.erase(it);
        } else {
            ++it;
        }
    }

    return evicted;
}

// ============================================================
// 热重载：文件监听
// ============================================================

void AssetManager::StartFileWatcher() {
    if (file_watcher_running_.load()) {
        return;
    }
    file_watcher_running_.store(true);
    file_watcher_thread_ = std::thread(&AssetManager::FileWatcherLoop, this);
}

void AssetManager::StopFileWatcher() {
    file_watcher_running_.store(false);
    if (file_watcher_thread_.joinable()) {
        file_watcher_thread_.join();
    }
}

void AssetManager::FileWatcherLoop() {
#ifdef _WIN32
    const std::string data_root = GetDataRoot();
    if (data_root.empty()) {
        DEBUG_LOG_WARN("FileWatcher: data root is empty, watcher exiting");
        file_watcher_running_.store(false);
        return;
    }

    std::wstring wide_path;
    {
        std::filesystem::path fs_path(data_root);
        wide_path = fs_path.wstring();
    }

    HANDLE dir_handle = CreateFileW(
        wide_path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (dir_handle == INVALID_HANDLE_VALUE) {
        DEBUG_LOG_ERROR("FileWatcher: failed to open directory handle for {}", data_root);
        file_watcher_running_.store(false);
        return;
    }

    DEBUG_LOG_INFO("FileWatcher: started monitoring {}", data_root);

    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        DEBUG_LOG_ERROR("FileWatcher: failed to create event object");
        CloseHandle(dir_handle);
        file_watcher_running_.store(false);
        return;
    }

    alignas(DWORD) char buffer[4096];
    while (file_watcher_running_.load()) {
        ResetEvent(overlapped.hEvent);
        BOOL result = ReadDirectoryChangesW(
            dir_handle,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            nullptr,
            &overlapped,
            nullptr);

        if (!result && GetLastError() != ERROR_IO_PENDING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // 每 200ms 检查一次是否需要退出
        while (file_watcher_running_.load()) {
            DWORD wait_result = WaitForSingleObject(overlapped.hEvent, 200);
            if (wait_result == WAIT_OBJECT_0) break;   // IO 完成
            if (wait_result == WAIT_TIMEOUT) continue;  // 超时，检查 running flag
            break; // 出错
        }

        if (!file_watcher_running_.load()) {
            CancelIo(dir_handle);
            break;
        }

        DWORD bytes_returned = 0;
        if (!GetOverlappedResult(dir_handle, &overlapped, &bytes_returned, FALSE) || bytes_returned == 0) {
            continue;
        }

        DWORD offset = 0;
        do {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer + offset);
            if (info->Action == FILE_ACTION_MODIFIED || info->Action == FILE_ACTION_ADDED) {
                std::wstring wname(info->FileName, info->FileNameLength / sizeof(WCHAR));
                std::string relative = std::filesystem::path(wname).generic_string();

                {
                    std::lock_guard<std::mutex> lock(hot_reload_mutex_);
                    if (std::find(pending_hot_reloads_.begin(), pending_hot_reloads_.end(), relative) == pending_hot_reloads_.end()) {
                        pending_hot_reloads_.push_back(relative);
                        DEBUG_LOG_INFO("FileWatcher: queued hot-reload for {}", relative);
                    }
                }
            }
            if (info->NextEntryOffset == 0) break;
            offset += info->NextEntryOffset;
        } while (offset < bytes_returned);
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(dir_handle);
#else
    DEBUG_LOG_WARN("FileWatcher: not implemented on this platform");
    while (file_watcher_running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
#endif
    DEBUG_LOG_INFO("FileWatcher: stopped");
}

std::size_t AssetManager::PumpHotReloads() {
    std::vector<std::string> reloads;
    {
        std::lock_guard<std::mutex> lock(hot_reload_mutex_);
        reloads.swap(pending_hot_reloads_);
    }

    if (reloads.empty()) {
        return 0;
    }

    std::size_t reloaded = 0;
    for (const auto& relative_path : reloads) {
        const std::string logical = NormalizeAssetPath(relative_path);
        const std::string cache_key = logical.empty() ? NormalizePath(relative_path) : logical;

        bool did_reload = false;

        // 纹理热重载
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto tex_it = textures_.find(cache_key);
            if (tex_it != textures_.end()) {
                textures_.erase(tex_it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_tex = LoadTexture(relative_path);
            if (reloaded_tex) {
                DEBUG_LOG_INFO("HotReload: reloaded texture {}", relative_path);
                ++reloaded;
            }
            dse::core::EventBus* bus = GetEventBus();
            if (bus) {
                bus->Publish<dse::core::ResourceLoadedEvent>(cache_key, reloaded_tex != nullptr);
            }
            continue;
        }

        // Dmesh 热重载
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = dmeshes_.find(cache_key);
            if (it != dmeshes_.end()) {
                dmeshes_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadDmesh(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded dmesh {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        // Danim 热重载
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = danims_.find(cache_key);
            if (it != danims_.end()) {
                danims_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadDanim(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded danim {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        // Dskel 热重载
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = dskels_.find(cache_key);
            if (it != dskels_.end()) {
                dskels_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadDskel(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded dskel {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        // AudioClip 热重载
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = audio_clips_.find(cache_key);
            if (it != audio_clips_.end()) {
                audio_clips_.erase(it);
                RemoveLru(cache_key);
                did_reload = true;
            }
        }
        if (did_reload) {
            auto reloaded_asset = LoadAudioClip(relative_path);
            if (reloaded_asset) {
                DEBUG_LOG_INFO("HotReload: reloaded audio clip {}", relative_path);
                ++reloaded;
            }
            continue;
        }

        DEBUG_LOG_INFO("HotReload: no cached asset matched for {}", relative_path);
    }

    return reloaded;
}
