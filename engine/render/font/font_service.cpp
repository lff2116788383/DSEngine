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

    // 解析字体
    stbtt_fontinfo font_info;
    int font_offset = stbtt_GetFontOffsetForIndex(ttf_data.data(), 0);
    if (font_offset < 0 || !stbtt_InitFont(&font_info, ttf_data.data(), font_offset)) {
        DEBUG_LOG_ERROR("FontService: invalid TTF data '{}'", ttf_path);
        return false;
    }

    const float font_size = config_.default_font_size;
    float scale = stbtt_ScaleForPixelHeight(&font_info, font_size);

    // 字体度量
    int ascent_raw, descent_raw, line_gap_raw;
    stbtt_GetFontVMetrics(&font_info, &ascent_raw, &descent_raw, &line_gap_raw);
    float ascent = ascent_raw * scale;
    float descent = descent_raw * scale;
    float line_height = (ascent_raw - descent_raw + line_gap_raw) * scale;

    // 收集码点
    std::vector<int> codepoints;
    for (int i = 0; i < config_.num_codepoints; ++i) {
        codepoints.push_back(config_.first_codepoint + i);
    }
    for (int cp : extra_codepoints) {
        codepoints.push_back(cp);
    }

    // SDF 光栅化
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
    const int atlas_w = config_.default_atlas_width;
    const int atlas_h = config_.default_atlas_height;
    stbrp_context pack_ctx;
    std::vector<stbrp_node> pack_nodes(atlas_w);
    stbrp_init_target(&pack_ctx, atlas_w, atlas_h, pack_nodes.data(), static_cast<int>(pack_nodes.size()));
    stbrp_pack_rects(&pack_ctx, rects.data(), static_cast<int>(rects.size()));

    // 生成 SDF 图集 + 字形度量
    std::vector<uint8_t> atlas(atlas_w * atlas_h, 0);
    const float inv_w = 1.0f / static_cast<float>(atlas_w);
    const float inv_h = 1.0f / static_cast<float>(atlas_h);

    auto instance = std::make_unique<FontInstance>();
    instance->font_id = font_id;
    instance->file_path = ttf_path;
    instance->sdf_mode = true;

    // 设置 TrueTypeFont 度量（无需 LoadFromMemory，避免双重解析）
    instance->font.SetMetrics(font_size, ascent, descent, line_height, atlas_w, atlas_h);

    int packed_count = 0;
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

        // 计算 advance 和 bearing
        int advance_raw, lsb;
        stbtt_GetCodepointHMetrics(&font_info, gs.codepoint, &advance_raw, &lsb);

        // 注入字形度量到 TrueTypeFont（UV 指向 SDF atlas）
        GlyphMetrics gm;
        gm.codepoint = gs.codepoint;
        gm.advance_x = advance_raw * scale;
        gm.bearing_x = static_cast<float>(gs.xoff);
        gm.bearing_y = static_cast<float>(gs.yoff);
        gm.width = static_cast<float>(gs.w);
        gm.height = static_cast<float>(gs.h);
        gm.uv = glm::vec4(
            dst_x * inv_w,
            dst_y * inv_h,
            (dst_x + gs.w) * inv_w,
            (dst_y + gs.h) * inv_h
        );
        instance->font.SetGlyph(gs.codepoint, gm);
        ++packed_count;
    }

    // 存储 SDF atlas 到 TrueTypeFont（供潜在 CPU 回读）
    instance->font.SetAtlasBitmap(std::move(atlas), atlas_w, atlas_h);

    // 上传 GPU 纹理 (R8 → RGBA8，A 通道存 SDF)
    std::vector<uint8_t> rgba(atlas_w * atlas_h * 4);
    const auto& atlas_ref = instance->font.GetAtlasBitmap();
    for (int i = 0; i < atlas_w * atlas_h; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = atlas_ref[i];
    }

    if (texture_create_fn_) {
        instance->gpu_texture_handle = texture_create_fn_(atlas_w, atlas_h, rgba.data(), true);
    }

    if (instance->gpu_texture_handle == 0) {
        DEBUG_LOG_WARN("FontService: GPU texture creation failed for '{}'", font_id);
    }

    DEBUG_LOG_INFO("FontService: loaded '{}' (SDF {}x{}, {}/{} glyphs packed, tex={})",
                   font_id, atlas_w, atlas_h, packed_count, codepoints.size(),
                   instance->gpu_texture_handle);

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
