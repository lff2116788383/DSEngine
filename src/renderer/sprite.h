#ifndef UNTITLED_SPRITE_H
#define UNTITLED_SPRITE_H

#include <rttr/registration>
#include <string>
#include "texture_2d.h"

class Sprite {
public:
    Sprite();
    ~Sprite();

    void set_texture(Texture2D* texture) { texture_ = texture; }
    Texture2D* texture() const { return texture_; }

    // Sprite rect in texture coordinates (pixels)
    void set_rect(float x, float y, float width, float height) { rect_ = {x, y, width, height}; }
    struct Rect { float x, y, width, height; };
    Rect rect() const { return rect_; }

    // Pivot point (0-1), default (0.5, 0.5) center
    void set_pivot(float x, float y) { pivot_ = {x, y}; }
    struct Vector2 { float x, y; };
    Vector2 pivot() const { return pivot_; }

    // Pixels per unit, default 100
    void set_ppu(float ppu) { ppu_ = ppu; }
    float ppu() const { return ppu_; }

    void set_texture_path(const std::string& path) { texture_path_ = path; }
    std::string texture_path() const { return texture_path_; }

    static Sprite* Create(const std::string& texture_path);
    static Sprite* CreateFromAtlas(const std::string& texture_path, float x, float y, float width, float height);
    static Sprite* CreateFromAtlas(const std::string& texture_path, float x, float y, float width, float height, float pivot_x, float pivot_y, float ppu);

private:
    Texture2D* texture_ = nullptr;
    std::string texture_path_;
    Rect rect_ = {0, 0, 0, 0};
    Vector2 pivot_ = {0.5f, 0.5f};
    float ppu_ = 100.0f;

RTTR_ENABLE();
};

#endif //UNTITLED_SPRITE_H
