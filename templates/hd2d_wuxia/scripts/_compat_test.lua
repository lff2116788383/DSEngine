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
    print("[compat] OK: ui.set_visible / audio.play_sfx / play_bgm 已支持 bool+number")
end
function Update(dt) end