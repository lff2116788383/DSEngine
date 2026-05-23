-- ============================================================
-- 全局配置
-- 3D demo 的 per-demo 配置已迁移至各 demo 的 _meta.config 表中，
-- 本文件仅保留全局设置和 2D demo 配置。
-- ============================================================
Config={}
Config.title="DSEngine"
Config.data_path="data/"
Config.game_entry="3d_fracture"

-- 2D demo 配置
Config.phase1_2d_showcase={
    camera_ortho_size=7.5
}
Config.phase1_2d_physics_showcase={
    camera_ortho_size=9.0
}

-- 3D 基础 fallback（无 _meta 的旧 demo 使用）
Config.basic_3d={
    camera_distance=6.0
}
return Config
