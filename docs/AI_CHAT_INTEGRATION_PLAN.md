# AI Chat 集成方案 - 基于 openai-agents-python

## 概述

本文档描述 DSEngine 编辑器内置 AI Chat 的完整集成方案，基于 OpenAI 官方的 `openai-agents-python` SDK。

---

## ✅ 关键架构决策：复用现有 MCP Adapter

`tools/mcp_adapter/dsengine_mcp.py` 已经是一个完整的 MCP Server，包含：
- 全部 20+ DSEngine 工具定义（实体/场景/Lua/截图/资产生成）
- WebSocket → ControlServer 桥接
- DALL-E 纹理生成、Meshy 3D 模型生成、ElevenLabs SFX 生成
- 截图工具 `dsengine_editor_screenshot`（可让 AI 看到视口）

`openai-agents-python` 原生支持 `MCPServerStdio`，因此 **`ai_chat_bridge.py` 不应重复定义工具**。

### 正确架构

```
C++ Editor UI
    ↕ JSON-lines (stdin/stdout)
ai_chat_bridge.py  ← 只负责 Agent 编排 + 流式协议转换（~100行）
    ↕ MCP stdio 子进程（自动启动）
dsengine_mcp.py   ← 已有！工具定义、WebSocket桥接全在这（~800行）
    ↕ WebSocket (9527)
C++ Control Server ← 已有！
```

**核心代码**：
```python
from openai_agents import Agent, MCPServerStdio

mcp_server = MCPServerStdio(
    params={"command": "python", "args": [str(mcp_adapter_path)]}
)

async def get_agent(agent_id: str) -> Agent:
    return Agent(
        name=agent_id,
        instructions=AGENT_INSTRUCTIONS[agent_id],
        mcp_servers=[mcp_server]  # 工具自动从 MCP 发现，无需手动定义！
    )
```

**好处**：工具增删只改 `dsengine_mcp.py`，`ai_chat_bridge.py` 代码量减少 75%。

---

### 核心设计原则

- **标准化**: 使用 OpenAI 官方 Agents SDK，兼容所有 OpenAI 协议提供商
- **轻量集成**: C++ 仅负责 UI 和工具执行，Python 使用标准 SDK
- **协议扩展**: JSON-lines 协议扩展支持流式、图片、多 Agent
- **配置灵活**: 支持多 Provider、多 Agent、代理设置
- **安全可靠**: API Key 加密存储、错误恢复、敏感信息过滤

---

## 架构设计

### 系统层次

```
┌─────────────────────────────────────────────────────────────┐
│                     C++ Editor UI Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Config Window│  │ Chat Panel   │  │ Agent Selector│     │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │           ChatPanel (Protocol Handler)               │   │
│  │  - JSON-lines encode/decode                         │   │
│  │  - Stream chunk aggregation                         │   │
│  │  - Image base64 decode/display                     │   │
│  │  - Tool execution via ControlServer                 │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           │
                    stdin/stdout (JSON-lines)
                           │
┌─────────────────────────────────────────────────────────────┐
│                Python Bridge (ai_chat_bridge.py)            │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         openai-agents-python Integration            │   │
│  │  - Agent definitions (SceneArchitect, ScriptWriter) │   │
│  │  - Tool wrapping (DSEngine tools)                   │   │
│  │  - Streaming support (asyncio)                      │   │
│  │  - Image generation/analysis                        │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Protocol Handler                         │   │
│  │  - stdin/stdout JSON-lines                          │   │
│  │  - Stream chunk emission                            │   │
│  │  - Image base64 encode                              │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           │
                    OpenAI API (HTTP)
                           │
┌─────────────────────────────────────────────────────────────┐
│                LLM Provider (OpenAI/Compatible)           │
│  OpenAI, DeepSeek, Ollama, Azure OpenAI, etc.               │
└─────────────────────────────────────────────────────────────┘
```

---

## 协议设计

### 原有协议

```json
// C++ -> Python
{"type": "user_message", "content": "..."}

// Python -> C++
{"type": "assistant_message", "content": "..."}
{"type": "tool_call", "name": "...", "arguments": "...", "call_id": "..."}
{"type": "tool_result", "call_id": "...", "result": "..."}
{"type": "error", "message": "..."}
{"type": "status", "message": "..."}
```

### 扩展协议

```json
// 用户消息（带 Agent 选择和图片）
{"type": "user_message", "content": "...", "agent_id": "scene_architect", "images": ["base64..."]}

// 流式片段
{"type": "stream_chunk", "content": "...", "chunk_id": 123, "is_last": false}

// 流式开始/结束
{"type": "stream_start"}
{"type": "stream_end", "chunk_id": 123}

// 图片生成
{"type": "image_generated", "image": "base64...", "prompt": "...", "model": "dall-e-3"}

// 图片分析
{"type": "image_analyzed", "content": "...", "image_id": "..."}

// Agent 切换
{"type": "agent_switch", "from": "general", "to": "scene_architect", "reason": "..."}

// Token 统计
{"type": "token_usage", "input_tokens": 100, "output_tokens": 50, "model": "gpt-4o"}
```

---

## Python Bridge 实现

### 依赖配置 (`tools/requirements_ai.txt`)

```
openai>=1.0.0
openai-agents-python>=0.1.0
Pillow>=10.0.0
tiktoken>=0.5.0
httpx>=0.25.0
```

### Agent 定义

```python
from openai import AsyncOpenAI
from openai_agents import Agent, Tool
import asyncio
import json
import base64
from typing import Dict, Any

# DSEngine 工具包装
class DSEngineTool:
    def __init__(self, name: str, description: str, params_schema: Dict[str, Any]):
        self.name = name
        self.description = description
        self.params_schema = params_schema
    
    async def __call__(self, **kwargs) -> Dict[str, Any]:
        # 发送工具调用请求到 C++
        emit({
            "type": "tool_call",
            "name": self.name,
            "arguments": json.dumps(kwargs)
        })
        
        # 等待 C++ 返回结果
        result_line = await stdin_readline()
        result_msg = json.loads(result_line)
        
        if result_msg.get("type") == "tool_result":
            return json.loads(result_msg.get("result", "{}"))
        return {"error": "No result received"}

# Agent 定义
SCENE_ARCHITECT = Agent(
    name="scene_architect",
    instructions="""You are a scene architect for DSEngine game editor.
    Focus on composition, lighting, and visual hierarchy.
    Use dsengine_entity_create to build scenes.
    Use dsengine_scene_get_state to inspect the current scene.""",
    tools=[
        DSEngineTool("dsengine_entity_create", 
            "Create a new entity with components",
            {"type": "object", "properties": {...}}),
        DSEngineTool("dsengine_entity_modify",
            "Modify entity properties",
            {"type": "object", "properties": {...}}),
        DSEngineTool("dsengine_scene_get_state",
            "Get current scene entity list",
            {"type": "object", "properties": {...}}),
    ]
)

SCRIPT_WRITER = Agent(
    name="script_writer",
    instructions="""You are a Lua scripting expert for DSEngine.
    Write clean, efficient game logic code following DSEngine conventions.
    Use dsengine_lua_execute to test code and dsengine_script_create to save files.""",
    tools=[
        DSEngineTool("dsengine_lua_execute",
            "Execute Lua code in the engine",
            {"type": "object", "properties": {"code": {"type": "string"}}}),
        DSEngineTool("dsengine_script_create",
            "Create or overwrite a Lua script file",
            {"type": "object", "properties": {...}}),
    ]
)

GENERAL_ASSISTANT = Agent(
    name="general",
    instructions="You are a helpful assistant for the DSEngine game editor."
)

# Agent 注册表
AGENT_REGISTRY = {
    "general": GENERAL_ASSISTANT,
    "scene_architect": SCENE_ARCHITECT,
    "script_writer": SCRIPT_WRITER,
}
```

### 主循环（带流式输出）

```python
import asyncio
import sys
from openai import AsyncOpenAI
from openai_agents import run_agent_stream

async def main():
    # 从环境变量或配置加载
    api_key = os.environ.get("OPENAI_API_KEY", "")
    base_url = os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1")
    model = os.environ.get("OPENAI_MODEL", "gpt-4o")
    proxy_url = os.environ.get("HTTP_PROXY", "")
    
    # 创建异步客户端
    http_client = None
    if proxy_url:
        import httpx
        http_client = httpx.AsyncClient(proxies=proxy_url)
    
    client = AsyncOpenAI(
        api_key=api_key,
        base_url=base_url,
        http_client=http_client
    )
    
    emit({"type": "status", "message": f"AI ready ({model})."})
    
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue
        
        msg_type = msg.get("type", "")
        
        if msg_type == "user_message":
            agent_id = msg.get("agent_id", "general")
            agent = AGENT_REGISTRY.get(agent_id, AGENT_REGISTRY["general"])
            content = msg.get("content", "")
            images = msg.get("images", [])
            
            # 流式运行
            emit({"type": "stream_start"})
            chunk_id = 0
            
            try:
                async for chunk in run_agent_stream(agent, content, client):
                    if chunk.type == "content":
                        emit({
                            "type": "stream_chunk",
                            "content": chunk.content,
                            "chunk_id": chunk_id,
                            "is_last": False
                        })
                        chunk_id += 1
                    elif chunk.type == "tool_call":
                        # 工具调用由 DSEngineTool 内部处理
                        pass
                    elif chunk.type == "token_usage":
                        emit({
                            "type": "token_usage",
                            "input_tokens": chunk.input_tokens,
                            "output_tokens": chunk.output_tokens,
                            "model": model
                        })
                
                emit({"type": "stream_end", "chunk_id": chunk_id})
                
            except Exception as e:
                emit({"type": "error", "message": str(e)})
```

---

## C++ 侧实现

### 协议定义 (`editor_chat_protocol.h`)

```cpp
#pragma once

#include <string>

namespace dse::editor {

enum class BridgeMessageType {
    // 原有
    UserMessage,
    AssistantMessage,
    ToolCall,
    ToolResult,
    Error,
    Status,
    // 新增
    StreamStart,
    StreamChunk,
    StreamEnd,
    ImageGenerated,
    ImageAnalyzed,
    AgentSwitch,
    TokenUsage
};

struct BridgeMessage {
    BridgeMessageType type;
    std::string content;
    std::string tool_name;
    std::string tool_args;
    std::string call_id;
    std::string agent_id;
    std::string image_base64;
    int chunk_id = 0;
    int input_tokens = 0;
    int output_tokens = 0;
    std::string model;
    bool is_last = false;
    bool valid = false;
};

BridgeMessage ParseBridgeMessage(const std::string& line);

} // namespace dse::editor
```

### ChatPanel 扩展 (`editor_chat_panel.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace dse::runtime {
class EngineInstance;
}

namespace dse::editor {

class ControlServer;

enum class ChatRole { User, Assistant, System, ToolResult };

struct ChatMessage {
    ChatRole role;
    std::string content;
    std::string agent_id;
    std::vector<uint8_t> image_data;
    bool is_streaming = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    
    float response_time_ms() const {
        auto end = end_time.time_since_epoch().count() ? end_time : std::chrono::steady_clock::now();
        return std::chrono::duration<float, std::milli>(end - start_time).count();
    }
};

class ChatPanel {
public:
    ChatPanel();
    ~ChatPanel();

    ChatPanel(const ChatPanel&) = delete;
    ChatPanel& operator=(const ChatPanel&) = delete;

    void SetBridgePath(const std::string& path);
    void Draw(ControlServer& server, dse::runtime::EngineInstance& engine);
    
    // 新增
    void SetCurrentAgent(const std::string& agent_id);
    void AttachImage(const std::vector<uint8_t>& image_data);
    void ClearHistory();
    void ExportConversation(const std::string& path);
    void ImportConversation(const std::string& path);

private:
    void SendToBridge(const std::string& text);
    void StartBridge();
    void StopBridge();
    void ExecuteToolCall(const std::string& tool_name, const std::string& args_json,
                         const std::string& call_id,
                         ControlServer& server, dse::runtime::EngineInstance& engine);
    void CheckBridgeHealth();
    
    // UI
    void DrawAgentSelector();
    void DrawImageAttachment();
    void DrawCodeBlock(const std::string& code, const std::string& language);

    char input_buf_[1024] = {};
    std::vector<ChatMessage> messages_;
    std::string current_agent_id_ = "general";
    std::vector<uint8_t> pending_image_;
    bool scroll_to_bottom_ = false;
    bool waiting_for_response_ = false;

    // Python 子进程
    std::string bridge_path_;
    std::atomic<bool> bridge_running_{false};
    std::thread reader_thread_;
    std::mutex output_mutex_;
    std::deque<std::string> pending_output_;

#ifdef _WIN32
    void* proc_handle_ = nullptr;
    void* stdin_write_ = nullptr;
    void* stdout_read_ = nullptr;
#else
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    pid_t bridge_pid_ = 0;
#endif
};

} // namespace dse::editor
```

### 流式渲染实现 (`editor_chat_panel.cpp`)

```cpp
void ChatPanel::Draw(ControlServer& server, dse::runtime::EngineInstance& engine) {
    // Process pending output from bridge
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        while (!pending_output_.empty()) {
            std::string line = std::move(pending_output_.front());
            pending_output_.pop_front();

            auto bmsg = ParseBridgeMessage(line);

            if (!bmsg.valid) {
                messages_.push_back({ChatRole::System, "[bridge] " + line});
                continue;
            }

            switch (bmsg.type) {
            case BridgeMessageType::AssistantMessage:
                messages_.push_back({ChatRole::Assistant, bmsg.content, current_agent_id_});
                waiting_for_response_ = false;
                scroll_to_bottom_ = true;
                break;
                
            case BridgeMessageType::StreamStart:
                if (messages_.empty() || messages_.back().role != ChatRole::Assistant) {
                    messages_.push_back({ChatRole::Assistant, "", current_agent_id_});
                    messages_.back().start_time = std::chrono::steady_clock::now();
                }
                messages_.back().is_streaming = true;
                break;
                
            case BridgeMessageType::StreamChunk:
                if (!messages_.empty() && messages_.back().role == ChatRole::Assistant) {
                    messages_.back().content += bmsg.content;
                    messages_.back().last_update = std::chrono::steady_clock::now();
                }
                scroll_to_bottom_ = true;
                break;
                
            case BridgeMessageType::StreamEnd:
                if (!messages_.empty()) {
                    messages_.back().is_streaming = false;
                    messages_.back().end_time = std::chrono::steady_clock::now();
                }
                waiting_for_response_ = false;
                break;
                
            case BridgeMessageType::ToolCall:
                messages_.push_back({ChatRole::System, "Calling: " + bmsg.tool_name});
                scroll_to_bottom_ = true;
                ExecuteToolCall(bmsg.tool_name, bmsg.tool_args, bmsg.call_id, server, engine);
                break;
                
            case BridgeMessageType::Error:
                messages_.push_back({ChatRole::System, "Error: " + bmsg.content});
                waiting_for_response_ = false;
                scroll_to_bottom_ = true;
                break;
                
            case BridgeMessageType::Status:
                messages_.push_back({ChatRole::System, bmsg.content});
                scroll_to_bottom_ = true;
                break;
                
            case BridgeMessageType::ImageGenerated:
                // 解码 base64 图片
                messages_.push_back({ChatRole::Assistant, "[Generated Image]", current_agent_id_});
                // TODO: base64 解码
                break;
                
            case BridgeMessageType::TokenUsage:
                // 更新 token 统计
                break;
                
            default:
                messages_.push_back({ChatRole::System, "[bridge] " + line});
                break;
            }
        }
    }

    // ... UI 渲染代码
}
```

---

## 配置系统

### 配置结构 (`editor_ai_config.h`)

```cpp
#pragma once

#include <string>
#include <vector>

namespace dse::editor {

struct AIProviderConfig {
    std::string name = "OpenAI";
    std::string api_key;
    std::string base_url = "https://api.openai.com/v1";
    std::string model = "gpt-4o";
    std::string image_model = "dall-e-3";
    std::string proxy_url;
    int timeout_ms = 30000;
    float temperature = 0.7f;
    int max_tokens = 4096;
};

struct AIConfig {
    std::vector<AIProviderConfig> providers;
    int current_provider_index = 0;
    bool enable_streaming = true;
    bool enable_images = true;
    std::string default_agent = "general";
    bool debug_mode = false;
    bool log_raw_protocol = false;
};

class AIConfigManager {
public:
    static AIConfigManager& Instance();
    
    void Load(const std::string& path);
    void Save(const std::string& path);
    AIConfig& GetConfig();
    
    void DrawConfigWindow();
    void ShowConfigWindow(bool show = true);
    
    // API Key 加密
    std::string EncryptAPIKey(const std::string& key);
    std::string DecryptAPIKey(const std::string& encrypted_key);

private:
    AIConfigManager() = default;
    AIConfig config_;
    bool show_config_ = false;
};

} // namespace dse::editor
```

### 配置窗口 UI (`editor_ai_config.cpp`)

```cpp
void AIConfigManager::DrawConfigWindow() {
    if (!show_config_) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("AI Configuration", &show_config_)) {
        // Provider 选择
        if (ImGui::BeginCombo("Provider", config_.providers[config_.current_provider_index].name.c_str())) {
            for (size_t i = 0; i < config_.providers.size(); ++i) {
                if (ImGui::Selectable(config_.providers[i].name.c_str(), i == config_.current_provider_index)) {
                    config_.current_provider_index = i;
                }
            }
            ImGui::EndCombo();
        }
        
        auto& provider = config_.providers[config_.current_provider_index];
        
        ImGui::Separator();
        
        // API Key (密码掩码)
        char key_buf[256];
        strncpy(key_buf, provider.api_key.c_str(), sizeof(key_buf));
        if (ImGui::InputText("API Key", key_buf, sizeof(key_buf), ImGuiInputTextFlags_Password)) {
            provider.api_key = key_buf;
        }
        
        // Base URL
        char url_buf[256];
        strncpy(url_buf, provider.base_url.c_str(), sizeof(url_buf));
        if (ImGui::InputText("Base URL", url_buf, sizeof(url_buf))) {
            provider.base_url = url_buf;
        }
        
        // Model
        char model_buf[64];
        strncpy(model_buf, provider.model.c_str(), sizeof(model_buf));
        if (ImGui::InputText("Model", model_buf, sizeof(model_buf))) {
            provider.model = model_buf;
        }
        
        // 代理
        char proxy_buf[256];
        strncpy(proxy_buf, provider.proxy_url.c_str(), sizeof(proxy_buf));
        if (ImGui::InputText("Proxy", proxy_buf, sizeof(proxy_buf))) {
            provider.proxy_url = proxy_buf;
        }
        
        // Temperature
        ImGui::SliderFloat("Temperature", &provider.temperature, 0.0f, 2.0f);
        
        // Max Tokens
        ImGui::InputInt("Max Tokens", &provider.max_tokens);
        
        ImGui::Separator();
        
        // 全局设置
        ImGui::Checkbox("Enable Streaming", &config_.enable_streaming);
        ImGui::Checkbox("Enable Images", &config_.enable_images);
        ImGui::Checkbox("Debug Mode", &config_.debug_mode);
        ImGui::Checkbox("Log Raw Protocol", &config_.log_raw_protocol);
        
        ImGui::Separator();
        
        // 默认 Agent
        char agent_buf[64];
        strncpy(agent_buf, config_.default_agent.c_str(), sizeof(agent_buf));
        if (ImGui::InputText("Default Agent", agent_buf, sizeof(agent_buf))) {
            config_.default_agent = agent_buf;
        }
        
        ImGui::Separator();
        
        // 按钮
        if (ImGui::Button("Save")) {
            Save("editor_ai_config.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Provider")) {
            config_.providers.push_back({"New Provider"});
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Provider") && config_.providers.size() > 1) {
            config_.providers.erase(config_.providers.begin() + config_.current_provider_index);
            config_.current_provider_index = 0;
        }
    }
    ImGui::End();
}
```

### API Key 加密存储

```cpp
#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>

std::string AIConfigManager::EncryptAPIKey(const std::string& key) {
    DATA_BLOB in, out;
    in.pbData = (BYTE*)key.data();
    in.cbData = key.size();
    
    if (CryptProtectData(&in, L"DSEngine AI Key", nullptr, nullptr, nullptr, 0, &out)) {
        std::string result((char*)out.pbData, out.cbData);
        LocalFree(out.pbData);
        return result;
    }
    return key; // Fallback
}

std::string AIConfigManager::DecryptAPIKey(const std::string& encrypted) {
    DATA_BLOB in, out;
    in.pbData = (BYTE*)encrypted.data();
    in.cbData = encrypted.size();
    
    if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        std::string result((char*)out.pbData, out.cbData);
        LocalFree(out.pbData);
        return result;
    }
    return encrypted; // Fallback
}
#else
// Linux/macOS: 使用 keyring 或简单的 XOR 混淆
std::string AIConfigManager::EncryptAPIKey(const std::string& key) {
    // 简单实现：XOR 混淆
    std::string result = key;
    const char key_xor[] = "DSEngineAIKeyXOR";
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] ^= key_xor[i % sizeof(key_xor)];
    }
    return result;
}

std::string AIConfigManager::DecryptAPIKey(const std::string& encrypted) {
    return EncryptAPIKey(encrypted); // XOR 是对称的
}
#endif
```

---

## 对话历史管理

### 对话历史结构 (`editor_ai_history.h`)

```cpp
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "editor_chat_panel.h"

namespace dse::editor {

struct ConversationEntry {
    ChatRole role;
    std::string content;
    std::string agent_id;
    std::vector<uint8_t> image_data;
    std::chrono::system_clock::time_point timestamp;
};

struct ConversationHistory {
    std::string conversation_id;
    std::string title;
    std::vector<ConversationEntry> messages;
    std::string agent_id;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_modified;
    
    void Save(const std::string& path);
    void Load(const std::string& path);
};

class ConversationManager {
public:
    static ConversationManager& Instance();
    
    void NewConversation();
    void SaveCurrentConversation(const std::string& path);
    void LoadConversation(const std::string& path);
    void DeleteConversation(const std::string& id);
    std::vector<ConversationHistory> ListConversations();
    void SetCurrentConversation(ConversationHistory* conv);
    
private:
    ConversationManager() = default;
    std::vector<ConversationHistory> conversations_;
    ConversationHistory* current_ = nullptr;
    std::string history_dir_ = "ai_conversations/";
};

} // namespace dse::editor
```

---

## 菜单栏集成

### AI 菜单 (`editor_shell.cpp`)

```cpp
void DrawEditorMainMenu(EditorContext& ctx, bool* show_preferences, bool* show_plugins, 
                        bool* show_chat, const PanelVisibility* panels) {
    if (ImGui::BeginMainMenuBar()) {
        // ... 现有菜单 ...
        
        if (ImGui::BeginMenu("AI")) {
            if (ImGui::MenuItem("Configuration")) {
                AIConfigManager::Instance().ShowConfigWindow();
            }
            ImGui::Separator();
            
            if (ImGui::BeginMenu("Select Agent")) {
                const auto& agents = GetAvailableAgents();
                for (const auto& agent : agents) {
                    if (ImGui::MenuItem(agent.name.c_str(), false, 
                                        chat_panel.GetCurrentAgent() == agent.id)) {
                        chat_panel.SetCurrentAgent(agent.id);
                    }
                }
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Clear Conversation")) {
                chat_panel.ClearHistory();
            }
            
            if (ImGui::MenuItem("Export Conversation")) {
                // 打开文件保存对话框
            }
            
            if (ImGui::MenuItem("Import Conversation")) {
                // 打开文件加载对话框
            }
            
            ImGui::EndMenu();
        }
        
        // ... 其他菜单 ...
    }
    ImGui::EndMainMenuBar();
}
```

---

## 安全性

### 敏感信息过滤

```python
import re

SENSITIVE_PATTERNS = [
    r"API[_-]?KEY\s*[:=]\s*['\"]?[a-zA-Z0-9_-]+['\"]?",
    r"SECRET\s*[:=]\s*['\"]?[a-zA-Z0-9_-]+['\"]?",
    r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}",
    r"password\s*[:=]\s*['\"]?[^\s'\"]+['\"]?",
]

def sanitize_response(content: str) -> str:
    for pattern in SENSITIVE_PATTERNS:
        content = re.sub(pattern, "[REDACTED]", content, flags=re.IGNORECASE)
    return content
```

### 工具权限控制

```cpp
struct AgentDefinition {
    std::string id;
    std::string name;
    std::string instructions;
    std::vector<std::string> allowed_tools;
    std::vector<std::string> blocked_tools;
    bool allow_file_write = false;
    bool allow_file_delete = false;
    bool allow_code_execution = true;
};
```

---

## 错误处理

### 网络重连

```python
class ResilientClient:
    def __init__(self, max_retries=3, backoff_factor=2):
        self.max_retries = max_retries
        self.backoff_factor = backoff_factor
    
    async def call_with_retry(self, func, *args, **kwargs):
        for attempt in range(self.max_retries):
            try:
                return await func(*args, **kwargs)
            except (ConnectionError, TimeoutError) as e:
                if attempt == self.max_retries - 1:
                    raise
                emit({"type": "status", 
                      "message": f"Connection failed, retrying in {self.backoff_factor ** attempt}s..."})
                await asyncio.sleep(self.backoff_factor ** attempt)
```

### 子进程崩溃恢复

```cpp
void ChatPanel::CheckBridgeHealth() {
#ifdef _WIN32
    if (bridge_running_ && proc_handle_) {
        DWORD exit_code;
        if (GetExitCodeProcess(proc_handle_, &exit_code) && exit_code != STILL_ACTIVE) {
            EditorLog(LogLevel::Warning, "[ChatPanel] Bridge crashed (exit code: %d), restarting...", exit_code);
            StopBridge();
            StartBridge();
        }
    }
#else
    // Linux/macOS 实现
#endif
}
```

---

## 性能监控

### Token 统计

```cpp
struct TokenStats {
    int input_tokens = 0;
    int output_tokens = 0;
    float estimated_cost = 0.0f;
    
    void Update(const std::string& model, int input, int output);
    std::string FormatCost() const;
    
private:
    float GetPricePer1KTokens(const std::string& model, bool is_input);
};
```

### 响应时间显示

```cpp
void ChatPanel::DrawMessage(const ChatMessage& msg) {
    // ... 消息渲染 ...
    
    if (msg.role == ChatRole::Assistant && !msg.is_streaming) {
        float response_time = msg.response_time_ms();
        ImGui::SameLine();
        ImGui::TextDisabled("%.1fms", response_time);
    }
}
```

---

## 高级功能

### 代码块语法高亮

```cpp
void ChatPanel::DrawCodeBlock(const std::string& code, const std::string& language) {
    // 使用 ImGuiColorTextEdit 或类似库
    // 支持 Lua, GLSL, C++, Python 等语法高亮
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::BeginChild("code_block", ImVec2(0, 200), true);
    
    // 渲染带语法高亮的代码
    // ...
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    // 操作按钮
    ImGui::SameLine();
    if (ImGui::Button("Apply to Script")) {
        ApplyCodeToScript(code);
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy to Clipboard")) {
        ImGui::SetClipboardText(code.c_str());
    }
}
```

### 自定义 Agent 定义

```json
// agents/custom_agents.json
{
    "agents": [
        {
            "id": "shader_expert",
            "name": "Shader Expert",
            "instructions": "You are a GLSL shader expert specializing in PBR materials.",
            "tools": [
                "dsengine_shader_create",
                "dsengine_shader_compile",
                "dsengine_material_create"
            ]
        },
        {
            "id": "physics_tuner",
            "name": "Physics Tuner",
            "instructions": "You specialize in physics simulation and collision tuning.",
            "tools": [
                "dsengine_entity_modify",
                "dsengine_physics_set_params"
            ]
        }
    ]
}
```

---

## 实施计划

> **最后更新**: 2026-05-21

### MVP 完成情况

**Phase 1: 基础集成** ✅ 已完成
- [x] 添加 Python 依赖配置 (`tools/requirements_ai.txt`) — `openai`, `mcp`
- [x] 扩展协议定义 (`editor_chat_protocol.h`) — 流式/Token/图片/Cancel 全部扩展
- [x] 改造 `ai_chat_bridge.py` — 标准 OpenAI API + MCP SDK 集成
- [x] C++ 侧协议解析扩展 — `ParseBridgeMessage` 处理全部消息类型

**Phase 2: 流式输出** ✅ 已完成
- [x] Python bridge 实现流式输出 — 10字符/包模拟流式 + `stream_chunk`
- [x] C++ 侧流式片段累积和渲染 — `is_streaming` 标志 + `last_update` 时间戳
- [x] 流式结束检测 — `stream_end` 消息 + `is_streaming = false`
- [x] Stop 按钮 / 流取消 — `cancel` 消息 + `asyncio.CancelledError`

**Phase 3: 配置系统** ✅ 已完成
- [x] 实现 `AIConfigManager` — 单例，支持多 Provider
- [x] 配置窗口 UI — ImGui 完整配置界面
- [x] 菜单栏 AI 菜单集成 — 独立 AI 菜单（含 Chat Panel + Configuration）
- [x] 配置持久化 — `bin/editor_ai_config.json` JSON 序列化
- [x] **API Key 加密存储** — Windows DPAPI + XOR fallback，hex 编码安全序列化
- [x] **配置注入 bridge** — `StartBridge` 时通过 `SetEnvironmentVariableA` 注入

**Phase 4: Agent 系统** ✅ 已完成
- [x] 定义内置 Agent — `general` / `scene_architect` / `script_writer` / `asset_manager`
- [x] Agent 选择 UI — ImGui Combo 下拉选择
- [x] Agent 切换协议支持 — `agent_id` 随每条消息传递

**Phase 5: 对话管理** ✅ 已完成 (2026-05-21)
- [x] 对话历史持久化 (`SaveHistory`/`LoadHistory`，JSON 格式，启动自动加载)
- [ ] 对话导出/导入
- [ ] 对话列表 UI

**Phase 6: 错误处理** ✅ 已完成 (2026-05-21)
- [x] 网络重连机制（bridge 崩溃检测 + 橙色 Reconnect 按钮）
- [x] 子进程崩溃恢复（reader thread 设置 bridge_crashed_ 标志）
- [x] API 限流处理（指数退避重试，区分 RateLimit/Connection/Auth/ServerError）

### Bug 修复记录（2026-05-21 审查）

本次代码审查发现并修复了 5 个 Bug：

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | `editor_ai_config.cpp` | DPAPI/XOR 二进制直接写 JSON 导致 null byte 截断 | hex 编码 + `dpapi:` / `xor:` 前缀 |
| 2 | `editor_chat_panel.cpp` | `AIConfigManager` 配置从未传给 bridge 子进程 | `StartBridge` 时 `SetEnvironmentVariableA` 注入 |
| 3 | `ai_chat_bridge.py` | `tools=[]` 空列表传 OpenAI API 报错 | `tools = schemas if schemas else None` |
| 4 | `editor_shell.cpp` | Window 菜单重复 "AI Chat" 条目 | 从 Window 菜单移除，统一到 AI 菜单 |
| 5 | `editor_chat_panel.cpp` | `ExecuteToolCall` 在持有 `output_mutex_` 时执行阻塞 reader 线程 | tool calls 存入局部向量，锁释放后执行 |

### V1.0 完整版本 (2026-05-21 部分完成)

- [x] Token 使用统计显示（底部右对齐 `in:N out:N`，累计跨轮次）
- [x] 响应时间显示（每条 Assistant 消息旁显示耗时 ms）
- [ ] 代码块语法高亮
- [ ] 代码一键应用
- [ ] 图片上传和渲染 UI
- [ ] 敏感信息过滤
- [ ] 工具权限控制
- [x] @mention 上下文注入（`@scene` / `@entity` / `@selection` / `@script:path`）
- [x] Markdown 渲染（标题/列表/代码块/粗体/内联代码）
- [x] 消息编辑/重发（hover 显示 Edit + Resend 按钮）

### V2.0 高级版本

- [ ] 自定义 Agent 和工具注册
- [ ] 图片编辑/变体
- [ ] 音频输入/输出
- [ ] RAG 集成
- [ ] 国际化支持
- [ ] 快捷键
- [ ] 多模态完整支持

---

## 文件结构

```
apps/editor_cpp/src/
├── editor_ai_config.h/cpp          # 配置管理
├── editor_ai_history.h/cpp         # 历史记录
├── editor_chat_protocol.h          # 协议定义
├── editor_chat_panel.h/cpp         # (修改) 流式/Agent/图片支持
tools/
├── requirements_ai.txt             # Python 依赖
├── ai_chat_bridge.py               # (修改) Agents SDK 集成
└── agents/
    └── custom_agents.json          # 自定义 Agent 定义
bin/
└── editor_ai_config.json           # 配置文件 (运行时生成)
ai_conversations/                    # 对话历史目录
└── *.json                          # 对话文件
```

---

## 补充功能

### 流取消（Stop Generation）

```json
// C++ -> Python
{"type": "cancel"}
```

```python
current_task: asyncio.Task = None

if msg_type == "cancel":
    if current_task and not current_task.done():
        current_task.cancel()
        emit({"type": "status", "message": "Generation cancelled."})
```

C++ 侧：Send 按钮在生成中变为 **Stop** 按钮。

---

### @mention 上下文注入

```cpp
void ChatPanel::ProcessMentions(std::string& text) {
    if (text.find("@scene") != std::string::npos)
        AppendSceneState(text);
    if (text.find("@selected") != std::string::npos)
        AppendSelectedEntityInfo(text);
    if (text.find("@error") != std::string::npos)
        AppendLastConsoleErrors(text);
    if (text.find("@screenshot") != std::string::npos)
        AttachViewportScreenshot();
}
```

---

### Markdown 渲染

引入 `imgui_md` 库（轻量），支持 `**bold**`, `` `code` ``, `# heading` 等渲染，而非显示原始文本。

---

### Prompt 预设模板

```cpp
const std::vector<std::pair<std::string, std::string>> PROMPT_TEMPLATES = {
    {"创建基础场景",  "帮我创建包含地面、天空光和点光源的基础场景"},
    {"生成 Lua 脚本", "为当前选中实体编写旋转动画 Lua 脚本"},
    {"生成 PBR 纹理", "生成一张无缝 PBR 混凝土纹理并导入场景"},
    {"截图分析",      "@screenshot 分析当前场景视觉效果，给出改进建议"},
    {"性能优化建议",  "分析当前场景，给出性能优化建议"},
};
```

---

### asyncio 与同步 stdin 的正确桥接

```python
async def main():
    loop = asyncio.get_event_loop()
    stdin_queue: asyncio.Queue = asyncio.Queue()
    
    def stdin_reader():
        for line in sys.stdin:
            asyncio.run_coroutine_threadsafe(stdin_queue.put(line), loop)
    
    threading.Thread(target=stdin_reader, daemon=True).start()
    
    while True:
        line = await stdin_queue.get()
        msg = json.loads(line.strip())
        # 处理消息，支持取消
        current_task = asyncio.create_task(handle_message(msg))
```

---

### 消息编辑/重发

```cpp
if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Edit & Resend")) {
        strncpy(input_buf_, msg.content.c_str(), sizeof(input_buf_));
        messages_.erase(messages_.begin() + msg_idx, messages_.end());
    }
    if (ImGui::MenuItem("Copy")) {
        ImGui::SetClipboardText(msg.content.c_str());
    }
    ImGui::EndPopup();
}
```

---

## 参考资源

- [OpenAI Agents Python SDK](https://github.com/openai/openai-agents-python)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ImGui Documentation](https://github.com/ocornut/imgui)
