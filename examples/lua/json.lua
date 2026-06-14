--- json.lua — 极简 JSON 编/解码器（纯 Lua，无依赖）
--- 仅用于示例：演示如何在脚本层把请求体打包成 JSON、把响应体解析回 table。
--- 引擎核心不含 JSON 逻辑（属业务/脚本层）。生产环境建议换用 lua-cjson 等成熟库。
---
--- 用法：
---   local json = require("json")
---   local s = json.encode({ model = "x", messages = { { role = "user", content = "hi" } } })
---   local t, err = json.decode(s)
---
--- 支持：nil/false/true、number、string（含常见转义）、array/object（table）。
--- 约定：空 table {} 编码为 "{}"（对象）；如需空数组请用 json.empty_array 占位。

local json = {}

json.empty_array = setmetatable({}, { __tostring = function() return "[]" end })

-- ───────────────────────────── 编码 ─────────────────────────────
local escape_map = {
    ['"'] = '\\"', ['\\'] = '\\\\', ['\b'] = '\\b',
    ['\f'] = '\\f', ['\n'] = '\\n', ['\r'] = '\\r', ['\t'] = '\\t',
}

local function escape_str(s)
    return '"' .. s:gsub('[%z\1-\31\\"]', function(c)
        return escape_map[c] or string.format('\\u%04x', c:byte())
    end) .. '"'
end

local function is_array(t)
    local n = 0
    for k in pairs(t) do
        if type(k) ~= "number" then return false end
        n = n + 1
    end
    return n == #t
end

local encode_value

local function encode_table(t)
    if t == json.empty_array then return "[]" end
    if next(t) == nil then return "{}" end
    local parts = {}
    if is_array(t) then
        for i = 1, #t do parts[i] = encode_value(t[i]) end
        return "[" .. table.concat(parts, ",") .. "]"
    end
    local i = 0
    for k, v in pairs(t) do
        i = i + 1
        parts[i] = escape_str(tostring(k)) .. ":" .. encode_value(v)
    end
    return "{" .. table.concat(parts, ",") .. "}"
end

encode_value = function(v)
    local tv = type(v)
    if tv == "nil" then return "null"
    elseif tv == "boolean" then return v and "true" or "false"
    elseif tv == "number" then return string.format("%.14g", v)
    elseif tv == "string" then return escape_str(v)
    elseif tv == "table" then return encode_table(v)
    else error("json.encode: unsupported type " .. tv) end
end

function json.encode(v)
    return encode_value(v)
end

-- ───────────────────────────── 解码 ─────────────────────────────
local function skip_ws(s, i)
    local _, j = s:find("^[ \t\r\n]*", i)
    return (j or i - 1) + 1
end

local parse_value

local unescape_map = {
    ['"'] = '"', ['\\'] = '\\', ['/'] = '/', ['b'] = '\b',
    ['f'] = '\f', ['n'] = '\n', ['r'] = '\r', ['t'] = '\t',
}

local function parse_string(s, i)
    -- 假设 s[i] == '"'
    local buf, j = {}, i + 1
    while j <= #s do
        local c = s:sub(j, j)
        if c == '"' then
            return table.concat(buf), j + 1
        elseif c == '\\' then
            local nc = s:sub(j + 1, j + 1)
            if nc == 'u' then
                local hex = s:sub(j + 2, j + 5)
                local cp = tonumber(hex, 16)
                if not cp then return nil, nil, "bad \\u escape" end
                -- 仅处理 BMP，转 UTF-8（示例够用）
                if cp < 0x80 then
                    buf[#buf + 1] = string.char(cp)
                elseif cp < 0x800 then
                    buf[#buf + 1] = string.char(0xC0 + math.floor(cp / 0x40),
                                                0x80 + (cp % 0x40))
                else
                    buf[#buf + 1] = string.char(0xE0 + math.floor(cp / 0x1000),
                                                0x80 + (math.floor(cp / 0x40) % 0x40),
                                                0x80 + (cp % 0x40))
                end
                j = j + 6
            else
                buf[#buf + 1] = unescape_map[nc] or nc
                j = j + 2
            end
        else
            buf[#buf + 1] = c
            j = j + 1
        end
    end
    return nil, nil, "unterminated string"
end

local function parse_number(s, i)
    local b, e = s:find("^-?%d+%.?%d*[eE]?[+-]?%d*", i)
    if not b then return nil, nil, "bad number" end
    return tonumber(s:sub(b, e)), e + 1
end

local function parse_array(s, i)
    local arr, j = {}, skip_ws(s, i + 1)
    if s:sub(j, j) == "]" then return arr, j + 1 end
    while true do
        local v, nj, err = parse_value(s, j)
        if err then return nil, nil, err end
        arr[#arr + 1] = v
        j = skip_ws(s, nj)
        local c = s:sub(j, j)
        if c == "]" then return arr, j + 1 end
        if c ~= "," then return nil, nil, "expected ',' or ']'" end
        j = skip_ws(s, j + 1)
    end
end

local function parse_object(s, i)
    local obj, j = {}, skip_ws(s, i + 1)
    if s:sub(j, j) == "}" then return obj, j + 1 end
    while true do
        if s:sub(j, j) ~= '"' then return nil, nil, "expected string key" end
        local key, nj, err = parse_string(s, j)
        if err then return nil, nil, err end
        j = skip_ws(s, nj)
        if s:sub(j, j) ~= ":" then return nil, nil, "expected ':'" end
        local v, vj, verr = parse_value(s, skip_ws(s, j + 1))
        if verr then return nil, nil, verr end
        obj[key] = v
        j = skip_ws(s, vj)
        local c = s:sub(j, j)
        if c == "}" then return obj, j + 1 end
        if c ~= "," then return nil, nil, "expected ',' or '}'" end
        j = skip_ws(s, j + 1)
    end
end

parse_value = function(s, i)
    i = skip_ws(s, i)
    local c = s:sub(i, i)
    if c == "{" then return parse_object(s, i)
    elseif c == "[" then return parse_array(s, i)
    elseif c == '"' then return parse_string(s, i)
    elseif c == "t" and s:sub(i, i + 3) == "true" then return true, i + 4
    elseif c == "f" and s:sub(i, i + 4) == "false" then return false, i + 5
    elseif c == "n" and s:sub(i, i + 3) == "null" then return nil, i + 4
    elseif c == "-" or c:match("%d") then return parse_number(s, i)
    else return nil, nil, "unexpected char '" .. c .. "' at " .. i end
end

--- 解析 JSON 字符串 → (value, err)。出错时 value 为 nil、err 为描述。
function json.decode(s)
    if type(s) ~= "string" then return nil, "json.decode: expected string" end
    local v, _, err = parse_value(s, 1)
    if err then return nil, err end
    return v
end

return json
