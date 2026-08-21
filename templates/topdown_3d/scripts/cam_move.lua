-- ============================================================================
-- cam_move.lua — 相机系统 (Cam_Move.cs 完整移植)
-- C# 原值: distancetarget=(0,1.3,-1.04) / fov=30 / limit_x=2.1 / movespeed=10
-- 为适配 DSEngine 相机距离(高 18 / 后 8 / FOV 55)按比例放大, 逻辑与 C# 一致
-- ============================================================================
local State = require("state")
local G = State.G
local Player = State.Player
local clamp = State.clamp
local lerp = State.lerp
local dist2d = State.dist2d

local CAM = {
  dx = 1,                    -- C# dx (Hitcam 方向翻转)
  zoom = false,              -- 缩放进行中 (C# zoom)
  z_speed = 0,               -- 缩放速度 (C# z_speed, -1=瞬时)
  z_time = 0,                -- 缩放持续时间 (C# z_time)
  zoomdelay = 0,             -- 缩放累计时间 (C# zoomdelay)
  fov = 35,                  -- 当前目标 FOV (原版 fov=30, DSE 放宽至 35)
  originfov = 35,            -- 原始 FOV (C# originfov)
  target_e = nil,            -- 跟随目标实体 (nil=玩家, C# target)
  distancetarget = {0, 18, 8}, -- 相机相对目标偏移 (C# distancetarget=(0,1.3,-1.04) 按比例放大)
  boundfactor = 1,           -- 边界缩放系数 (C# boundfactor)
  resetcam_delay = 0,        -- 复位延迟 (C# resetcam_delay)
  resetstart = false,        -- 延迟复位进行中 (C# resetstart)
  fovchange = false,         -- FOV 变化激活 (C# fovchange)
  fovbk_delay = 0,           -- FOV 回退延迟 (C# fovbk_delay)
  limit_x = 25,              -- C# limit_x=2.1 (引擎尺度)
  limit_y_b = -20,           -- C# limit_y_b=-3.3
  limit_y_f = 25,            -- C# limit_y_f=0.85
  topviewon = 0,             -- 0=正常 1=俯视过渡 2=俯视恢复 (C# topviewon)
  topviewdelay = 3,          -- 俯视时长 (C# topviewdelay)
  movespeed = 10,            -- 跟随速度 (C# movespeed)
  hit_shake1 = {0.8, 0, 0.4},-- C# hit_shake1=(0.06,0,0.03) 按相机距离放大
  hit_shake2 = {0, 0.4, 0.3},-- C# hit_shake2=(0,0.03,0.02)
  pitch = -50,               -- 相机俯仰角 (原版 listener 四元数反推约 50°)
}

-- 相机复位 (C# ResetCam)
local function CameraReset()
  CAM.target_e = nil
  CAM.fov = CAM.originfov
  CAM.fovchange = true
  CAM.resetstart = false
  CAM.zoom = false
  CAM.zoomdelay = 0
end

-- 相机聚焦目标 (C# LookTarget)
local function CameraLookTarget(e, fov, delay)
  CAM.target_e = e
  if delay > 0 then
    CAM.resetstart = true
    CAM.resetcam_delay = delay
  end
  if fov ~= 0 then
    CameraZoomIn(-1, fov, delay)
  end
end

-- 相机缩放 (C# ZoomIn)
local function CameraZoomIn(zoomspeed, fov, delay)
  CAM.zoom = true
  CAM.z_speed = zoomspeed
  CAM.fov = fov
  CAM.z_time = delay
  CAM.fovchange = true
end

-- 俯视切换 (C# Topview)
local function CameraTopview()
  CAM.distancetarget = {0, 18, 0.2}
  CAM.topviewon = 1
  CAM.topviewdelay = 3
  CAM.movespeed = 5
end

-- 受击震动 (C# Hitcam): 位置偏移, 随跟随 Lerp 自然衰减
local function CameraHitcam()
  if not G.cam then return end
  CAM.dx = -CAM.dx
  local cx, cy, cz = dse.ecs.get_transform_position(G.cam)
  if cx then
    dse.ecs.set_transform_position(G.cam, cx + CAM.hit_shake1[1] * CAM.dx, cy, cz + CAM.hit_shake1[3] * CAM.dx)
  end
end

-- 受击震动 (C# Hitcam2): 按系数偏移
local function CameraHitcam2(factor)
  if not G.cam then return end
  local cx, cy, cz = dse.ecs.get_transform_position(G.cam)
  if cx then
    dse.ecs.set_transform_position(G.cam,
      cx + CAM.hit_shake2[1] * factor, cy + CAM.hit_shake2[2] * factor, cz + CAM.hit_shake2[3] * factor)
  end
end

-- 相机逐帧更新 (C# Cam_Move.Update 完整移植)
local function UpdateCamera(dt)
  if not G.cam then return end

  -- C# Update: resetstart 延迟复位
  if CAM.resetstart then
    if CAM.resetcam_delay > 0 then
      CAM.resetcam_delay = CAM.resetcam_delay - dt
    else
      CAM.resetcam_delay = 0
      CAM.resetstart = false
      CameraReset()
    end
  end

  -- 目标位置 (C# chaposition = target.position + distancetarget)
  local tx, ty, tz
  if CAM.target_e then
    tx, ty, tz = dse.ecs.get_transform_position(CAM.target_e)
    if not tx then tx, ty, tz = Player.x, Player.y, Player.z end
  else
    tx, ty, tz = Player.x, Player.y, Player.z
  end
  local chapos_x = tx + CAM.distancetarget[1]
  local chapos_y = ty + CAM.distancetarget[2]
  local chapos_z = tz + CAM.distancetarget[3]

  -- 平滑跟随 (C# Lerp * movespeed)
  local cx, cy, cz = dse.ecs.get_transform_position(G.cam)
  local nx, ny, nz
  if cx then
    local f = clamp(dt * CAM.movespeed, 0, 1)
    nx = lerp(cx, chapos_x, f)
    ny = lerp(cy, chapos_y, f)
    nz = lerp(cz, chapos_z, f)
  else
    nx, ny, nz = chapos_x, chapos_y, chapos_z
  end

  -- limitpos 边界 (C# limit clamp; topviewon==1 时 z 不限制)
  local lx, lz = nx, nz
  if CAM.topviewon ~= 1 then
    lz = clamp(lz, CAM.limit_y_b * CAM.boundfactor, CAM.limit_y_f * CAM.boundfactor)
  end
  lx = clamp(lx, -CAM.limit_x * CAM.boundfactor, CAM.limit_x * CAM.boundfactor)
  if CAM.topviewon == 2 then
    nx = nx + (lx - nx) * clamp(dt * 3, 0, 1)
    nz = nz + (lz - nz) * clamp(dt * 3, 0, 1)
  else
    nx, nz = lx, lz
  end

  -- FOV 变化 (C# fovchange)
  if CAM.fovchange then
    local cur_fov = dse.ecs.get_camera3d_fov(G.cam) or CAM.originfov
    CAM.boundfactor = 0.8 + 0.2 * CAM.originfov / (cur_fov > 0.001 and cur_fov or CAM.originfov)
    if CAM.zoom then
      if CAM.z_speed == -1 then
        dse.ecs.set_camera3d_fov(G.cam, CAM.fov)
      else
        dse.ecs.set_camera3d_fov(G.cam, lerp(cur_fov, CAM.fov, clamp(dt * CAM.z_speed, 0, 1)))
      end
      if CAM.zoomdelay < CAM.z_time then
        CAM.zoomdelay = CAM.zoomdelay + dt
      else
        CAM.zoom = false
        CAM.zoomdelay = 0
        CAM.fovbk_delay = 0
      end
    elseif CAM.fovbk_delay < 2 then
      CAM.fovbk_delay = CAM.fovbk_delay + dt
      dse.ecs.set_camera3d_fov(G.cam, lerp(cur_fov, CAM.originfov, clamp(dt * 3, 0, 1)))
    else
      CAM.fovbk_delay = 0
      CAM.fovchange = false
      dse.ecs.set_camera3d_fov(G.cam, CAM.originfov)
    end
  end

  -- 俯视模式 (C# topviewon)
  if CAM.topviewon > 0 then
    CAM.topviewdelay = CAM.topviewdelay - dt
    if CAM.topviewon == 1 then
      CAM.pitch = lerp(CAM.pitch, -70, clamp(dt * 5, 0, 1))
      local cf = dse.ecs.get_camera3d_fov(G.cam) or CAM.originfov
      dse.ecs.set_camera3d_fov(G.cam, lerp(cf, 25, clamp(dt * 3, 0, 1)))
      if CAM.topviewdelay < 1.5 then
        CAM.distancetarget = {0, 18, 8}
        CAM.topviewon = 2
        CAM.movespeed = 5
      end
    elseif CAM.topviewon == 2 then
      CAM.pitch = lerp(CAM.pitch, -50, clamp(dt * 5, 0, 1))
      local cf = dse.ecs.get_camera3d_fov(G.cam) or CAM.originfov
      dse.ecs.set_camera3d_fov(G.cam, lerp(cf, CAM.originfov, clamp(dt * 3, 0, 1)))
      if CAM.topviewdelay < 0 then
        CAM.topviewon = 0
        CAM.pitch = -50
        CAM.movespeed = 10
      end
    end
  end

  dse.ecs.set_transform_position(G.cam, nx, ny, nz)
  dse.ecs.set_transform_rotation(G.cam, CAM.pitch, 0, 0)
end

local M = {
  CAM = CAM,
  CameraReset = CameraReset,
  CameraLookTarget = CameraLookTarget,
  CameraZoomIn = CameraZoomIn,
  CameraTopview = CameraTopview,
  CameraHitcam = CameraHitcam,
  CameraHitcam2 = CameraHitcam2,
  UpdateCamera = UpdateCamera,
}

return M
