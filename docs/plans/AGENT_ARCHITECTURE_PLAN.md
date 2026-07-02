# DSEngine Editor Agent 架构设计方案

> 版本: 2.0 | 日期: 2026-07-01
> 目标: 统一 AI 交互入口，让 AI 能自主规划、调度、执行完整的游戏开发任务
> 
> **v2.0 变更 (2026-07-01)**: 合并 AI Chat 进 Agent 架构（统一面板，消除双系统），
> 修复 7 项架构/设计/安全问题（工具调用路径、验证策略、安全沙箱等）

---

## 一、现状分析与问题定义

### 1.1 当前架构

```
用户输入 "做一个平台跳跃游戏"
    ↓
ChatPanel (ImGui)
    ↓ JSON-lines
ai_chat_bridge.py (OpenAI API + MCP)
    ↓ tool_call
dsengine_mcp.py → ControlServer → 引擎
```

**问题**: 当前是 **单轮 request-response** 模式。AI 收到一条消息，执行一轮工具调用，返回结果。无法：

| 缺失能力 | 后果 |
|----------|------|
| 任务分解 | "做一个平台跳跃游戏" 无法自动拆分为可执行步骤 |
| 多步规划 | 无法处理步骤间的依赖关系（先建场景再写脚本） |
| 执行调度 | 无法自动串行/并行执行子任务 |
| 状态感知 | 不知道上一步执行后场景变成什么样 |
| 错误恢复 | 工具调用失败后没有重试/替代策略 |
| 审计日志 | 无法回溯 AI 做了什么、花了多少 token |
| 人机协作 | 关键决策点无法暂停等待用户确认 |
| 上下文管理 | 长任务 token 超限后对话断裂 |

**额外问题**: 现有 ChatPanel + ai_chat_bridge.py 与 Agent 系统形成两套并行系统——两套工具调用路径、两套消息协议、双倍维护成本。Agent 实现后 Chat 应被合并而非共存。

### 1.2 目标

实现一个 **统一 Agent 层**，取代现有 ChatPanel + ai_chat_bridge.py，使 AI 能够：
1. **统一入口**: 简单操作（直接执行）和复杂任务（规划执行）走同一个面板
2. 将高层目标分解为可执行子任务（TaskPlan）
3. 按依赖顺序自动调度执行（TaskScheduler）
4. 每步执行后感知状态变化（截图 + 场景 diff）
5. 关键节点暂停等待用户审批（Human-in-the-loop）
6. 完整记录执行过程（AuditLog）
7. 多个专业 Agent 协作完成复杂任务（Orchestrator）

### 1.3 AI Chat 合并策略

**不是删除 Chat，而是将 Chat 能力吸收进 Agent 架构。**

| 场景 | 旧方案 (双系统) | 新方案 (统一) |
|------|----------------|---------------|
| "创建一个红方块" | ChatPanel → ai_chat_bridge.py | Agent 直接执行模式 (跳过 plan/approve) |
| "做一个平台跳跃游戏" | 无法处理 | Agent 完整模式 (plan → approve → execute) |
| 对话问答 ("场景里有什么?") | ChatPanel | Agent 对话模式 (无工具调用) |

**合并后可移除的文件**（Phase 2 完成后）：
- `apps/editor_cpp/src/editor_chat_panel.h/cpp` → 被 `editor_agent_panel.h/cpp` 取代
- `tools/ai_chat_bridge.py` → 被 `tools/agent/agent_bridge.py` 取代
- `apps/editor_cpp/src/editor_chat_protocol.h` → 协议合入 `editor_agent_protocol.h`

**保留不动的文件**：
- `apps/editor_cpp/src/editor_ai_config.h/cpp` → Agent 复用（Provider/API Key/代理配置）
- `tools/mcp_adapter/dsengine_mcp.py` → 外部 AI 客户端继续使用

---

## 二、开源框架选型

### 2.1 候选框架对比

| 框架 | 维护方 | Stars | 核心模型 | 优势 | 劣势 | 适配 DSEngine |
|------|--------|-------|---------|------|------|--------------|
| **LangGraph** | LangChain | 12k+ | 状态机/图 | 精确控制流、检查点持久化、HITL 原生支持 | 学习曲线较陡 | **最佳** |
| OpenAI Agents SDK | OpenAI | 15k+ | Agent + Handoff | 轻量、原生 tool calling | 无任务规划/调度/审计，仅 OpenAI 兼容 | 现有基础 |
| AutoGen | Microsoft | 40k+ | 多 Agent 对话 | 灵活对话模式 | 过度复杂、对话驱动不适合确定性工作流 | 中 |
| CrewAI | CrewAI | 25k+ | 角色 + 任务 | API 简单、角色分工清晰 | 控制流较弱、检查点有限 | 中 |
| Semantic Kernel | Microsoft | 22k+ | Plugin + Planner | 企业级、多语言 | C#/.NET 偏重、Python 版功能滞后 | 低 |
| Dify | Dify | 55k+ | 可视化工作流 | 拖拽式、开箱即用 | 独立服务、不适合嵌入编辑器进程 | 低 |

### 2.2 推荐方案: LangGraph + 现有 MCP 工具链

**选择 LangGraph 的理由：**

1. **状态机模型与游戏开发工作流天然契合**  
   游戏开发是高度结构化的流程（场景搭建 → 脚本编写 → 测试 → 调优），LangGraph 的有向图模型精确映射这种工作流。

2. **内置检查点持久化**  
   LangGraph 的 `MemorySaver` / `SqliteSaver` 可以在每个节点执行后保存完整状态。编辑器关闭后重启可以恢复到上次的执行进度。

3. **原生 Human-in-the-loop**  
   `interrupt_before` / `interrupt_after` 机制允许在关键节点暂停等待用户确认，完美适配 "AI 展示计划 → 用户审批 → 继续执行" 的交互模式。

4. **与现有架构零冲突**  
   - 现有 MCP 工具链完全保留（`dsengine_mcp.py` 23 个工具不动）
   - 现有 `ai_chat_bridge.py` 保留为简单对话模式
   - Agent 层作为新增 Python 模块，通过相同的 JSON-lines 协议与 C++ 通信

5. **LLM 无关**  
   通过 `langchain-openai` 适配器支持 OpenAI、DeepSeek、Ollama 等所有兼容 API。

6. **生产就绪**  
   LangChain 生态最成熟的组件之一，有大量生产案例。

### 2.3 架构层次

```
┌─────────────────────────────────────────────────────────┐
│                    C++ Editor UI                         │
│  AgentPanel (统一面板，取代 ChatPanel)                     │
│  - 对话区 (流式输出、Markdown、@mention)                  │
│  - 任务列表 (子任务状态、进度条)                          │
│  - 审批按钮 (Approve / Reject / Modify)                  │
│  - 审计日志查看器                                        │
│  - 配置入口 (复用 AIConfigManager)                       │
└────────────────────┬────────────────────────────────────┘
                     │ JSON-lines (stdin/stdout)
                     │ 工具调用: tool_call → C++ DispatchTool (同进程直调)
                     │
┌────────────────────┴────────────────────────────────────┐
│          Python Agent Layer (agent_bridge.py 统一入口)    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │     agent_bridge.py (取代 ai_chat_bridge.py)      │   │
│  │  - 统一消息入口                                    │   │
│  │  - 复杂度分类: 规则匹配 (非 LLM 调用)             │   │
│  │  - 简单请求 → 直接执行 (跳过 plan/approve)        │   │
│  │  - 复杂任务 → agent_orchestrator                  │   │
│  │  - 工具调用走 JSON-lines tool_call (非 MCP)       │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │          agent_orchestrator.py (~300 行)           │   │
│  │  - LangGraph StateGraph 定义                      │   │
│  │  - 节点: plan → approve → execute → verify → done │   │
│  │  - 条件边: 重试 / 跳过 / 回退                     │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │          agent_planner.py (~200 行)               │   │
│  │  - 高层目标 → 子任务 DAG 分解                     │   │
│  │  - 子任务依赖关系推导                             │   │
│  │  - 预估工作量 / token 消耗                        │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │          agent_executor.py (~250 行)              │   │
│  │  - 单个子任务的执行循环                            │   │
│  │  - 工具调用 + 错误恢复                            │   │
│  │  - 执行前后状态快照 (截图 + scene diff)           │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │          agent_specialists.py (~150 行)           │   │
│  │  - SceneArchitect / ScriptWriter / AssetManager   │   │
│  │  - QATester / PhysicsTuner                        │   │
│  │  - 每个 Specialist 有专属 system prompt + 工具集  │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │     agent_safety.py (~80 行)                      │   │
│  │  - 安全策略 (禁用危险操作、频率限制、代码长度限制) │   │
│  │  - 工具调用拦截与审计                             │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │          agent_audit.py (~100 行)                 │   │
│  │  - 执行日志 (每步: 输入/输出/耗时/token)          │   │
│  │  - 汇总报告生成                                   │   │
│  │  - SQLite 持久化                                  │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │     dsengine_mcp.py (现有, 不改)                  │   │
│  │  - 23 个 MCP Tool 定义                            │   │
│  │  - 仅供外部 AI 客户端 (Cursor/Claude Desktop)     │   │
│  │  - Agent 内部不走此路径                           │   │
│  └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

### 2.4 工具调用路径对比 (v2.0 优化)

```
v1.0 (有问题):
  Agent → MCP stdio subprocess → WebSocket → ControlServer → 引擎
  (多一次子进程 + WebSocket 往返，延迟 ~100ms/次)

v2.0 (修正):
  Agent → JSON-lines tool_call → C++ ExecuteToolCall() → DispatchTool() → 引擎
  (同进程直调，延迟 <1ms/次，复用现有 ChatPanel 的成熟路径)

外部客户端 (不变):
  Cursor/Claude → MCP stdio → dsengine_mcp.py → WebSocket → ControlServer
```

---

## 三、核心设计

### 3.1 LangGraph 状态机定义

```python
# agent_orchestrator.py

from typing import TypedDict, Literal, Annotated
from langgraph.graph import StateGraph, START, END
from langgraph.checkpoint.sqlite import SqliteSaver

class TaskItem(TypedDict):
    id: str
    title: str
    description: str
    specialist: str          # "scene_architect" | "script_writer" | ...
    dependencies: list[str]  # 前置任务 id 列表
    status: str              # "pending" | "running" | "done" | "failed" | "skipped"
    result: str              # 执行结果摘要
    error: str               # 错误信息
    retry_count: int
    tools_used: list[str]
    tokens_used: int
    duration_ms: float

class AgentState(TypedDict):
    # ── 用户输入 ──
    user_goal: str                    # 原始高层目标
    user_constraints: list[str]       # 用户约束 ("不要用物理引擎", "像素风格")
    execution_mode: str               # "direct" | "full" (由 classify 节点设置)

    # ── 任务规划 ──
    task_plan: list[TaskItem]         # 子任务列表 (DAG)
    current_task_index: int           # 当前执行到第几个任务

    # ── 执行状态 ──
    scene_checkpoint_path: str        # Agent 执行前的场景快照路径 (一键回滚用)
    scene_snapshot_before: str        # 单步执行前场景 JSON 摘要
    scene_snapshot_after: str         # 单步执行后场景 JSON 摘要
    screenshot_before: str            # 执行前截图 (base64)
    screenshot_after: str             # 执行后截图 (base64)

    # ── 审批 ──
    approval_status: str              # "pending" | "approved" | "rejected" | "modified"
    user_feedback: str                # 用户修改意见

    # ── 全局 ──
    messages: list[dict]              # 对话历史 (用于上下文)
    total_tokens: int
    total_duration_ms: float
    error_count: int
    max_retries: int                  # 默认 3


# ── 状态机图定义 ──

graph = StateGraph(AgentState)

graph.add_node("classify",  classify_node)   # 复杂度分类 (规则匹配, 无 LLM 调用)
graph.add_node("direct",    direct_node)     # 简单请求: 直接 LLM + tool call (原 Chat 能力)
graph.add_node("checkpoint",checkpoint_node) # 保存场景快照 (用于一键回滚)
graph.add_node("plan",      plan_node)       # 任务分解
graph.add_node("approve",   approve_node)    # 人机审批
graph.add_node("execute",   execute_node)    # 执行当前子任务
graph.add_node("verify",    verify_node)     # 验证执行结果 (确定性优先, LLM 可选)
graph.add_node("summarize", summarize_node)  # 生成最终报告

graph.add_edge(START, "classify")

graph.add_conditional_edges("classify", route_by_complexity, {
    "direct":     "direct",       # 简单请求 → 直接执行 (跳过规划)
    "checkpoint": "checkpoint",   # 复杂任务 → 先保存场景快照
})

graph.add_edge("direct", END)               # 简单请求执行完即结束
graph.add_edge("checkpoint", "plan")
graph.add_edge("plan", "approve")

graph.add_conditional_edges("approve", route_after_approve, {
    "execute":   "execute",     # 用户批准 → 开始执行
    "plan":      "plan",        # 用户修改 → 重新规划
    "end":       END,           # 用户取消
})

graph.add_edge("execute", "verify")

graph.add_conditional_edges("verify", route_after_verify, {
    "execute":   "execute",     # 验证失败 + 重试次数未达上限 → 重试
    "next_task": "execute",     # 验证通过 + 还有下一个任务 → 继续
    "summarize": "summarize",   # 所有任务完成 → 汇总
    "approve":   "approve",     # 验证失败 + 重试用尽 → 请求用户介入
})

graph.add_edge("summarize", END)

# 关键: 在 approve 节点前中断，等待用户输入
agent = graph.compile(
    checkpointer=SqliteSaver(
        os.path.join(project_dir, ".dse", "agent_state.db")
    ),
    interrupt_before=["approve"],
)
```

### 3.2 状态机可视化

```
                    ┌─────────┐
                    │  START   │
                    └────┬────┘
                         │
                    ┌────▼──────┐
                    │ classify  │ (规则匹配, 零延迟, 无 LLM)
                    └────┬──────┘
                         │
              ┌──────────┴──────────┐
              │ simple              │ complex
              │                     │
         ┌────▼────┐         ┌─────▼──────┐
         │ direct  │         │ checkpoint │ (场景快照, 一键回滚)
         │ (原Chat)│         └─────┬──────┘
         └────┬────┘               │
              │              ┌─────▼────┐
              │        ┌─────│   plan   │◄────────────────┐
              │        │     └────┬─────┘                  │
              │        │          │                        │
              │        │     ┌────▼──────┐    user_modify  │
              │        │     │  approve  │─────────────────┘
              │        │     │  (HITL)   │
              │        │     └────┬──────┘
              │        │          │ user_approved
              │        │          │
              │        │     ┌────▼──────┐
              │        │     │  execute  │◄────────┐
              │        │     └────┬──────┘         │
              │        │          │                │
              │        │     ┌────▼──────┐  retry  │
              │        │     │  verify   │─────────┘
              │        │     └────┬──────┘
              │        │          │ all_done
              │        │          │
              │        │     ┌────▼───────┐
              │        │     │ summarize  │
              │        │     └────┬───────┘
              │        │          │
     cancel   │   cancel│    ┌────▼────┐
              └────────┴────►│  END   │
                            └─────────┘
```

---

## 四、节点实现详解

### 4.0 Classify 节点 — 复杂度分类 (v2.0 新增)

```python
# agent_bridge.py

import re

# 规则匹配，零延迟，不调用 LLM
COMPLEX_TASK_PATTERNS = [
    r"做一个.+游戏",
    r"创建完整.+场景",
    r"实现.+功能",
    r"(make|create|build)\s+a\s+.+(game|level|scene|system)",
    r"(完整|完全|全部)(实现|创建|搭建)",
    r"(设计|构建).+(关卡|世界|环境)",
]

def classify_node(state: AgentState) -> dict:
    """规则匹配分类，不调用 LLM，零延迟。
    
    v2.0 优化: 取代 v1.0 的 should_use_agent() LLM 分类，
    避免每条消息额外 500ms-2s 的 API 往返。
    """
    goal = state["user_goal"]

    # 规则匹配: 包含复杂任务关键词 → full 模式
    for pattern in COMPLEX_TASK_PATTERNS:
        if re.search(pattern, goal, re.IGNORECASE):
            return {"execution_mode": "full"}

    # 用户通过 UI 显式选择 Agent 模式
    if state.get("force_agent_mode", False):
        return {"execution_mode": "full"}

    # 默认: 简单请求 → 直接执行
    return {"execution_mode": "direct"}


def route_by_complexity(state: AgentState) -> str:
    if state["execution_mode"] == "full":
        return "checkpoint"
    return "direct"
```

### 4.0b Direct 节点 — 简单请求直接执行 (取代原 AI Chat)

```python
# agent_bridge.py

async def direct_node(state: AgentState) -> dict:
    """简单请求直接执行，等同于原 ai_chat_bridge.py 的行为。
    
    流程: LLM 生成回复 → 如需工具调用 → 通过 JSON-lines 发给 C++ 执行 → 返回结果。
    不经过 plan/approve/verify，响应速度与原 Chat 一致。
    """
    llm = ChatOpenAI(
        model=os.environ.get("OPENAI_MODEL", "gpt-4o"),
        base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
        api_key=os.environ.get("OPENAI_API_KEY", ""),
        temperature=0.7,
        streaming=True,
    )

    tools = get_tool_definitions()
    llm_with_tools = llm.bind_tools(tools)

    messages = state.get("messages", [])
    messages.append(HumanMessage(content=state["user_goal"]))

    total_tokens = 0

    # 与原 ai_chat_bridge.py 一致的工具调用循环
    for _ in range(10):  # max tool call rounds
        response = await llm_with_tools.ainvoke(messages)
        total_tokens += response.usage_metadata.get("total_tokens", 0)
        messages.append(response)

        if not response.tool_calls:
            # 流式输出文本回复
            emit({"type": "content", "text": response.content})
            break

        for tc in response.tool_calls:
            # 通过 JSON-lines 发给 C++ 执行 (同进程直调, 不走 MCP)
            result = await call_tool_via_jsonlines(tc["name"], tc["args"])
            messages.append({
                "role": "tool",
                "tool_call_id": tc["id"],
                "content": json.dumps(result, ensure_ascii=False),
            })

    return {
        "messages": messages,
        "total_tokens": state.get("total_tokens", 0) + total_tokens,
    }
```

### 4.0c Checkpoint 节点 — 场景快照 (v2.0 新增)

```python
# agent_orchestrator.py

async def checkpoint_node(state: AgentState) -> dict:
    """在 Agent 正式执行前保存场景快照，用于一键回滚。
    
    v2.0 优化: 解决 "Agent 执行 24 次工具调用后需要 undo 24 次" 的问题。
    用户点击 "回滚" → dsengine_scene_load(checkpoint_path) → 恢复到 Agent 执行前状态。
    """
    checkpoint_path = "_agent_checkpoint_" + state.get("session_id", "default") + ".dscene"

    await call_tool_via_jsonlines("dsengine_scene_save", {"path": checkpoint_path})

    emit({
        "type": "agent_checkpoint_created",
        "path": checkpoint_path,
        "message": "场景快照已保存，可随时一键回滚",
    })

    return {"scene_checkpoint_path": checkpoint_path}
```

### 4.1 Plan 节点 — 任务分解

```python
# agent_planner.py

from langchain_openai import ChatOpenAI
from langchain_core.messages import SystemMessage, HumanMessage
import json

PLANNER_SYSTEM_PROMPT = """你是 DSEngine 游戏引擎的任务规划专家。

你的职责是将用户的高层目标分解为可执行的子任务列表。

## 可用的 Specialist Agent

| Agent ID | 擅长领域 | 可用工具 |
|----------|---------|---------|
| scene_architect | 场景搭建、实体创建、灯光布局、材质设置 | entity_create/modify/delete, material_create, scene_save |
| script_writer | Lua 脚本编写、游戏逻辑、控制器 | lua_execute, script_create |
| asset_manager | 资产导入、AI 纹理/模型/音效生成 | asset_import, asset_generate_texture/model/sfx |
| qa_tester | 运行测试、截图验证、问题发现 | editor_play/stop, editor_screenshot, scene_get_state |
| physics_tuner | 物理参数调优、碰撞器配置 | entity_modify(rigidbody/collider), lua_execute |

## 输出格式

输出 JSON 数组，每个任务包含:
- id: 唯一标识 (T1, T2, ...)
- title: 简短标题
- description: 详细描述 (包含具体参数: 位置、大小、颜色等)
- specialist: 分配给哪个 Agent
- dependencies: 前置任务 id 列表 (空列表 = 无依赖)
- estimated_tools: 预计使用的工具数量

## 规划原则

1. 任务粒度: 每个任务 1-5 个工具调用，不要太粗也不要太细
2. 依赖关系: 场景搭建 → 脚本编写 → 测试 (遵循自然顺序)
3. 并行性: 无依赖的任务可以标记为同级 (相同 dependencies)
4. 可验证: 每个任务完成后应该有可观察的结果 (截图可见 / 脚本可执行)
5. 渐进式: 先搭建最小可运行版本，再逐步增加功能
"""

async def plan_node(state: AgentState) -> dict:
    """将用户目标分解为子任务 DAG。"""
    llm = ChatOpenAI(
        model=os.environ.get("OPENAI_MODEL", "gpt-4o"),
        base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
        api_key=os.environ.get("OPENAI_API_KEY", ""),
        temperature=0.3,  # 规划需要更低温度以保证结构化输出
    )

    # 获取当前场景状态作为规划上下文 (通过 JSON-lines 同进程直调, 不走 MCP)
    scene_state = await call_tool_via_jsonlines("dsengine_scene_get_state", {})

    messages = [
        SystemMessage(content=PLANNER_SYSTEM_PROMPT),
        HumanMessage(content=f"""## 当前场景状态
{json.dumps(scene_state, ensure_ascii=False, indent=2)}

## 用户目标
{state['user_goal']}

## 用户约束
{chr(10).join(state.get('user_constraints', [])) or '无'}

## 用户修改意见 (如有)
{state.get('user_feedback', '无')}

请输出子任务 JSON 数组。"""),
    ]

    response = await llm.ainvoke(messages)

    # 解析 LLM 输出的 JSON
    task_plan = parse_task_plan(response.content)

    return {
        "task_plan": task_plan,
        "current_task_index": 0,
        "total_tokens": state.get("total_tokens", 0) + response.usage_metadata.get("total_tokens", 0),
    }
```

### 4.2 Approve 节点 — 人机审批

```python
# agent_orchestrator.py

async def approve_node(state: AgentState) -> dict:
    """向用户展示任务计划，等待审批。
    
    LangGraph 的 interrupt_before=["approve"] 会在此节点前暂停执行，
    将当前状态返回给 C++ 层展示。用户通过 UI 操作后，
    C++ 层调用 graph.update_state() 注入审批结果，恢复执行。
    """
    # 生成计划摘要发送给 C++ UI
    plan_summary = format_plan_for_display(state["task_plan"])

    emit({
        "type": "agent_plan",
        "plan": plan_summary,
        "task_count": len(state["task_plan"]),
        "estimated_tools": sum(t.get("estimated_tools", 3) for t in state["task_plan"]),
    })

    # 此处 LangGraph 自动暂停 (interrupt_before)
    # 用户审批后通过 graph.update_state() 注入:
    #   approval_status = "approved" | "rejected" | "modified"
    #   user_feedback = "..." (如果 modified)

    return state


def route_after_approve(state: AgentState) -> str:
    """根据用户审批结果决定下一步。"""
    status = state.get("approval_status", "pending")
    if status == "approved":
        return "execute"
    elif status == "modified":
        return "plan"      # 带着用户反馈重新规划
    else:  # rejected / pending
        return "end"
```

### 4.3 Execute 节点 — 子任务执行

```python
# agent_executor.py

from langchain_openai import ChatOpenAI
from langchain_core.tools import tool

SPECIALIST_PROMPTS = {
    "scene_architect": """你是 DSEngine 场景架构师。
你擅长创建游戏场景、布置实体、设置灯光和材质。
执行任务时遵循以下原则:
- 先获取场景状态 (dsengine_scene_get_state) 了解现有实体
- 创建实体时给出精确的 position/rotation/scale
- 创建完成后截图 (dsengine_editor_screenshot) 验证效果
- 所有操作可通过 undo 回滚""",

    "script_writer": """你是 DSEngine Lua 脚本专家。
你擅长编写游戏逻辑、玩家控制器、AI 行为等 Lua 脚本。
执行任务时遵循以下原则:
- 使用 dsengine_script_create 创建 .lua 文件 (自动热重载)
- 用 dsengine_lua_execute 测试代码片段
- 脚本要健壮: 检查 nil、使用 pcall 包装
- 遵循 DSEngine Lua API: dse.ecs / dse.input / dse.audio""",

    "asset_manager": """你是 DSEngine 资产管理专家。
你擅长导入资产、创建材质、使用 AI 生成纹理和模型。
执行任务时遵循以下原则:
- 生成纹理时提供详细的 prompt (风格、分辨率、无缝)
- 材质使用 PBR 工作流 (albedo/normal/roughness/metallic)
- 导入后验证资产是否正确加载""",

    "qa_tester": """你是 DSEngine QA 测试员。
你擅长运行游戏、截图对比、发现问题。
执行任务时遵循以下原则:
- 先截图记录当前状态
- 进入 Play 模式测试
- 截图记录测试结果
- 退出 Play 模式 (会恢复编辑器状态)
- 报告发现的问题""",

    "physics_tuner": """你是 DSEngine 物理调优专家。
你擅长配置刚体、碰撞器、物理参数。
执行任务时遵循以下原则:
- 先检查实体现有组件
- 设置合理的 mass / body_type
- 添加合适的碰撞器 (Box/Sphere)
- 进入 Play 模式测试物理效果""",
}


async def execute_node(state: AgentState) -> dict:
    """执行当前子任务。使用 LangGraph 内置的 ToolNode 调用 MCP 工具。"""
    idx = state["current_task_index"]
    task = state["task_plan"][idx]

    # 标记任务开始
    task["status"] = "running"
    emit({"type": "agent_task_status", "task_id": task["id"], "status": "running"})

    # 执行前快照 (通过 JSON-lines 同进程直调)
    scene_before = await call_tool_via_jsonlines("dsengine_scene_get_state", {})
    screenshot_before = await call_tool_via_jsonlines("dsengine_editor_screenshot", {"target": "scene"})

    # 获取 specialist 的 system prompt
    specialist = task.get("specialist", "scene_architect")
    system_prompt = SPECIALIST_PROMPTS.get(specialist, SPECIALIST_PROMPTS["scene_architect"])

    # 构建 LLM (带工具调用)
    llm = ChatOpenAI(
        model=os.environ.get("OPENAI_MODEL", "gpt-4o"),
        base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
        api_key=os.environ.get("OPENAI_API_KEY", ""),
        temperature=0.2,
    )

    # 绑定工具 (从 agent_safety.py 过滤后的工具定义)
    tools = get_safe_tool_definitions()
    llm_with_tools = llm.bind_tools(tools)

    # 构建上下文消息
    messages = [
        SystemMessage(content=system_prompt),
        HumanMessage(content=f"""## 当前任务
标题: {task['title']}
描述: {task['description']}

## 当前场景状态
{json.dumps(scene_before, ensure_ascii=False, indent=2)}

## 已完成的前置任务
{format_completed_tasks(state['task_plan'][:idx])}

请执行此任务。完成后用 dsengine_editor_screenshot 截图验证。"""),
    ]

    # ReAct 循环: LLM 思考 → 调用工具 → 观察结果 → 继续
    total_tokens = 0
    tools_used = []
    max_iterations = 15  # 防止无限循环

    for iteration in range(max_iterations):
        response = await llm_with_tools.ainvoke(messages)
        total_tokens += response.usage_metadata.get("total_tokens", 0)
        messages.append(response)

        # 没有工具调用 = 任务完成
        if not response.tool_calls:
            break

        # 执行所有工具调用
        for tc in response.tool_calls:
            tool_name = tc["name"]
            tool_args = tc["args"]
            tools_used.append(tool_name)

            emit({
                "type": "agent_tool_call",
                "task_id": task["id"],
                "tool": tool_name,
                "iteration": iteration,
            })

            # 安全检查 + 执行 (通过 JSON-lines 同进程直调, 不走 MCP)
            safety_check = agent_safety.check_tool_call(tool_name, tool_args)
            if not safety_check.allowed:
                result = {"error": f"Safety blocked: {safety_check.reason}"}
            else:
                result = await call_tool_via_jsonlines(tool_name, tool_args)

            messages.append({
                "role": "tool",
                "tool_call_id": tc["id"],
                "content": json.dumps(result, ensure_ascii=False),
            })

    # 执行后快照
    scene_after = await call_tool_via_jsonlines("dsengine_scene_get_state", {})
    screenshot_after = await call_tool_via_jsonlines("dsengine_editor_screenshot", {"target": "scene"})

    # 更新任务状态
    task["tools_used"] = tools_used
    task["tokens_used"] = total_tokens
    task["result"] = response.content if response.content else "Task completed"

    return {
        "task_plan": state["task_plan"],
        "scene_snapshot_before": json.dumps(scene_before),
        "scene_snapshot_after": json.dumps(scene_after),
        "screenshot_before": screenshot_before,
        "screenshot_after": screenshot_after,
        "total_tokens": state.get("total_tokens", 0) + total_tokens,
    }
```

### 4.4 Verify 节点 — 二级验证 (v2.0 优化)

```python
# agent_orchestrator.py

async def verify_node(state: AgentState) -> dict:
    """二级验证策略，减少 ~40% token 消耗。
    
    v2.0 优化: 不再每步都用 LLM 验证。
    
    Level 1 (确定性验证, 默认): 
      - 工具调用全部无报错 → 通过
      - 场景 diff 显示预期变化 (有新实体/修改) → 通过
      - 无需 LLM 调用, 零 token 消耗
      
    Level 2 (LLM 验证, 仅失败/可疑时触发):
      - Level 1 失败 (工具报错或场景无变化)
      - 用户开启 "严格验证" 模式
      - 任务类型为 qa_tester (需要主观判断)
    """
    idx = state["current_task_index"]
    task = state["task_plan"][idx]
    total_tokens = 0

    # ── Level 1: 确定性验证 ──
    tools_had_errors = any(
        "error" in t_name.lower() for t_name in task.get("tools_used", [])
    )
    scene_diff = compute_scene_diff(
        json.loads(state.get("scene_snapshot_before", "{}")),
        json.loads(state.get("scene_snapshot_after", "{}")),
    )
    scene_changed = scene_diff != "无变化"

    # 确定性通过: 工具无错 + 场景有变化
    if not tools_had_errors and scene_changed and task.get("specialist") != "qa_tester":
        task["status"] = "done"
        emit({"type": "agent_task_status", "task_id": task["id"],
              "status": "done", "result": task.get("result", ""),
              "verification": "deterministic"})
        return {
            "task_plan": state["task_plan"],
            "current_task_index": idx + 1,
        }

    # ── Level 2: LLM 验证 (仅在 Level 1 失败或 qa_tester 时) ──
    llm = ChatOpenAI(model=os.environ.get("OPENAI_MODEL", "gpt-4o"), temperature=0)

    verification_prompt = f"""验证以下任务是否成功完成:

任务: {task['title']}
描述: {task['description']}
场景变化: {scene_diff}
执行结果: {task.get('result', 'N/A')}
工具报错: {tools_had_errors}

输出 JSON: {{"completed": true/false, "reason": "...", "should_retry": true/false}}"""

    response = await llm.ainvoke([HumanMessage(content=verification_prompt)])
    total_tokens = response.usage_metadata.get("total_tokens", 0)
    verdict = parse_json(response.content)

    if verdict.get("completed", False):
        task["status"] = "done"
        emit({"type": "agent_task_status", "task_id": task["id"],
              "status": "done", "result": task.get("result", ""),
              "verification": "llm"})
        return {
            "task_plan": state["task_plan"],
            "current_task_index": idx + 1,
            "total_tokens": state.get("total_tokens", 0) + total_tokens,
        }
    else:
        task["retry_count"] = task.get("retry_count", 0) + 1
        if task["retry_count"] >= state.get("max_retries", 3):
            task["status"] = "failed"
            task["error"] = verdict.get("reason", "Unknown error")
            emit({"type": "agent_task_status", "task_id": task["id"],
                  "status": "failed", "error": task["error"]})
            return {
                "task_plan": state["task_plan"],
                "error_count": state.get("error_count", 0) + 1,
                "total_tokens": state.get("total_tokens", 0) + total_tokens,
            }
        else:
            emit({"type": "agent_task_status", "task_id": task["id"],
                  "status": "retrying",
                  "retry": task["retry_count"],
                  "reason": verdict.get("reason", "")})
            return {
                "task_plan": state["task_plan"],
                "total_tokens": state.get("total_tokens", 0) + total_tokens,
            }


def route_after_verify(state: AgentState) -> str:
    """根据验证结果路由。"""
    idx = state["current_task_index"]
    tasks = state["task_plan"]

    # 当前任务失败且重试用尽
    if idx < len(tasks) and tasks[idx].get("status") == "failed":
        return "approve"  # 请求用户介入

    # 当前任务需要重试
    if idx < len(tasks) and tasks[idx].get("status") == "running":
        return "execute"

    # 还有更多任务
    if idx < len(tasks):
        return "next_task"  # → execute (下一个任务)

    # 全部完成
    return "summarize"
```

### 4.5 Summarize 节点 — 最终报告

```python
# agent_orchestrator.py

async def summarize_node(state: AgentState) -> dict:
    """生成任务执行总结报告。"""
    tasks = state["task_plan"]
    done     = [t for t in tasks if t["status"] == "done"]
    failed   = [t for t in tasks if t["status"] == "failed"]
    skipped  = [t for t in tasks if t["status"] == "skipped"]

    report = {
        "type": "agent_complete",
        "summary": {
            "goal": state["user_goal"],
            "total_tasks": len(tasks),
            "completed": len(done),
            "failed": len(failed),
            "skipped": len(skipped),
            "total_tokens": state.get("total_tokens", 0),
            "total_duration_ms": state.get("total_duration_ms", 0),
            "tools_used": list(set(
                tool for t in tasks for tool in t.get("tools_used", [])
            )),
        },
        "tasks": [
            {
                "id": t["id"],
                "title": t["title"],
                "status": t["status"],
                "result": t.get("result", ""),
                "error": t.get("error", ""),
                "tokens": t.get("tokens_used", 0),
            }
            for t in tasks
        ],
    }

    # 最终截图
    final_screenshot = await call_tool_via_jsonlines(
        "dsengine_editor_screenshot", {"target": "scene"})
    report["final_screenshot"] = final_screenshot

    emit(report)
    return state
```

---

## 五、审计日志系统

### 5.1 数据模型

```python
# agent_audit.py

import sqlite3
import time
import json
from dataclasses import dataclass, field
from typing import Optional

@dataclass
class AuditEntry:
    session_id: str         # Agent 会话 ID
    timestamp: float        # Unix 时间戳
    event_type: str         # "plan" | "approve" | "execute" | "verify" | "tool_call" | "error"
    task_id: Optional[str]  # 关联的子任务 ID
    node_name: str          # LangGraph 节点名
    input_data: str         # JSON 序列化的输入
    output_data: str        # JSON 序列化的输出
    tokens_used: int
    duration_ms: float
    error: Optional[str]
    specialist: Optional[str]


class AuditLog:
    """SQLite 持久化的审计日志。"""

    def __init__(self, db_path: str = None):
        if db_path is None:
            db_path = os.path.join(project_dir, ".dse", "agent_audit.db")
        self.conn = sqlite3.connect(db_path)
        self._create_tables()

    def _create_tables(self):
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS audit_entries (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id  TEXT NOT NULL,
                timestamp   REAL NOT NULL,
                event_type  TEXT NOT NULL,
                task_id     TEXT,
                node_name   TEXT NOT NULL,
                input_data  TEXT,
                output_data TEXT,
                tokens_used INTEGER DEFAULT 0,
                duration_ms REAL DEFAULT 0,
                error       TEXT,
                specialist  TEXT
            )
        """)
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS sessions (
                id          TEXT PRIMARY KEY,
                user_goal   TEXT NOT NULL,
                started_at  REAL NOT NULL,
                ended_at    REAL,
                status      TEXT DEFAULT 'running',
                total_tokens INTEGER DEFAULT 0,
                task_count  INTEGER DEFAULT 0,
                completed   INTEGER DEFAULT 0,
                failed      INTEGER DEFAULT 0
            )
        """)
        self.conn.commit()

    def log(self, entry: AuditEntry):
        """记录一条审计日志。"""
        self.conn.execute("""
            INSERT INTO audit_entries
            (session_id, timestamp, event_type, task_id, node_name,
             input_data, output_data, tokens_used, duration_ms, error, specialist)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            entry.session_id, entry.timestamp, entry.event_type,
            entry.task_id, entry.node_name,
            entry.input_data, entry.output_data,
            entry.tokens_used, entry.duration_ms,
            entry.error, entry.specialist,
        ))
        self.conn.commit()

    def get_session_summary(self, session_id: str) -> dict:
        """获取会话执行摘要。"""
        rows = self.conn.execute("""
            SELECT event_type, COUNT(*), SUM(tokens_used), SUM(duration_ms)
            FROM audit_entries WHERE session_id = ?
            GROUP BY event_type
        """, (session_id,)).fetchall()

        return {
            "events": {r[0]: {"count": r[1], "tokens": r[2], "duration_ms": r[3]}
                       for r in rows},
            "total_entries": sum(r[1] for r in rows),
            "total_tokens": sum(r[2] for r in rows),
            "total_duration_ms": sum(r[3] for r in rows),
        }

    def get_task_trace(self, session_id: str, task_id: str) -> list[dict]:
        """获取某个子任务的完整执行轨迹。"""
        rows = self.conn.execute("""
            SELECT timestamp, event_type, node_name, input_data, output_data,
                   tokens_used, duration_ms, error
            FROM audit_entries
            WHERE session_id = ? AND task_id = ?
            ORDER BY timestamp
        """, (session_id, task_id)).fetchall()

        return [
            {
                "time": r[0], "event": r[1], "node": r[2],
                "input": r[3], "output": r[4],
                "tokens": r[5], "duration_ms": r[6], "error": r[7],
            }
            for r in rows
        ]
```

---

## 5b、安全沙箱 (v2.0 新增)

### 5b.1 AgentSafetyPolicy

```python
# agent_safety.py

from dataclasses import dataclass
from typing import Optional
import time

@dataclass
class SafetyCheckResult:
    allowed: bool
    reason: Optional[str] = None

class AgentSafetyPolicy:
    """Agent 模式下的安全策略，防止 LLM 幻觉导致破坏性操作。
    
    v2.0 新增: 解决原方案缺少安全边界的问题。
    """

    # Agent 模式下禁用的危险工具
    BLOCKED_TOOLS = {
        "dsengine_entity_batch_delete",  # 批量删除风险太高
    }

    # Lua 代码长度限制 (字符数)
    MAX_LUA_CODE_LENGTH = 2000

    # 工具调用频率限制
    MAX_CALLS_PER_SECOND = 5
    _call_timestamps: list[float] = []

    @classmethod
    def check_tool_call(cls, tool_name: str, args: dict) -> SafetyCheckResult:
        """在执行工具调用前进行安全检查。"""

        # 1. 工具黑名单
        if tool_name in cls.BLOCKED_TOOLS:
            return SafetyCheckResult(
                allowed=False,
                reason=f"Tool '{tool_name}' is blocked in Agent mode. "
                       f"Use individual entity_delete instead."
            )

        # 2. Lua 代码长度限制
        if tool_name == "dsengine_lua_execute":
            code = args.get("code", "")
            if len(code) > cls.MAX_LUA_CODE_LENGTH:
                return SafetyCheckResult(
                    allowed=False,
                    reason=f"Lua code too long ({len(code)} chars, "
                           f"max {cls.MAX_LUA_CODE_LENGTH}). "
                           f"Use dsengine_script_create for large scripts."
                )

        # 3. 频率限制
        now = time.time()
        cls._call_timestamps = [t for t in cls._call_timestamps if now - t < 1.0]
        if len(cls._call_timestamps) >= cls.MAX_CALLS_PER_SECOND:
            return SafetyCheckResult(
                allowed=False,
                reason=f"Rate limit: max {cls.MAX_CALLS_PER_SECOND} calls/second"
            )
        cls._call_timestamps.append(now)

        return SafetyCheckResult(allowed=True)


def get_safe_tool_definitions() -> list[dict]:
    """返回过滤后的工具定义列表 (排除黑名单工具)。"""
    all_tools = get_all_tool_definitions()
    return [t for t in all_tools if t["name"] not in AgentSafetyPolicy.BLOCKED_TOOLS]
```

---

## 六、协议扩展

### 6.1 Agent 专用消息类型

在现有 JSON-lines 协议基础上新增 Agent 相关消息：

```json
// ── Bridge → C++ (新增) ──

// Agent 任务规划完成，等待用户审批
{
    "type": "agent_plan",
    "plan": [
        {
            "id": "T1",
            "title": "创建基础场景",
            "description": "创建地面、天空盒、方向光",
            "specialist": "scene_architect",
            "dependencies": [],
            "estimated_tools": 4
        },
        // ...
    ],
    "task_count": 8,
    "estimated_tools": 25,
    "estimated_tokens": 15000
}

// 子任务状态变更
{
    "type": "agent_task_status",
    "task_id": "T1",
    "status": "running" | "done" | "failed" | "retrying" | "skipped",
    "result": "已创建 3 个实体: Floor, Skybox, DirectionalLight",
    "error": "",
    "retry": 0,
    "screenshot": "base64..."
}

// Agent 工具调用通知 (不需要 C++ 执行, 仅 UI 显示)
{
    "type": "agent_tool_call",
    "task_id": "T1",
    "tool": "dsengine_entity_create",
    "iteration": 0
}

// Agent 执行完成报告
{
    "type": "agent_complete",
    "summary": {
        "goal": "做一个平台跳跃游戏",
        "total_tasks": 8,
        "completed": 7,
        "failed": 1,
        "skipped": 0,
        "total_tokens": 23456,
        "total_duration_ms": 45000,
        "tools_used": ["dsengine_entity_create", "dsengine_script_create", ...]
    },
    "final_screenshot": "base64..."
}

// 场景快照已创建 (用于一键回滚)
{
    "type": "agent_checkpoint_created",
    "path": "_agent_checkpoint_xxx.dscene",
    "message": "场景快照已保存，可随时一键回滚"
}

// 安全策略拦截通知
{
    "type": "agent_safety_blocked",
    "tool": "dsengine_entity_batch_delete",
    "reason": "Tool blocked in Agent mode"
}

// ── C++ → Bridge ──

// 用户发送消息 (统一入口, classify 节点自动分流)
{
    "type": "user_message",
    "content": "做一个平台跳跃游戏",
    "force_agent_mode": false,
    "constraints": ["像素风格"]
}

// 用户审批 Agent 计划
{
    "type": "agent_approve",
    "status": "approved" | "rejected" | "modified",
    "feedback": "第三步改成蓝色天空"
}

// 用户一键回滚
{
    "type": "agent_rollback"
}
```

### 6.2 消息路由 (v2.0 统一入口)

```python
# agent_bridge.py (取代 ai_chat_bridge.py)

async def route_message(msg: dict):
    """统一消息入口 — 所有消息都走 Agent 状态机。
    
    v2.0: 取消了 should_use_agent() LLM 分类调用。
    改为 classify 节点的规则匹配 (零延迟)，在状态机内部分流。
    """
    msg_type = msg.get("type", "")

    if msg_type == "user_message":
        # 所有用户消息统一走 Agent 状态机
        # classify 节点会自动判断走 direct (简单) 还是 full (复杂)
        await run_agent_graph({
            "user_goal": msg["content"],
            "user_constraints": msg.get("constraints", []),
            "force_agent_mode": msg.get("force_agent_mode", False),
        })

    elif msg_type == "agent_approve":
        # 用户审批 → update_state 恢复状态机
        await handle_agent_approve(msg)

    elif msg_type == "agent_rollback":
        # 一键回滚 → 加载快照
        await handle_agent_rollback()
```

---

## 七、C++ 侧扩展

### 7.1 AgentPanel (统一面板，取代 ChatPanel)

```cpp
// editor_agent_panel.h

#pragma once

#include <string>
#include <vector>
#include <functional>

namespace dse::editor {

struct AgentTask {
    std::string id;
    std::string title;
    std::string description;
    std::string specialist;
    std::string status;         // "pending" | "running" | "done" | "failed"
    std::string result;
    std::string error;
    int retry_count = 0;
    int estimated_tools = 0;
    float progress = 0.0f;     // 0.0 ~ 1.0
    std::string screenshot;     // base64 完成截图
};

struct AgentSession {
    std::string goal;
    std::vector<AgentTask> tasks;
    int total_tokens = 0;
    float total_duration_ms = 0;
    std::string status;         // "idle" | "direct" | "planning" | "awaiting_approval" | "executing" | "done"
    std::string checkpoint_path; // 场景快照路径 (一键回滚用)
};

/// 编辑器统一 AI 面板 — 取代原 ChatPanel，同时支持对话和 Agent 任务。
/// 
/// 功能继承自 ChatPanel:
///   - 对话输入/输出 (流式 Markdown 渲染)
///   - @mention 上下文注入 (@scene/@entity/@selection)
///   - Token 统计、响应时间
///   - 对话历史持久化 (JSON)
///   - AI Provider 配置 (复用 AIConfigManager)
///
/// 新增 Agent 功能:
///   - 任务规划展示 + 审批按钮
///   - 子任务进度追踪
///   - 一键回滚 (场景快照)
///   - 审计日志查看器
class AgentPanel {
public:
    void Draw();

    // ── 对话功能 (继承自 ChatPanel) ──
    void OnStreamingContent(const std::string& chunk);
    void OnToolCallResult(const std::string& tool_name, const std::string& result);

    // ── Agent 功能 (新增) ──
    void OnAgentPlan(const std::vector<AgentTask>& tasks);
    void OnTaskStatusUpdate(const std::string& task_id, const std::string& status,
                            const std::string& result, const std::string& error);
    void OnCheckpointCreated(const std::string& path);
    void OnAgentComplete(int total_tokens, float duration_ms, const std::string& screenshot);

    // 用户操作
    void SetApproveCallback(std::function<void(const std::string& status,
                                               const std::string& feedback)> cb);
    void SetRollbackCallback(std::function<void()> cb);

    bool HasActiveSession() const { return session_.status != "idle"; }

private:
    void DrawChatArea();          // 对话区 (流式输出 + Markdown)
    void DrawInputBar();          // 输入栏 (含 @mention)
    void DrawTaskList();          // 任务列表 (Agent 模式)
    void DrawProgressBar();       // 进度条
    void DrawApprovalButtons();   // 审批按钮
    void DrawRollbackButton();    // 一键回滚
    void DrawAuditLog();          // 审计日志
    void DrawStatusBar();         // 底部状态 (token/耗时)

    AgentSession session_;
    std::vector<ChatMessage> messages_;  // 对话历史
    std::function<void(const std::string&, const std::string&)> approve_cb_;
    std::function<void()> rollback_cb_;
    bool show_audit_log_ = false;
    char input_buf_[1024] = {};
    char feedback_buf_[512] = {};
};

} // namespace dse::editor
```

### 7.2 AgentPanel UI 布局

```
┌─ Agent Panel ──────────────────────────────────────────────┐
│ 目标: 做一个平台跳跃游戏                                     │
│ 状态: 执行中 (3/8)  Token: 12,345  耗时: 23.4s              │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ [=============================>                 ] 38%│   │
│ └──────────────────────────────────────────────────────┘   │
│                                                            │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ 子任务列表                                            │   │
│ │ ✓ T1  创建基础场景 (scene_architect)         done    │   │
│ │ ✓ T2  创建玩家角色 (scene_architect)         done    │   │
│ │ ► T3  创建平台关卡 (scene_architect)         running │   │
│ │   T4  编写玩家控制器 (script_writer)         pending │   │
│ │   T5  编写相机跟随 (script_writer)           pending │   │
│ │   T6  添加收集物 (scene_architect)           pending │   │
│ │   T7  编写计分系统 (script_writer)           pending │   │
│ │   T8  测试运行 (qa_tester)                   pending │   │
│ └──────────────────────────────────────────────────────┘   │
│                                                            │
│ 当前: T3 - 创建平台关卡                                     │
│ 工具调用: dsengine_entity_create (iteration 2)              │
│                                                            │
│ [查看审计日志]  [暂停]  [一键回滚]  [取消]                      │
└────────────────────────────────────────────────────────────┘

// 审批模式 (awaiting_approval)
┌─ Agent Panel ──────────────────────────────────────────────┐
│ 目标: 做一个平台跳跃游戏                                     │
│ AI 已生成任务计划，请审批:                                    │
│                                                            │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ T1  创建基础场景 → scene_architect (3 tools)        │   │
│ │ T2  创建玩家角色 → scene_architect (2 tools)        │   │
│ │ T3  创建平台关卡 → scene_architect (5 tools)        │   │
│ │ T4  编写玩家控制器 → script_writer (2 tools)        │   │
│ │ T5  编写相机跟随 → script_writer (2 tools)          │   │
│ │ T6  添加收集物 → scene_architect (3 tools)          │   │
│ │ T7  编写计分系统 → script_writer (2 tools)          │   │
│ │ T8  测试运行 → qa_tester (4 tools)                  │   │
│ └──────────────────────────────────────────────────────┘   │
│ 预计: 23 次工具调用, ~15000 tokens                          │
│                                                            │
│ 修改意见: [                                            ]    │
│                                                            │
│ [✓ 批准执行]  [✎ 修改后重新规划]  [✗ 取消]                   │
└────────────────────────────────────────────────────────────┘
```

---

## 八、上下文管理策略

### 8.1 Token 预算管理

```python
# agent_context.py

class ContextManager:
    """管理 Agent 执行过程中的上下文窗口。"""

    def __init__(self, max_context_tokens: int = 128000):
        self.max_context_tokens = max_context_tokens
        self.reserve_tokens = 4096    # 为 LLM 输出预留

    def build_task_context(self, state: AgentState, task: TaskItem) -> list[dict]:
        """为当前子任务构建最优上下文。

        策略:
        1. System prompt (specialist) — 必须保留
        2. 当前任务描述 — 必须保留
        3. 场景状态快照 — 必须保留 (最新)
        4. 已完成任务的摘要 (压缩) — 按相关性选择
        5. 对话历史 — 滑动窗口 (最近 N 轮)
        """
        budget = self.max_context_tokens - self.reserve_tokens
        messages = []

        # 1. System prompt (~500 tokens, 必须)
        system_msg = build_system_message(task["specialist"])
        messages.append(system_msg)
        budget -= estimate_tokens(system_msg["content"])

        # 2. 当前任务描述 (~200 tokens, 必须)
        task_msg = build_task_message(task)
        budget -= estimate_tokens(task_msg["content"])

        # 3. 场景状态 (~500-2000 tokens, 必须)
        scene_msg = build_scene_context(state)
        budget -= estimate_tokens(scene_msg)

        # 4. 已完成任务摘要 (~50 tokens/task, 有预算时保留)
        completed_summary = summarize_completed_tasks(state["task_plan"])
        summary_tokens = estimate_tokens(completed_summary)
        if summary_tokens < budget * 0.3:
            task_msg["content"] += f"\n\n## 已完成任务\n{completed_summary}"
            budget -= summary_tokens

        messages.append({"role": "user", "content": task_msg["content"] + f"\n\n{scene_msg}"})

        return messages

    async def compress_history(self, messages: list[dict], max_tokens: int) -> list[dict]:
        """压缩对话历史到指定 token 数内。

        策略:
        - 保留最近 3 轮完整对话
        - 之前的对话用 LLM 生成摘要替代
        - 工具调用结果截断到 500 字符
        
        v2.0 修复: 改为 async def + await，
        v1.0 使用 asyncio.run() 在已有事件循环内会抛 RuntimeError。
        """
        if estimate_tokens_list(messages) <= max_tokens:
            return messages

        # 保留最近 3 轮
        recent = messages[-6:]
        older = messages[:-6]

        # 对 older 生成摘要 (v2.0: await 而非 asyncio.run)
        summary = await generate_summary(older)

        return [
            {"role": "system", "content": f"[之前的对话摘要]\n{summary}"},
            *recent,
        ]
```

### 8.2 场景 Diff 压缩

```python
def compute_scene_diff(before: dict, after: dict) -> str:
    """计算场景执行前后的差异，生成人类可读的 diff。
    
    目的: 减少传给 LLM 的 token 数 (完整场景可能几千 token，diff 通常几百)。
    """
    diff_lines = []

    before_entities = {e["name"]: e for e in before.get("entities", [])}
    after_entities  = {e["name"]: e for e in after.get("entities", [])}

    # 新增实体
    for name in after_entities:
        if name not in before_entities:
            e = after_entities[name]
            diff_lines.append(f"+ 新增实体 '{name}' "
                              f"pos={e.get('position', [0,0,0])} "
                              f"components={list(e.get('components', {}).keys())}")

    # 删除实体
    for name in before_entities:
        if name not in after_entities:
            diff_lines.append(f"- 删除实体 '{name}'")

    # 修改实体
    for name in after_entities:
        if name in before_entities:
            changes = diff_entity(before_entities[name], after_entities[name])
            if changes:
                diff_lines.append(f"~ 修改实体 '{name}': {changes}")

    return "\n".join(diff_lines) if diff_lines else "无变化"
```

---

## 九、错误恢复策略

### 9.1 错误分类与自动恢复

```python
class ErrorRecovery:
    """Agent 执行错误的分类和自动恢复策略。"""

    STRATEGIES = {
        # 工具调用失败
        "tool_timeout": {
            "max_retries": 2,
            "action": "retry_with_backoff",
            "backoff_seconds": [1, 3],
        },
        "tool_invalid_args": {
            "max_retries": 1,
            "action": "fix_args_and_retry",  # 让 LLM 修正参数
        },
        "entity_not_found": {
            "max_retries": 1,
            "action": "refresh_scene_state",  # 重新获取场景状态再试
        },
        "lua_syntax_error": {
            "max_retries": 2,
            "action": "fix_code_and_retry",  # 让 LLM 修正 Lua 代码
        },

        # 网络错误
        "api_rate_limit": {
            "max_retries": 3,
            "action": "retry_with_backoff",
            "backoff_seconds": [5, 15, 30],
        },
        "api_connection_error": {
            "max_retries": 3,
            "action": "retry_with_backoff",
            "backoff_seconds": [2, 5, 10],
        },

        # 逻辑错误
        "task_verification_failed": {
            "max_retries": 2,
            "action": "retry_with_feedback",  # 把失败原因作为上下文重试
        },
        "scene_state_unexpected": {
            "max_retries": 1,
            "action": "undo_and_retry",  # 撤销后重试
        },
    }

    @classmethod
    async def handle(cls, error_type: str, context: dict) -> dict:
        strategy = cls.STRATEGIES.get(error_type, {
            "max_retries": 0,
            "action": "escalate_to_user",
        })

        if context.get("retry_count", 0) >= strategy["max_retries"]:
            return {"action": "escalate_to_user", "error": context.get("error", "")}

        action = strategy["action"]

        if action == "retry_with_backoff":
            delay = strategy["backoff_seconds"][min(
                context.get("retry_count", 0),
                len(strategy["backoff_seconds"]) - 1
            )]
            await asyncio.sleep(delay)
            return {"action": "retry"}

        if action == "undo_and_retry":
            await call_tool_via_jsonlines("dsengine_editor_undo", {})
            return {"action": "retry"}

        if action == "refresh_scene_state":
            scene = await call_tool_via_jsonlines("dsengine_scene_get_state", {})
            return {"action": "retry", "updated_context": scene}

        return {"action": "escalate_to_user", "error": context.get("error", "")}
```

---

## 十、依赖与安装

### 10.1 Python 依赖

```
# tools/requirements_agent.txt

langgraph>=0.2.0                    # 核心: 状态机编排
langchain-openai>=0.2.0             # LLM 适配 (OpenAI/DeepSeek 兼容)
langchain-core>=0.3.0               # 基础抽象
langgraph-checkpoint-sqlite>=2.0.0  # 检查点持久化

# 已有依赖 (无需新增)
# openai>=1.0.0                     # 已在 requirements_ai.txt
# mcp                               # 已在 requirements_ai.txt
```

### 10.2 DeepSeek 接入配置

```python
# 只需修改环境变量，代码零改动:
# OPENAI_BASE_URL=https://api.deepseek.com/v1
# OPENAI_MODEL=deepseek-chat
# OPENAI_API_KEY=sk-...

# LangGraph 通过 langchain-openai 的 ChatOpenAI 自动适配:
llm = ChatOpenAI(
    model=os.environ.get("OPENAI_MODEL", "deepseek-chat"),
    base_url=os.environ.get("OPENAI_BASE_URL", "https://api.deepseek.com/v1"),
    api_key=os.environ.get("OPENAI_API_KEY", ""),
)
# DeepSeek 兼容 OpenAI 协议，tool_calling / streaming 均正常工作
```

---

## 十一、完整工作流示例

### 11.1 "做一个平台跳跃游戏"

```
用户: "做一个平台跳跃游戏，像素风格，有跳跃和收集金币功能"

==== Phase 0: 分类 + 快照 (classify + checkpoint 节点) ====

classify: 规则匹配命中 "做一个.+游戏" → execution_mode = "full"
checkpoint: 保存 _agent_checkpoint_abc123.dscene → 可一键回滚

==== Phase 1: 任务分解 (plan 节点) ====

AI Planner 分析后输出:

T1  创建基础场景          → scene_architect  deps=[]     ~3 tools
    - 创建地面 (Plane, green, scale=[20,1,20])
    - 添加方向光
    - 设置天空盒

T2  创建玩家角色          → scene_architect  deps=[T1]   ~2 tools
    - 创建胶囊体实体 (Capsule, blue, pos=[0,2,0])
    - 添加 Camera3D 组件

T3  创建平台关卡          → scene_architect  deps=[T1]   ~5 tools
    - 创建 5-8 个浮动平台 (Cube, various positions)
    - 从低到高渐进排列

T4  放置金币              → scene_architect  deps=[T3]   ~4 tools
    - 在每个平台上放置 Sphere (gold, small)

T5  编写玩家控制器        → script_writer   deps=[T2]   ~2 tools
    - WASD 移动 + 空格跳跃
    - 创建 player_controller.lua

T6  编写相机跟随          → script_writer   deps=[T2]   ~2 tools
    - 平滑跟随玩家位置
    - 创建 camera_follow.lua

T7  编写金币收集逻辑      → script_writer   deps=[T4,T5] ~2 tools
    - 碰撞检测 + 计分 + 金币消失
    - 创建 coin_collect.lua

T8  测试运行              → qa_tester       deps=[T5,T6,T7] ~4 tools
    - 进入 Play 模式
    - 截图验证
    - 退出 Play 模式

预计: 24 次工具调用, ~18000 tokens

==== Phase 2: 用户审批 (approve 节点, HITL 中断) ====

[C++ AgentPanel 显示任务列表]
用户点击 "批准执行"

==== Phase 3: 逐步执行 (execute → verify 循环) ====

T1 执行中...
  → dsengine_entity_create(name="Ground", mesh="plane", color=[0.3,0.8,0.3], scale=[20,1,20])
  → dsengine_entity_create(name="Sun", components=[{type:"DirectionalLight", ...}])
  → dsengine_editor_screenshot(target="scene")
T1 验证通过 ✓

T2 执行中...
  → dsengine_entity_create(name="Player", mesh="capsule", color=[0.2,0.4,1.0], pos=[0,2,0])
  → dsengine_entity_add_component(entity="Player", component={type:"Camera3D", fov:60})
T2 验证通过 ✓

T3 执行中...
  → dsengine_entity_create(name="Platform_1", pos=[3,3,0], scale=[3,0.5,3], color=[0.6,0.4,0.2])
  → dsengine_entity_create(name="Platform_2", pos=[-2,5,3], ...)
  → ... (5 个平台)
  → dsengine_editor_screenshot(target="scene")
T3 验证通过 ✓

... (T4-T7 类似)

T8 执行中...
  → dsengine_scene_get_state()  (确认所有实体就位)
  → dsengine_editor_play()
  → dsengine_editor_screenshot(target="game")
  → dsengine_editor_stop()
T8 验证通过 ✓

==== Phase 4: 完成报告 (summarize 节点) ====

{
    "goal": "做一个平台跳跃游戏",
    "completed": 8/8,
    "total_tokens": 16,234,
    "total_duration": 38.5s,
    "tools_used": 24,
    "final_screenshot": "base64..."
}
```

---

## 十二、与现有架构的关系 (v2.0 合并方案)

### 12.1 组件变更总览

| 组件 | 处理方式 | 说明 |
|------|---------|------|
| `editor_control_server.cpp` | **不改** | ControlServer + DispatchTool 是工具执行核心，Agent 和 MCP 都调用它 |
| `editor_control_tools.cpp` | **不改** | 23 个 Tool handler 不变 |
| `editor_ai_config.h/cpp` | **不改** | Provider/Key/代理配置直接被 Agent 复用 |
| `dsengine_mcp.py` | **不改** | 仅供外部 AI 客户端 (Cursor/Claude Desktop)，Agent 内部不走此路径 |
| `editor_chat_panel.h/cpp` | **Phase 2 后移除** | 被 `editor_agent_panel.h/cpp` 完整取代 |
| `editor_chat_protocol.h` | **Phase 2 后移除** | 协议合入 `editor_agent_protocol.h` |
| `ai_chat_bridge.py` | **Phase 1 后移除** | 被 `tools/agent/agent_bridge.py` 完整取代 |
| `CMakeLists.txt` | **修改** | 替换 chat_panel 源文件为 agent_panel |

### 12.2 新增组件

| 文件 | 语言 | 行数 | 说明 |
|------|------|------|------|
| `tools/agent/agent_bridge.py` | Python | ~350 | 统一入口 (取代 ai_chat_bridge.py) |
| `tools/agent/agent_orchestrator.py` | Python | ~300 | LangGraph 状态机 + 节点 |
| `tools/agent/agent_planner.py` | Python | ~200 | 任务分解 + specialist prompt |
| `tools/agent/agent_executor.py` | Python | ~250 | 子任务 ReAct 循环 |
| `tools/agent/agent_specialists.py` | Python | ~150 | 5 个 specialist 角色定义 |
| `tools/agent/agent_safety.py` | Python | ~80 | 安全沙箱 (工具黑名单/频率限制) |
| `tools/agent/agent_audit.py` | Python | ~100 | 审计日志 SQLite |
| `tools/agent/agent_context.py` | Python | ~150 | 上下文管理 + token 预算 |
| `tools/agent/agent_recovery.py` | Python | ~100 | 错误分类 + 恢复策略 |
| `tools/requirements_agent.txt` | text | ~10 | LangGraph 依赖 (pinned 版本) |
| `apps/editor_cpp/src/editor_agent_panel.h` | C++ | ~100 | 统一面板声明 |
| `apps/editor_cpp/src/editor_agent_panel.cpp` | C++ | ~450 | 统一面板实现 (对话+任务+审计) |
| `apps/editor_cpp/src/editor_agent_protocol.h` | C++ | ~60 | Agent 协议定义 |
| **合计** | | **~2300** | |

### 12.3 统一面板模式 (v2.0)

```
┌─ Agent Panel (统一, 取代 ChatPanel) ────────────────────────┐
│                                                              │
│  ┌─ 对话区 ──────────────────────────────────────────────┐  │
│  │ 用户: "创建一个红方块在 0,5,0"                          │  │
│  │ AI: 已创建实体 RedCube [pos=0,5,0]  (classify→direct)  │  │
│  │                                                        │  │
│  │ 用户: "做一个平台跳跃游戏"                              │  │
│  │ AI: 已生成 8 步任务计划，请审批    (classify→full)      │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─ 任务区 (仅 full 模式显示) ────────────────────────────┐  │
│  │ T1 创建基础场景   done    T5 玩家控制器  pending       │  │
│  │ T2 创建玩家角色   done    T6 相机跟随    pending       │  │
│  │ T3 创建平台关卡   running T7 计分系统    pending       │  │
│  │ T4 放置金币       pending T8 测试运行    pending       │  │
│  │ [=========>                              ] 25%          │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
│  输入: [________________________] [@scene] [发送]             │
│  Token: 12,345  耗时: 23.4s  [审计日志] [一键回滚] [取消]    │
└──────────────────────────────────────────────────────────────┘

统一面板根据 classify 节点结果自动切换:
- direct 模式: 只显示对话区 (与原 ChatPanel 体验一致)
- full 模式: 对话区 + 任务区 + 进度条 + 审批按钮 + 回滚按钮
```

---

## 十三、实施计划

### Phase 1: Agent 核心 + 直接模式 (4-5 天)

**目标: Agent 统一入口可用，简单请求走 direct 模式（等同于原 Chat 功能）**

- [ ] 安装 LangGraph + langchain-openai 依赖
- [ ] 实现 `agent_bridge.py` (统一入口 + classify 分类 + direct 直接执行)
- [ ] 实现 `agent_orchestrator.py` (StateGraph 状态机)
- [ ] 实现 `agent_planner.py` (任务分解)
- [ ] 实现 `agent_executor.py` (ReAct 执行循环)
- [ ] 实现 `agent_safety.py` (安全策略)
- [ ] 实现 `editor_agent_protocol.h` (协议定义)
- [ ] 端到端测试: direct 模式 "创建一个红方块"
- [ ] 端到端测试: full 模式 "创建一个包含 3 个实体的场景"

### Phase 2: 统一面板 + HITL + 审计 (3-4 天)

**目标: C++ 统一面板取代 ChatPanel，支持审批、回滚、审计**

- [ ] 实现 `editor_agent_panel.h/cpp` (统一面板: 对话 + 任务 + 审计)
- [ ] 实现 `agent_audit.py` (SQLite 日志)
- [ ] 实现 checkpoint 节点 + 一键回滚
- [ ] 集成 LangGraph `interrupt_before` HITL 审批
- [ ] 移除 `editor_chat_panel.h/cpp` + `ai_chat_bridge.py`
- [ ] 更新 `CMakeLists.txt`
- [ ] 测试: 用户修改计划后重新规划
- [ ] 测试: 一键回滚到 Agent 执行前

### Phase 3: 鲁棒性 + 上下文管理 (2-3 天)

- [ ] 实现 `agent_recovery.py` (错误恢复)
- [ ] 实现 `agent_context.py` (token 预算 + 上下文压缩)
- [ ] 实现二级验证 (确定性优先, LLM 可选)
- [ ] 检查点持久化 (`SqliteSaver` 到 `.dse/` 目录)
- [ ] 长任务测试: "做一个完整游戏 Demo"

### Phase 4: 高级功能 (可选)

- [ ] 子任务并行执行 (DAG 拓扑排序)
- [ ] 多 Agent 协作 (Handoff 机制)
- [ ] 审计日志可视化 UI
- [ ] Agent 模板 (预设常见任务规划)
- [ ] DeepSeek tool_calling 降级方案 (prompt-based JSON 输出解析)
- [ ] 用户自定义 Specialist Agent

---

## 十四、风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| DeepSeek tool_calling 兼容性不完美 | 中 | 高 | Phase 4 实现降级: prompt 中要求输出 JSON → 解析为 tool call |
| LLM 幻觉导致破坏性操作 | 中 | 高 | AgentSafetyPolicy 拦截 + 场景快照一键回滚 |
| 任务分解质量差 | 中 | 中 | HITL 审批 + 低温度 + few-shot 示例 |
| 长任务 token 超限 | 高 | 中 | 上下文管理器 + 滑动窗口 + 摘要压缩 |
| 子任务间状态不一致 | 低 | 高 | 每步后场景 diff 验证 + 确定性验证优先 |
| Agent 循环不收敛 | 中 | 中 | max_iterations 硬限制 + 重试上限 + 频率限制 |
| LangGraph 依赖链过重 | 低 | 低 | pin 版本 + venv 隔离；极端情况退化为 ~300 行自研状态机 |
| Python 子进程内存泄漏 | 低 | 中 | 定期重启 bridge + 内存监控 |

---

## 十五、总结

### 核心选型

**LangGraph** 作为 Agent 编排框架，因为：
- 状态机模型精确映射游戏开发工作流
- 内置检查点持久化 + Human-in-the-loop
- 与现有 ControlServer 零冲突集成
- LLM 无关（OpenAI/DeepSeek/Ollama 均支持）

### 架构原则 (v2.0)

1. **统一入口**: 一个面板、一个 Python bridge，classify 节点按复杂度自动分流
2. **同进程直调**: 工具调用走 JSON-lines → C++ DispatchTool（<1ms），不走 MCP（~100ms）
3. **安全优先**: AgentSafetyPolicy 拦截危险操作 + 场景快照一键回滚
4. **按需验证**: 确定性验证优先（零 token），LLM 验证仅在失败/可疑时触发
5. **人机协作**: 关键节点自动暂停等待审批，不是完全放飞
6. **可审计**: 每步执行完整记录到 SQLite，可回溯可追责
7. **渐进替换**: Phase 1 Agent 与 Chat 并存，Phase 2 完成后安全移除 Chat

### v2.0 优化总结

| # | 优化项 | 效果 |
|---|--------|------|
| 1 | 工具调用同进程直调 (取代 MCP) | 延迟从 ~100ms/次降到 <1ms/次 |
| 2 | 规则匹配分类 (取代 LLM 分类) | 每条消息延迟减少 500ms-2s |
| 3 | 二级验证策略 (确定性优先) | token 消耗减少 ~40% |
| 4 | 场景快照一键回滚 | 用户安全感，无需 undo 24 次 |
| 5 | 安全沙箱 (工具黑名单/频率限制) | 防止 LLM 幻觉破坏 |
| 6 | 依赖版本锁定 + venv 隔离 | 可维护性 |
| 7 | asyncio.run → await 修复 | 消除运行时崩溃 |
| 8 | Chat 合并进 Agent (统一面板) | 消除双系统维护成本 |

### 工作量估计

| 阶段 | 工作量 | 代码变更 |
|------|--------|---------|
| Phase 1 Agent 核心 + 直接模式 | 4-5 天 | ~1130 行 Python 新增 |
| Phase 2 统一面板 + HITL + 审计 | 3-4 天 | ~610 行 C++ 新增 + ~900 行旧 Chat 移除 |
| Phase 3 鲁棒性 | 2-3 天 | ~350 行 Python 新增 |
| **合计** | **9-12 天** | **~2300 行新增, ~900 行移除** |

---

## 十六、依赖管理 (v2.0 新增)

### 16.1 版本锁定

```
# tools/requirements_agent.txt
# 所有依赖 pin 精确版本，避免升级导致兼容问题
# 发布 >= 7 天的版本 (供应链安全)

langgraph==0.2.60
langchain-openai==0.2.14
langchain-core==0.3.30
langgraph-checkpoint-sqlite==2.0.6

# 已有依赖 (requirements_ai.txt 中, 不重复)
# openai>=1.0.0
# mcp
```

### 16.2 隔离安装

```bash
# 使用 venv 隔离，避免与用户系统 Python 冲突
python -m venv tools/agent/.venv
tools/agent/.venv/bin/pip install -r tools/requirements_agent.txt

# C++ 启动 bridge 时使用 venv 的 Python:
# tools/agent/.venv/bin/python tools/agent/agent_bridge.py
```

---

## 参考资源

- [LangGraph Documentation](https://langchain-ai.github.io/langgraph/)
- [LangGraph Human-in-the-loop](https://langchain-ai.github.io/langgraph/concepts/human_in_the_loop/)
- [LangGraph Checkpointing](https://langchain-ai.github.io/langgraph/concepts/persistence/)
- [OpenAI Function Calling](https://platform.openai.com/docs/guides/function-calling)
- [DeepSeek API Compatibility](https://platform.deepseek.com/api-docs/)
- [MCP Protocol Specification](https://modelcontextprotocol.io/)
