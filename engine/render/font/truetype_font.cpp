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

namespace {
/// 从 UTF-8 字符串解码下一个码点，i 为当前偏移，解码后推进 i
int DecodeUtf8(const std::string& text, size_t& i) {
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
        return -1;
    }
    return cp;
}

/// CJK 字符可在字符前/后断行
bool IsCJKCodepoint(int cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)   // CJK Unified
        || (cp >= 0x3400 && cp <= 0x4DBF)   // CJK Extension A
        || (cp >= 0x3000 && cp <= 0x303F)   // CJK Punctuation
        || (cp >= 0xFF00 && cp <= 0xFFEF)   // Fullwidth Forms
        || (cp >= 0x3040 && cp <= 0x309F)   // Hiragana
        || (cp >= 0x30A0 && cp <= 0x30FF);  // Katakana
}
} // anonymous namespace

bool TrueTypeFont::LoadFromFile(const std::string& ttf_path, const FontAtlasConfig& config) {
    std::ifstream file(ttf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("TrueTypeFont: 无法打开文件 {}", ttf_path);
        return false;
    }
    auto size = file.tellg();
    if (size <= 0) {  // tellg 失败 (-1) 时 static_cast<size_t> 会请求约 SIZE_MAX 字节触发 bad_alloc
        DEBUG_LOG_ERROR("TrueTypeFont: 文件大小无效 {}", ttf_path);
        return false;
    }
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
    // 图集尺寸须为正且乘积不溢出 int，否则下方 resize(atlas_width_*atlas_height_) 会因有符号溢出 UB / 超大分配。
    if (atlas_width_ <= 0 || atlas_height_ <= 0 ||
        static_cast<int64_t>(atlas_width_) * atlas_height_ > (64 * 1024 * 1024)) {
        DEBUG_LOG_ERROR("TrueTypeFont: 非法图集尺寸 {}x{}", atlas_width_, atlas_height_);
        return false;
    }

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
        int cp = DecodeUtf8(text, i);
        if (cp < 0) continue;

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

TrueTypeFont::LayoutResult TrueTypeFont::LayoutTextEx(
    const std::string& text, const LayoutParams& params) const {

    LayoutResult result;
    if (text.empty() || !IsValid()) return result;

    const float line_h = line_height_ + params.line_spacing_extra;
    const float max_w = params.max_width;
    const bool wrap = (max_w > 0.0f) && (params.overflow == 0);
    const bool truncate = (max_w > 0.0f) && (params.overflow == 1);
    const bool ellipsis = (max_w > 0.0f) && (params.overflow == 2);

    // Phase 1: 解码所有码点
    struct CodepointInfo {
        int codepoint;
        float advance;
    };
    std::vector<CodepointInfo> codepoints;
    for (size_t i = 0; i < text.size(); ) {
        int cp = DecodeUtf8(text, i);
        if (cp < 0) continue;
        float adv = 0.0f;
        if (auto* gm = GetGlyph(cp)) adv = gm->advance_x;
        codepoints.push_back({cp, adv});
    }

    // 省略号宽度
    float ellipsis_w = 0.0f;
    if (ellipsis) {
        if (auto* dot = GetGlyph('.')) {
            ellipsis_w = dot->advance_x * 3.0f;
        }
    }

    // Phase 2: 按行分割
    struct LineInfo {
        size_t start;       // codepoints 索引
        size_t count;
        float width;
        bool truncated;
    };
    std::vector<LineInfo> lines;
    float cursor_x = 0.0f;
    size_t line_start = 0;
    size_t last_break_pos = 0;      // 上一个可断行点
    float width_at_break = 0.0f;

    for (size_t idx = 0; idx < codepoints.size(); ++idx) {
        int cp = codepoints[idx].codepoint;
        float adv = codepoints[idx].advance;

        // 换行符
        if (cp == '\n') {
            lines.push_back({line_start, idx - line_start, cursor_x, false});
            line_start = idx + 1;
            cursor_x = 0.0f;
            last_break_pos = line_start;
            width_at_break = 0.0f;
            continue;
        }

        // 记录可断行点（空格后/CJK 字符前）
        if (cp == ' ' || cp == '\t') {
            last_break_pos = idx + 1;
            width_at_break = cursor_x + adv;
        } else if (IsCJKCodepoint(cp)) {
            last_break_pos = idx;
            width_at_break = cursor_x;
        }

        float new_x = cursor_x + adv;

        // 需要换行?
        if (max_w > 0.0f && new_x > max_w && idx > line_start) {
            if (wrap) {
                // 行数限制
                if (params.max_lines > 0 && static_cast<int>(lines.size()) + 1 >= params.max_lines) {
                    // 最后一行截断到 max_w
                    lines.push_back({line_start, idx - line_start, cursor_x, true});
                    line_start = codepoints.size(); // 停止
                    break;
                }

                // 回退到断行点
                if (last_break_pos > line_start) {
                    lines.push_back({line_start, last_break_pos - line_start, width_at_break, false});
                    line_start = last_break_pos;
                    // 跳过断行点处的空格
                    while (line_start < codepoints.size() && codepoints[line_start].codepoint == ' ') {
                        ++line_start;
                    }
                    idx = line_start > 0 ? line_start - 1 : 0; // for 循环 ++idx
                } else {
                    // 无断行点，强制在当前字符前断行
                    lines.push_back({line_start, idx - line_start, cursor_x, false});
                    line_start = idx;
                    idx = idx > 0 ? idx - 1 : 0;
                }
                cursor_x = 0.0f;
                last_break_pos = line_start;
                width_at_break = 0.0f;
                continue;
            } else if (truncate) {
                lines.push_back({line_start, idx - line_start, cursor_x, true});
                line_start = codepoints.size();
                break;
            } else if (ellipsis) {
                // 回退直到能放下省略号
                float line_w = cursor_x;
                size_t end = idx;
                while (end > line_start && line_w + ellipsis_w > max_w) {
                    --end;
                    line_w -= codepoints[end].advance;
                }
                lines.push_back({line_start, end - line_start, line_w, true});
                line_start = codepoints.size();
                break;
            }
        }

        cursor_x = new_x;
    }

    // 最后一行
    if (line_start < codepoints.size()) {
        lines.push_back({line_start, codepoints.size() - line_start, cursor_x, false});
    }
    // 空文本至少 1 行
    if (lines.empty()) {
        lines.push_back({0, 0, 0.0f, false});
    }

    // Phase 3: 生成 CharLayout 并应用对齐
    result.line_count = static_cast<int>(lines.size());
    float max_line_w = 0.0f;

    for (int line_idx = 0; line_idx < static_cast<int>(lines.size()); ++line_idx) {
        auto& line = lines[line_idx];
        max_line_w = std::fmax(max_line_w, line.width);

        // 对齐偏移
        float align_offset = 0.0f;
        if (max_w > 0.0f) {
            float remaining = max_w - line.width;
            if (params.align == 1) align_offset = remaining * 0.5f;       // 居中
            else if (params.align == 2) align_offset = remaining;          // 右对齐
        }

        float cx = align_offset;
        float cy = static_cast<float>(line_idx) * line_h;

        for (size_t j = 0; j < line.count; ++j) {
            auto& cpi = codepoints[line.start + j];
            CharLayout cl;
            cl.codepoint = cpi.codepoint;
            cl.x = cx;
            cl.y = cy;
            cx += cpi.advance;
            result.chars.push_back(cl);
        }

        // 省略号: 如果该行被截断且为省略号模式
        if (line.truncated && ellipsis) {
            for (int d = 0; d < 3; ++d) {
                CharLayout dot;
                dot.codepoint = '.';
                dot.x = cx;
                dot.y = cy;
                if (auto* gm = GetGlyph('.')) cx += gm->advance_x;
                result.chars.push_back(dot);
            }
        }
    }

    result.total_width = max_w > 0.0f ? max_w : max_line_w;
    result.total_height = static_cast<float>(result.line_count) * line_h;
    return result;
}

void TrueTypeFont::SetMetrics(float font_size, float ascent, float descent, float line_height,
                              int atlas_w, int atlas_h) {
    font_size_ = font_size;
    ascent_ = ascent;
    descent_ = descent;
    line_height_ = line_height;
    atlas_width_ = atlas_w;
    atlas_height_ = atlas_h;
}

void TrueTypeFont::SetGlyph(int codepoint, const GlyphMetrics& gm) {
    glyphs_[codepoint] = gm;
}

void TrueTypeFont::SetAtlasBitmap(std::vector<uint8_t>&& bitmap, int width, int height) {
    atlas_bitmap_ = std::move(bitmap);
    atlas_width_ = width;
    atlas_height_ = height;
}

} // namespace render
} // namespace dse
