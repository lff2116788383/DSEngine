-- demo_toon.lua
-- Toon / Cel Shading 演示：展示 toon_cel.dssl 材质的各参数效果
-- 用法：在 Lua host 中 require 或直接运行

local world = dse.get_world()
local scene = dse.get_scene()

-- 创建主角网格（使用 toon_cel 材质）
local hero = world:create_entity()
world:add_mesh_renderer(hero, "data/mesh/character.mesh")
world:set_material(hero, "toon_cel")

-- 基础参数：蓝紫色调暗区，自然白色亮区
dse.material_set_vec4(hero, "shadow_color",  0.12, 0.08, 0.22, 1.0)
dse.material_set_float(hero, "shadow_threshold", 0.38)
dse.material_set_float(hero, "shadow_softness",  0.04)

-- 离散高光：中等大小、明显强度
dse.material_set_float(hero, "specular_size",     0.65)
dse.material_set_float(hero, "specular_strength",  1.2)

-- 边缘光：暖色调，中等强度
dse.material_set_vec4(hero, "rim_color",   1.0, 0.85, 0.5, 1.0)
dse.material_set_float(hero, "rim_strength", 0.5)

-- 第二个实体：卡通风格建筑（调暗阴影、关闭高光、柔和边缘）
local building = world:create_entity()
world:add_mesh_renderer(building, "data/mesh/building.mesh")
world:set_material(building, "toon_cel")

dse.material_set_vec4(building, "shadow_color",   0.1, 0.1, 0.12, 1.0)
dse.material_set_float(building, "shadow_threshold", 0.4)
dse.material_set_float(building, "shadow_softness",  0.02)
dse.material_set_float(building, "specular_size",    0.9)   -- 高阈值 = 几乎无高光
dse.material_set_float(building, "specular_strength", 0.3)
dse.material_set_float(building, "rim_strength",      0.15)

print("[demo_toon] Toon shading demo loaded.")
print("  hero:     purple shadow / warm rim / discrete specular")
print("  building: dark shadow / near-zero specular / minimal rim")
