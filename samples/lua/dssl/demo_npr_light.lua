-- demo_npr_light.lua
-- NPR 自定义光照模型演示：展示 DSSL light() 函数的三种预设
-- hatching (交叉线影) / gradient_ramp (渐变色带) / minnaert (月球散射)

local ecs = require("dse.ecs")

-- ============================================================
-- 1) Hatching — 素描/版画风格
-- ============================================================
local hatching_entity = ecs.create_entity()
ecs.add_mesh_renderer(hatching_entity, "data/mesh/sphere.mesh")

local mat_hatching = dssl.load_material("samples/lua/dssl/hatching.dssl")
local inst_hatching = dssl.create_instance(mat_hatching)
dssl.set_color(inst_hatching, "ink_color",   0.1, 0.08, 0.05, 1.0)
dssl.set_color(inst_hatching, "paper_color", 0.95, 0.92, 0.85, 1.0)
dssl.set_float(inst_hatching, "hatch_density",   30.0)
dssl.set_float(inst_hatching, "hatch_thickness",  0.4)
dssl.apply_material(inst_hatching, hatching_entity)
ecs.set_transform_position(hatching_entity, -300, 0, 0)

-- ============================================================
-- 2) Gradient Ramp — 渐变色带光照
-- ============================================================
local ramp_entity = ecs.create_entity()
ecs.add_mesh_renderer(ramp_entity, "data/mesh/sphere.mesh")

local mat_ramp = dssl.load_material("samples/lua/dssl/gradient_ramp.dssl")
local inst_ramp = dssl.create_instance(mat_ramp)
dssl.set_color(inst_ramp, "albedo_color", 1.0, 0.85, 0.7, 1.0)
dssl.set_color(inst_ramp, "warm_color",   1.0, 0.9,  0.7, 1.0)
dssl.set_color(inst_ramp, "cool_color",   0.3, 0.4,  0.6, 1.0)
dssl.set_float(inst_ramp, "ramp_smoothness", 0.1)
dssl.set_float(inst_ramp, "ramp_bands",      4.0)
dssl.apply_material(inst_ramp, ramp_entity)
ecs.set_transform_position(ramp_entity, 0, 0, 0)

-- ============================================================
-- 3) Minnaert — 月球表面散射
-- ============================================================
local minnaert_entity = ecs.create_entity()
ecs.add_mesh_renderer(minnaert_entity, "data/mesh/sphere.mesh")

local mat_minnaert = dssl.load_material("samples/lua/dssl/minnaert.dssl")
local inst_minnaert = dssl.create_instance(mat_minnaert)
dssl.set_color(inst_minnaert, "albedo_color", 0.75, 0.73, 0.7, 1.0)
dssl.set_float(inst_minnaert, "darkness", 1.5)
dssl.apply_material(inst_minnaert, minnaert_entity)
ecs.set_transform_position(minnaert_entity, 300, 0, 0)

print("[demo_npr_light] NPR light() models loaded:")
print("  left:   hatching  (cross-hatch sketch style)")
print("  center: gradient_ramp (warm/cool color bands)")
print("  right:  minnaert  (lunar limb darkening)")
