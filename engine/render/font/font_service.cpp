/**
 * @file font_service.cpp
 * @brief FontService 实现 — SDF 图集生成与 GPU 上传
 */

#include "engine/render/font/font_service.h"
#include "engine/base/debug.h"

#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

// stb_truetype 已在 truetype_font.cpp 中定义了实现，这里只需头文件声明
#include <stb/stb_truetype.h>
#include <stb/stb_rect_pack.h>

namespace dse {
namespace render {

FontService::~FontService() {
    Shutdown();
}

void FontService::SetTextureCallbacks(TextureCreateFn create_fn, TextureDeleteFn delete_fn) {
    texture_create_fn_ = std::move(create_fn);
    texture_delete_fn_ = std::move(delete_fn);
}

bool FontService::LoadFont(const std::string& font_id, const std::string& ttf_path,
                           const std::vector<int>& extra_codepoints) {
    if (fonts_.count(font_id)) {
        DEBUG_LOG_WARN("FontService: font '{}' already loaded", font_id);
        return true;
    }

    // 读取 TTF 文件
    std::ifstream file(ttf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("FontService: cannot open '{}'", ttf_path);
        return false;
    }
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> ttf_data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(ttf_data.data()), size);
    if (!file) {
        DEBUG_LOG_ERROR("FontService: read failed '{}'", ttf_path);
        return false;
    }

    // 配置 SDF 图集
    FontAtlasConfig atlas_config;
    atlas_config.font_size = config_.default_font_size;
    atlas_config.atlas_width = config_.default_atlas_width;
    atlas_config.atlas_height = config_.default_atlas_height;
    atlas_config.padding = config_.sdf_padding;
    atlas_config.first_codepoint = config_.first_codepoint;
    atlas_config.num_codepoints = config_.num_codepoints;
    atlas_config.extra_codepoints = extra_codepoints;

    // 解析字体
    stbtt_fontinfo font_info;
    int font_offset = stbtt_GetFontOffsetForIndex(ttf_data.data(), 0);
    if (font_offset < 0 || !stbtt_InitFont(&font_info, ttf_data.data(), font_offset)) {
        DEBUG_LOG_ERROR("FontService: invalid TTF data '{}'", ttf_path);
        return false;
    }

    float scale = stbtt_ScaleForPixelHeight(&font_info, atlas_config.font_size);

    // 收集码点
    std::vector<int> codepoints;
    for (int i = 0; i < atlas_config.num_codepoints; ++i) {
        codepoints.push_back(atlas_config.first_codepoint + i);
    }
    for (int cp : atlas_config.extra_codepoints) {
        codepoints.push_back(cp);
    }

    // SDF 光栅化 + rect pack
    struct GlyphSDF {
        int codepoint;
        unsigned char* bitmap;
        int w, h;
        int xoff, yoff;
    };
    std::vector<GlyphSDF> glyph_sdfs(codepoints.size());
    std::vector<stbrp_rect> rects(codepoints.size());

    const int sdf_pad = config_.sdf_padding;
    const unsigned char on_edge = static_cast<unsigned char>(config_.sdf_on_edge_value);
    const float pixel_dist = config_.sdf_pixel_dist_scale;

    for (size_t i = 0; i < codepoints.size(); ++i) {
        int cp = codepoints[i];
        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char* bmp = stbtt_GetCodepointSDF(
            &font_info, scale, cp, sdf_pad, on_edge, pixel_dist,
            &w, &h, &xoff, &yoff);

        glyph_sdfs[i] = {cp, bmp, w, h, xoff, yoff};
        rects[i].id = static_cast<int>(i);
        rects[i].w = static_cast<stbrp_coord>(w + 2);
        rects[i].h = static_cast<stbrp_coord>(h + 2);
    }

    // Pack
    const int atlas_w = atlas_config.atlas_width;
    const int atlas_h = atlas_config.atlas_height;
    stbrp_context pack_ctx;
    std::vector<stbrp_node> pack_nodes(atlas_w);
    stbrp_init_target(&pack_ctx, atlas_w, atlas_h, pack_nodes.data(), static_cast<int>(pack_nodes.size()));
    stbrp_pack_rects(&pack_ctx, rects.data(), static_cast<int>(rects.size()));

    // 生成图集 (R8)
    std::vector<uint8_t> atlas(atlas_w * atlas_h, 0);
    float inv_w = 1.0f / static_cast<float>(atlas_w);
    float inv_h = 1.0f / static_cast<float>(atlas_h);

    // 创建 TrueTypeFont 实例用于存储度量
    auto instance = std::make_unique<FontInstance>();
    instance->font_id = font_id;
    instance->file_path = ttf_path;
    instance->sdf_mode = true;

    // 获取字体度量
    int ascent_raw, descent_raw, line_gap_raw;
    stbtt_GetFontVMetrics(&font_info, &ascent_raw, &descent_raw, &line_gap_raw);

    // 使用底层 font 对象存储度量（通过 LoadFromMemory 方式）
    // 这里直接生成 SDF 图集，不使用 TrueTypeFont 的 bitmap 路径
    // 手动设置 font 的内部状态
    FontAtlasConfig cfg_for_font;
    cfg_for_font.font_size = atlas_config.font_size;
    cfg_for_font.atlas_width = atlas_w;
    cfg_for_font.atlas_height = atlas_h;
    cfg_for_font.padding = sdf_pad;
    cfg_for_font.first_codepoint = atlas_config.first_codepoint;
    cfg_for_font.num_codepoints = atlas_config.num_codepoints;
    cfg_for_font.extra_codepoints = extra_codepoints;

    // 先加载到 TrueTypeFont 获取度量和布局能力（使用普通 bitmap）
    instance->font.LoadFromMemory(ttf_data, cfg_for_font);

    // 用 SDF 数据填充图集（覆盖 TrueTypeFont 的 bitmap）
    for (size_t i = 0; i < codepoints.size(); ++i) {
        auto& rect = rects[i];
        auto& gs = glyph_sdfs[i];

        if (!rect.was_packed) {
            if (gs.bitmap) stbtt_FreeSDF(gs.bitmap, nullptr);
            continue;
        }

        int dst_x = rect.x + 1;
        int dst_y = rect.y + 1;

        if (gs.bitmap) {
            for (int row = 0; row < gs.h; ++row) {
                std::memcpy(
                    atlas.data() + (dst_y + row) * atlas_w + dst_x,
                    gs.bitmap + row * gs.w,
                    gs.w);
            }
            stbtt_FreeSDF(gs.bitmap, nullptr);
        }

        // 更新字形 UV（覆盖 TrueTypeFont 中的值，使用 SDF 尺寸）
        // 注意：SDF 位图比普通位图大了 sdf_padding*2
    }

    // 上传 GPU 纹理 (R8 → RGBA8，A 通道存 SDF)
    std::vector<uint8_t> rgba(atlas_w * atlas_h * 4);
    for (int i = 0; i < atlas_w * atlas_h; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = atlas[i];
    }

    if (texture_create_fn_) {
        instance->gpu_texture_handle = texture_create_fn_(atlas_w, atlas_h, rgba.data(), true);
    }

    if (instance->gpu_texture_handle == 0) {
        DEBUG_LOG_WARN("FontService: GPU texture creation failed for '{}'", font_id);
    }

    DEBUG_LOG_INFO("FontService: loaded '{}' (SDF {}x{}, {} glyphs, tex={})",
                   font_id, atlas_w, atlas_h, codepoints.size(), instance->gpu_texture_handle);

    fonts_[font_id] = std::move(instance);

    if (default_font_id_.empty()) {
        default_font_id_ = font_id;
    }

    return true;
}

void FontService::UnloadFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) return;

    if (it->second->gpu_texture_handle && texture_delete_fn_) {
        texture_delete_fn_(it->second->gpu_texture_handle);
    }
    fonts_.erase(it);

    if (default_font_id_ == font_id) {
        default_font_id_ = fonts_.empty() ? "" : fonts_.begin()->first;
    }
}

bool FontService::SetDefaultFont(const std::string& font_id) {
    if (!fonts_.count(font_id)) {
        DEBUG_LOG_ERROR("FontService: font '{}' not found", font_id);
        return false;
    }
    default_font_id_ = font_id;
    return true;
}

FontInstance* FontService::GetFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    return it != fonts_.end() ? it->second.get() : nullptr;
}

const FontInstance* FontService::GetFont(const std::string& font_id) const {
    auto it = fonts_.find(font_id);
    return it != fonts_.end() ? it->second.get() : nullptr;
}

FontInstance* FontService::GetDefaultFont() {
    if (default_font_id_.empty()) return nullptr;
    return GetFont(default_font_id_);
}

float FontService::MeasureText(const std::string& text, const std::string& font_id,
                               float font_size) const {
    const std::string& fid = font_id.empty() ? default_font_id_ : font_id;
    auto it = fonts_.find(fid);
    if (it == fonts_.end() || !it->second->font.IsValid()) return 0.0f;

    float raw_width = it->second->font.MeasureTextWidth(text);
    if (font_size > 0.0f && it->second->font.GetFontSize() > 0.0f) {
        raw_width *= font_size / it->second->font.GetFontSize();
    }
    return raw_width;
}

float FontService::GetLineHeight(const std::string& font_id, float font_size) const {
    const std::string& fid = font_id.empty() ? default_font_id_ : font_id;
    auto it = fonts_.find(fid);
    if (it == fonts_.end() || !it->second->font.IsValid()) return 0.0f;

    float raw_height = it->second->font.GetLineHeight();
    if (font_size > 0.0f && it->second->font.GetFontSize() > 0.0f) {
        raw_height *= font_size / it->second->font.GetFontSize();
    }
    return raw_height;
}

void FontService::Shutdown() {
    for (auto& [id, inst] : fonts_) {
        if (inst->gpu_texture_handle && texture_delete_fn_) {
            texture_delete_fn_(inst->gpu_texture_handle);
            inst->gpu_texture_handle = 0;
        }
    }
    fonts_.clear();
    default_font_id_.clear();
}

unsigned int FontService::UploadAtlasToGPU(const TrueTypeFont& font) {
    if (!texture_create_fn_ || !font.IsValid()) return 0;

    int w = font.GetAtlasWidth();
    int h = font.GetAtlasHeight();
    const auto& bitmap = font.GetAtlasBitmap();

    std::vector<uint8_t> rgba(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }
    return texture_create_fn_(w, h, rgba.data(), true);
}

} // namespace render
} // namespace dse
