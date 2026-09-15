-- 绑定兼容层功能验证：以前这些调用会报 number expected / got boolean
function Awake()
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
    dse.ui.add_renderer(e, 0, 1, 1, 1, 1, 100000, 32, 32)
    dse.ui.set_visible(e, true)     -- bool
    dse.ui.set_visible(e, false)
    dse.ui.set_visible(e, 1)        -- number
    dse.audio.play_sfx("templates/hd2d_wuxia/assets/audio/sfx_ui.wav", 0.3, false)  -- bool loop
    dse.audio.play_bgm("templates/hd2d_wuxia/assets/audio/bgm_title.wav", 0.2, true)
    -- P2：精灵 UV 子矩形 / 排序层 / 变体 / 混合模式 + 采样器可控加载
    local tex = dse.assets.load_texture_ex("templates/hd2d_wuxia/assets/fx/slash_0.png", "nearest", "clamp")
    local sp = dse.ecs.create_entity()
    dse.ecs.add_transform(sp, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_sprite(sp, 1, 1, 1, 1, 0, tex)
    dse.ecs.set_sprite_uv_rect(sp, 0.0, 0.0, 1.0, 1.0)
    dse.ecs.set_sprite_sorting_layer(sp, 3)
    dse.ecs.set_sprite_shader_variant(sp, "SPRITE_UNLIT")
    dse.ecs.set_sprite_blend_mode(sp, "add")
    dse.ecs.set_sprite_blend_mode(sp, 0)
    print("[compat] OK: P1 bool/number + P2 精灵 API(uv_rect/sorting_layer/variant/blend/load_texture_ex)")
end
function Update(dt) end