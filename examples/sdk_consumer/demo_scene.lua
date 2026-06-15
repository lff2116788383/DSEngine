-- DSEngine SDK Consumer —— 完整 Demo 场景（3D + Lua + Jolt 物理）
--
-- 由 C++ 宿主 game_main.cpp 经 dse::runtime::RunEngine(BusinessMode::Lua) 加载，
-- 演示一个 SDK 消费者如何用「公共脚本 API」搭建一个带物理模拟的 3D 场景：
--   * 透视相机 + 平行光
--   * 静态地面刚体
--   * 一座会在重力下塌落的动态立方体塔（Jolt 刚体 + 盒碰撞体）
-- 仅使用 docs 中已文档化的 dse.ecs API，确保跨版本稳定。

local cube_v = { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                 -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
local cube_i = { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }

local state = { t = 0.0, frame = 0, tracked = nil, start_y = 0.0, reported = false }

-- 生成一个带网格 + 盒碰撞体 + 刚体的立方体。
-- dynamic=true → 动态刚体(type=2)；false → 静态(type=0)。
local function spawn_box(x, y, z, sx, sy, sz, r, g, b, dynamic, mass)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
  dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
  dse.ecs.set_mesh_shader_variant(e, dynamic and "MESH_LIT" or "MESH_UNLIT")
  dse.ecs.add_box_collider_3d(e, sx, sy, sz)
  dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass or 1.0)
  return e
end

function Awake()
  -- 透视相机：略微俯视塔体
  local cam = dse.ecs.create_entity()
  dse.ecs.add_transform(cam, 0, 7, 18, 1, 1, 1)
  dse.ecs.set_transform_rotation(cam, -16, 0, 0)
  dse.ecs.add_camera_3d(cam, 60, 200)

  -- 平行光（方向 + 颜色 + 强度 + 环境光）
  local light = dse.ecs.create_entity()
  dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.5, 1.0, 0.97, 0.9, 1.3, 0.35, 0.35)

  -- 静态地面
  spawn_box(0, -0.5, 0, 24, 1, 24, 0.24, 0.27, 0.30, false, 0.0)

  -- 动态立方体塔：4 层、每层略微错位，重力下会失稳塌落
  local colors = {
    {0.90, 0.30, 0.25}, {0.30, 0.65, 0.95},
    {0.95, 0.80, 0.25}, {0.45, 0.85, 0.45},
  }
  for layer = 0, 3 do
    local n = 4 - layer
    local off = (layer % 2 == 0) and 0.0 or 0.55
    for i = 0, n - 1 do
      local x = (i - (n - 1) * 0.5) * 1.15 + off
      local y = 0.6 + layer * 1.05
      local c = colors[layer + 1]
      local e = spawn_box(x, y, 0, 0.5, 0.5, 0.5, c[1], c[2], c[3], true, 1.0)
      if layer == 3 and i == 0 then
        state.tracked = e
        state.start_y = y
      end
    end
  end

  print("[sdk_demo] scene ready: camera + dir light + ground + 13-box dynamic tower")
end

function Update(dt)
  state.t = state.t + (dt or 0.0)
  state.frame = state.frame + 1

  -- 每 ~30 帧打印一次被跟踪顶层方块的高度，证明物理在驱动场景下落
  if state.tracked and state.frame % 30 == 0 and dse.ecs.get_transform_position then
    local _, y, _ = dse.ecs.get_transform_position(state.tracked)
    if y then
      print(string.format("[sdk_demo] t=%.2fs frame=%d top_box_y=%.3f (start=%.3f, dropped=%.3f)",
            state.t, state.frame, y, state.start_y, state.start_y - y))
      if not state.reported and (state.start_y - y) > 0.05 then
        print("[sdk_demo] physics OK: gravity is pulling the tower down")
        state.reported = true
      end
    end
  end
end
