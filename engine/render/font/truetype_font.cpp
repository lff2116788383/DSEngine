/**
 * @file truetype_font.cpp
 * @brief TrueType 字体图集生成实现
 */

#include "engine/render/font/truetype_font.h"
#include "engine/base/debug.h"

#include <fstream>
#include <cmath>
#include <cstring>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace dse {
namespace render {

bool TrueTypeFont::LoadFromFile(const std::string& ttf_path, const FontAtlasConfig& config) {
    std::ifstream file(ttf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("TrueTypeFont: 无法打开文件 {}", ttf_path);
        return false;
    }
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) {
        DEBUG_LOG_ERROR("TrueTypeFont: 读取文件失败 {}", ttf_path);
        return false;
    }
    return LoadFromMemory(data, config);
}

bool TrueTypeFont::LoadFromMemory(const std::vector<uint8_t>& ttf_data, const FontAtlasConfig& config) {
    // stb_truetype 至少需要 12 字节来读取 offset table
    if (ttf_data.size() < 12) {
        DEBUG_LOG_ERROR("TrueTypeFont: TTF 数据太短 ({}B)", ttf_data.size());
        return false;
    }

    int font_offset = stbtt_GetFontOffsetForIndex(ttf_data.data(), 0);
    if (font_offset < 0) {
        DEBUG_LOG_ERROR("TrueTypeFont: 无法获取字体偏移");
        return false;
    }

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, ttf_data.data(), font_offset)) {
        DEBUG_LOG_ERROR("TrueTypeFont: 无法解析 TTF 数据");
        return false;
    }

    font_size_ = config.font_size;
    atlas_width_ = config.atlas_width;
    atlas_height_ = config.atlas_height;

    float scale = stbtt_ScaleForPixelHeight(&font_info, font_size_);

    int ascent_raw, descent_raw, line_gap_raw;
    stbtt_GetFontVMetrics(&font_info, &ascent_raw, &descent_raw, &line_gap_raw);
    ascent_ = ascent_raw * scale;
    descent_ = descent_raw * scale;
    line_height_ = (ascent_raw - descent_raw + line_gap_raw) * scale;

    // 收集所有需要的码点
    std::vector<int> codepoints;
    for (int i = 0; i < config.num_codepoints; ++i) {
        codepoints.push_back(config.first_codepoint + i);
    }
    for (int cp : config.extra_codepoints) {
        codepoints.push_back(cp);
    }

    // 使用 stb_rect_pack 排列字形
    std::vector<stbrp_rect> rects(codepoints.size());

    struct GlyphBitmap {
        int codepoint;
        unsigned char* bitmap;
        int w, h;
        int xoff, yoff;
    };
    std::vector<GlyphBitmap> glyph_bitmaps(codepoints.size());

    for (size_t i = 0; i < codepoints.size(); ++i) {
        int cp = codepoints[i];
        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char* bmp = stbtt_GetCodepointBitmap(&font_info, 0, scale, cp, &w, &h, &xoff, &yoff);

        glyph_bitmaps[i] = {cp, bmp, w, h, xoff, yoff};

        rects[i].id = static_cast<int>(i);
        rects[i].w = static_cast<stbrp_coord>(w + config.padding * 2);
        rects[i].h = static_cast<stbrp_coord>(h + config.padding * 2);
    }

    stbrp_context pack_ctx;
    std::vector<stbrp_node> pack_nodes(atlas_width_);
    stbrp_init_target(&pack_ctx, atlas_width_, atlas_height_, pack_nodes.data(), static_cast<int>(pack_nodes.size()));
    stbrp_pack_rects(&pack_ctx, rects.data(), static_cast<int>(rects.size()));

    // 创建图集位图
    atlas_bitmap_.resize(atlas_width_ * atlas_height_, 0);

    float inv_w = 1.0f / static_cast<float>(atlas_width_);
    float inv_h = 1.0f / static_cast<float>(atlas_height_);

    for (size_t i = 0; i < codepoints.size(); ++i) {
        auto& rect = rects[i];
        auto& gb = glyph_bitmaps[i];

        if (!rect.was_packed) {
            if (gb.bitmap) stbtt_FreeBitmap(gb.bitmap, nullptr);
            continue;
        }

        int dst_x = rect.x + config.padding;
        int dst_y = rect.y + config.padding;

        // 复制字形位图到图集
        if (gb.bitmap) {
            for (int row = 0; row < gb.h; ++row) {
                std::memcpy(
                    atlas_bitmap_.data() + (dst_y + row) * atlas_width_ + dst_x,
                    gb.bitmap + row * gb.w,
                    gb.w
                );
            }
            stbtt_FreeBitmap(gb.bitmap, nullptr);
        }

        // 计算 advance
        int advance_raw, lsb;
        stbtt_GetCodepointHMetrics(&font_info, gb.codepoint, &advance_raw, &lsb);

        GlyphMetrics gm;
        gm.codepoint = gb.codepoint;
        gm.advance_x = advance_raw * scale;
        gm.bearing_x = static_cast<float>(gb.xoff);
        gm.bearing_y = static_cast<float>(gb.yoff);
        gm.width = static_cast<float>(gb.w);
        gm.height = static_cast<float>(gb.h);
        gm.uv = glm::vec4(
            dst_x * inv_w,
            dst_y * inv_h,
            (dst_x + gb.w) * inv_w,
            (dst_y + gb.h) * inv_h
        );

        glyphs_[gb.codepoint] = gm;
    }

    DEBUG_LOG_INFO("TrueTypeFont: 加载成功, 字号={}, 图集={}x{}, 字形数={}",
                   font_size_, atlas_width_, atlas_height_, glyphs_.size());
    return true;
}

const GlyphMetrics* TrueTypeFont::GetGlyph(int codepoint) const {
    auto it = glyphs_.find(codepoint);
    return it != glyphs_.end() ? &it->second : nullptr;
}

float TrueTypeFont::MeasureTextWidth(const std::string& text) const {
    float width = 0.0f;
    for (size_t i = 0; i < text.size(); ) {
        // 简单 ASCII 处理；多字节 UTF-8 取首字节
        int cp = static_cast<unsigned char>(text[i]);
        if ((cp & 0x80) == 0) {
            ++i;
        } else if ((cp & 0xE0) == 0xC0 && i + 1 < text.size()) {
            cp = ((cp & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((cp & 0xF0) == 0xE0 && i + 2 < text.size()) {
            cp = ((cp & 0x0F) << 12)
               | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6)
               | (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((cp & 0xF8) == 0xF0 && i + 3 < text.size()) {
            cp = ((cp & 0x07) << 18)
               | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12)
               | ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6)
               | (static_cast<unsigned char>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            ++i;
            continue;
        }

        if (auto* glyph = GetGlyph(cp)) {
            width += glyph->advance_x;
        }
    }
    return width;
}

std::vector<TrueTypeFont::CharLayout> TrueTypeFont::LayoutText(const std::string& text) const {
    std::vector<CharLayout> result;
    float cursor_x = 0.0f;
    for (size_t i = 0; i < text.size(); ) {
        int cp = static_cast<unsigned char>(text[i]);
        if ((cp & 0x80) == 0) {
            ++i;
        } else if ((cp & 0xE0) == 0xC0 && i + 1 < text.size()) {
            cp = ((cp & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((cp & 0xF0) == 0xE0 && i + 2 < text.size()) {
            cp = ((cp & 0x0F) << 12)
               | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6)
               | (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((cp & 0xF8) == 0xF0 && i + 3 < text.size()) {
            cp = ((cp & 0x07) << 18)
               | ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12)
               | ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6)
               | (static_cast<unsigned char>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            ++i;
            continue;
        }

        CharLayout cl;
        cl.codepoint = cp;
        cl.x = cursor_x;
        cl.y = 0.0f;

        if (auto* glyph = GetGlyph(cp)) {
            cursor_x += glyph->advance_x;
        }

        result.push_back(cl);
    }
    return result;
}

} // namespace render
} // namespace dse
