#include "sprite.h"
#include "texture_2d.h"
#include <rttr/registration>

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<Sprite>("Sprite")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("texture", &Sprite::texture, &Sprite::set_texture)
        .property("texture_path", &Sprite::texture_path, &Sprite::set_texture_path)
        .property("ppu", &Sprite::ppu, &Sprite::set_ppu);
}

Sprite::Sprite() {}

Sprite::~Sprite() {}

Sprite* Sprite::Create(const std::string& texture_path) {
    auto texture = Texture2D::LoadFromFile(texture_path);
    if (!texture) return nullptr;

    Sprite* sprite = new Sprite();
    sprite->set_texture(texture);
    sprite->set_texture_path(texture_path);
    sprite->set_rect(0, 0, (float)texture->width(), (float)texture->height());
    return sprite;
}

Sprite* Sprite::CreateFromAtlas(const std::string& texture_path, float x, float y, float width, float height) {
    return CreateFromAtlas(texture_path, x, y, width, height, 0.5f, 0.5f, 100.0f);
}

Sprite* Sprite::CreateFromAtlas(const std::string& texture_path, float x, float y, float width, float height, float pivot_x, float pivot_y, float ppu) {
    auto texture = Texture2D::LoadFromFile(texture_path);
    if (!texture) return nullptr;

    Sprite* sprite = new Sprite();
    sprite->set_texture(texture);
    sprite->set_texture_path(texture_path);
    sprite->set_rect(x, y, width, height);
    sprite->set_pivot(pivot_x, pivot_y);
    sprite->set_ppu(ppu);
    return sprite;
}
