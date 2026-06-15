--- deepseek_npc.lua — 用 dse.http + 脚本层 JSON 调 DeepSeek 做 AI 智能 NPC（示例）
---
--- 说明：
---   * 引擎只提供「通用异步 HTTP(S) 客户端」dse.http（GET/POST，OpenSSL TLS，
---     回调在主线程/脚本线程触发，不阻塞游戏循环）。
---   * DeepSeek 的 JSON 拼装/解析、对话管理等「业务逻辑」全在脚本层（本文件），
---     不进引擎核心——这样引擎保持通用，换任何 REST API 都不用改 C++。
---   * 真实调用需你自己的 DeepSeek API Key（见下）。本文件可直接被引擎 Lua 宿主加载：
---       bin/DSEngine_lua_debug.exe --script=examples/lua/deepseek_npc.lua
---     （需用 DSE_ENABLE_HTTP=ON 构建的宿主；否则 dse.http 不存在。）
---
--- dse.http API 速查：
---   dse.http.request{ url=, method=, headers={K=V}, body=, timeout=, verify_peer=,
---                     ca_file=, on_done=function(resp) end } -> req_id
---   dse.http.get(url, on_done) -> req_id
---   dse.http.post(url, body [,content_type], on_done) -> req_id
---   dse.http.update()        -- 触发已完成回调（引擎每帧自动调用，手动驱动时用）
---   dse.http.available()     -> bool
---   resp = { id, status, body, error, ok, headers={...} }

local json = require("json")

-- ─────────────────────────── 配置 ───────────────────────────
local DEEPSEEK = {
    -- 优先读环境变量，避免把密钥写进脚本/仓库。
    api_key = os.getenv("DEEPSEEK_API_KEY") or "<在此填入或设置环境变量 DEEPSEEK_API_KEY>",
    url     = "https://api.deepseek.com/chat/completions",
    model   = "deepseek-chat",
}

-- ─────────────────────────── NPC 对象 ───────────────────────────
local NPC = {}
NPC.__index = NPC

function NPC.new(name, persona)
    return setmetatable({
        name     = name,
        busy     = false,                       -- 是否有请求在途（避免重复发）
        history  = {                            -- 对话历史（含 system 人设）
            { role = "system", content = persona },
        },
    }, NPC)
end

--- 玩家说一句话，NPC 异步回复。on_reply(text, err) 在收到回复时触发。
function NPC:say(player_text, on_reply)
    if not dse.http or not dse.http.available() then
        on_reply(nil, "dse.http 不可用：请用 DSE_ENABLE_HTTP=ON 构建引擎")
        return
    end
    if self.busy then
        on_reply(nil, self.name .. " 正在思考中…")
        return
    end
    self.busy = true
    self.history[#self.history + 1] = { role = "user", content = player_text }

    local body = json.encode({
        model    = DEEPSEEK.model,
        messages = self.history,
        stream   = false,
    })

    dse.http.request{
        url     = DEEPSEEK.url,
        method  = "POST",
        timeout = 30,
        headers = {
            ["Content-Type"]  = "application/json",
            ["Authorization"] = "Bearer " .. DEEPSEEK.api_key,
        },
        body = body,
        on_done = function(resp)
            self.busy = false
            if not resp.ok then
                on_reply(nil, string.format("HTTP 失败 status=%d error=%s body=%s",
                    resp.status, resp.error or "", resp.body or ""))
                return
            end
            local data, derr = json.decode(resp.body)
            if not data then
                on_reply(nil, "JSON 解析失败: " .. tostring(derr))
                return
            end
            local choice = data.choices and data.choices[1]
            local reply  = choice and choice.message and choice.message.content
            if not reply then
                on_reply(nil, "响应缺少 choices[1].message.content")
                return
            end
            -- 记录到历史，支持多轮对话
            self.history[#self.history + 1] = { role = "assistant", content = reply }
            on_reply(reply, nil)
        end,
    }
end

-- ─────────────────────────── 用法示例 ───────────────────────────
-- 在你的游戏逻辑里这样用（这里给一个最小演示）：
local npc = NPC.new("守卫艾伦", "你是中世纪城镇的守卫艾伦，说话简短、略带警惕，只用中文回答。")

npc:say("你好，城里最近太平吗？", function(text, err)
    if err then
        print("[NPC] 出错: " .. err)
    else
        print("[艾伦] " .. text)
    end
end)

-- 若引擎托管主循环，dse.http.update() 会被每帧自动调用，回调自然触发。
-- 若你在独立脚本里手动驱动，可这样轮询直到回调完成：
--   while npc.busy do dse.http.update(); end

return NPC
