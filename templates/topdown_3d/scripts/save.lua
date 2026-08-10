-- ============================================================================
-- save.lua — 存档系统
-- 对应 C#: Crypto / DataSave / ConvertSaveData / TimeControl
-- 引擎无 PlayerPrefs/存档 API, 采用轻量自实现: 本地文件 + 异或混淆 + hex 编码
-- (非 1:1 复刻 AES, 满足桌面单机持久化即可)
-- 导出: save_all() / load_all() / save_path()
-- ============================================================================

local State = require("state")
local G, Player = State.G, State.Player

local SAVE_FILE = "topdown_3d_save.dat"
local SAVE_KEY = "anjueolh"            -- 与原版 Crypto.KEY 一致
local SAVE_VERSION = 1

-- 存档路径: 优先 DSE_DATA_ROOT (模板目录), 否则进程 cwd (bin/)
local function save_path()
  local root = os.getenv("DSE_DATA_ROOT")
  if root and root ~= "" then
    return root .. "/" .. SAVE_FILE
  end
  return SAVE_FILE
end

-- hex 编解码 (避免额外依赖 Base64 实现)
local function to_hex(s)
  return (s:gsub(".", function(c) return string.format("%02x", c:byte()) end))
end
local function from_hex(s)
  return (s:gsub("%x%x", function(h) return string.char(tonumber(h, 16)) end))
end

-- 异或混淆: 逐字节与 key 轮转异或 (Lua 5.4 位运算符)
local function xor_obfuscate(s, key)
  local n = #key
  local out = {}
  for i = 1, #s do
    local k = key:byte(((i - 1) % n) + 1)
    out[i] = string.char(s:byte(i) ~ k)
  end
  return table.concat(out)
end

local function obfuscate(s) return to_hex(xor_obfuscate(s, SAVE_KEY)) end
local function deobfuscate(s) return xor_obfuscate(from_hex(s), SAVE_KEY) end

-- 序列化技能等级 (稀疏表 [set] = grade) -> "set:grade,set:grade"
local function serialize_grades(grades)
  local parts = {}
  for set, grade in pairs(grades or {}) do
    if type(set) == "number" and type(grade) == "number" then
      table.insert(parts, string.format("%d:%d", set, grade))
    end
  end
  table.sort(parts)
  return table.concat(parts, ",")
end

-- 反序列化技能等级
local function parse_grades(s)
  local grades = {}
  if s and s ~= "" then
    for part in s:gmatch("[^,]+") do
      local set, grade = part:match("^(%d+):(%d+)$")
      if set then grades[tonumber(set)] = tonumber(grade) end
    end
  end
  return grades
end

-- 保存全部存档键 (对应 C# ConvertSaveData.ConvertData)
function save_all()
  local lines = {
    "version=" .. SAVE_VERSION,
    "coin=" .. (G.coin or 0),
    "jade=" .. (G.jade or 0),
    "soul=" .. (G.soul or 1),
    "player_level=" .. (Player.level or 1),
    "player_exp=" .. (Player.exp or 0),
    "weapon_kind=" .. (Player.weapon_kind or 0),
    "stage_index=" .. (G.stage_index or 0),
    "skill_grades=" .. serialize_grades(Player.skill_grades),
  }
  local payload = table.concat(lines, "\n")
  local f, err = io.open(save_path(), "wb")
  if not f then
    print("[save] 写入失败: " .. tostring(err))
    return false
  end
  f:write(obfuscate(payload))
  f:close()
  print("[save] 已保存 -> " .. save_path())
  return true
end

-- 读取全部存档键; 无存档/版本不符/损坏时返回 false (视为新游戏)
function load_all()
  local f = io.open(save_path(), "rb")
  if not f then return false end
  local content = f:read("*a")
  f:close()

  local ok, text = pcall(function()
    return deobfuscate(content)
  end)
  if not ok or not text or text == "" then
    print("[save] 存档损坏, 忽略")
    return false
  end

  local data = {}
  for line in text:gmatch("[^\n]+") do
    local k, v = line:match("^([^=]+)=(.*)$")
    if k then data[k] = v end
  end
  if tonumber(data.version or 0) ~= SAVE_VERSION then
    print("[save] 存档版本不匹配, 忽略")
    return false
  end

  G.coin = tonumber(data.coin or 0) or 0
  G.jade = tonumber(data.jade or 0) or 0
  G.soul = tonumber(data.soul or 1) or 1
  G.stage_index = tonumber(data.stage_index or 0) or 0
  Player.level = tonumber(data.player_level or 1) or 1
  Player.exp = tonumber(data.player_exp or 0) or 0
  Player.weapon_kind = tonumber(data.weapon_kind or 0) or 0
  Player.skill_grades = parse_grades(data.skill_grades)

  print("[save] 已读取 -> " .. save_path())
  return true
end

return {
  save_all = save_all,
  load_all = load_all,
  save_path = save_path,
}
