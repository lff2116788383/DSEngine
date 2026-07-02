# DSEngine VS Code Extension — 完整设计方案

> **版本**: v1.0  
> **日期**: 2026-07-01  
> **状态**: 待实现  
> **位置**: `tools/vscode-extension/`（主仓子目录，非独立仓库）

---

## 1. 背景与目标

### 1.1 当前 MCP 架构

```
AI IDE (Cursor / Trae / VS Code + Copilot)
    ↓ MCP stdio protocol
dsengine_mcp.py (子进程, tools/mcp_adapter/)
    ↓ WebSocket (ws://127.0.0.1:9527)
DSEngine Editor (ControlServer, 49 个内建工具)
```

当前已能正常工作，但存在以下 onboarding 痛点：

| 痛点 | 影响 |
|------|------|
| 用户需手动在 IDE settings 里写 MCP 配置 JSON | 新手劝退率高 |
| 用户需自己先启动 DSEngine Editor | 流程断裂 |
| 无连接状态可视化 | 出错时不知道 MCP 是否在工作 |
| 需手动安装 Python 依赖（`websocket-client`） | 又一个劝退点 |
| 版本更新后需手动更新 MCP 配置路径 | 维护负担 |

### 1.2 目标

| 维度 | 目标 |
|------|------|
| **安装** | VS Code Marketplace 一键安装，零配置即可用 |
| **启动** | 打开含 `.dseproj` 的工作区 → 自动提示启动编辑器 |
| **可见** | 状态栏实时显示连接状态、场景名、实体数 |
| **兼容** | VS Code / Cursor / Trae / Windsurf 全兼容 |
| **零侵入** | 不修改引擎/编辑器任何代码 |

### 1.3 不做什么

- 不重新实现 MCP 协议（复用现有 `dsengine_mcp.py`）
- 不替代编辑器 UI（场景编辑仍在 DSEngine Editor 中）
- 不做代码补全/语法高亮（那是语言扩展的职责，可作为 v0.2）
- 不做 Lua debugger 集成（可作为 v0.3）

---

## 2. 方案自审：问题与优化

对初始方案做了严格审查，发现 **6 个问题**：

### 问题 1：状态栏轮询创建第二条 WebSocket 连接（资源浪费）

**初始方案**：扩展直接通过 WebSocket 连接 ControlServer 做健康检查和状态轮询。

**问题**：`dsengine_mcp.py` 已有一条 WebSocket 连接，扩展再开一条会：
- 浪费一个 ControlServer 连接槽
- 两个客户端同时操作可能产生竞态
- 增加扩展自身复杂度（需要自己实现 JSON-RPC 客户端）

**优化方案**：不直接连 WebSocket。改用两层策略：
1. **进程存活检测**：检查 `dse_editor_cpp` 进程是否存在（`process.pid` / `tasklist`）
2. **MCP 状态间接获取**：扩展启动 `dsengine_mcp.py` 子进程后，通过子进程 stderr 日志获取连接状态（`[MCP] WS connect failed` / `[MCP] WS connected`）
3. **按需主动查询**：仅在用户点击状态栏时，通过 MCP stdio 发一次 `dsengine_editor_get_state` 获取详细信息

### 问题 2：MCP 自动注册跨 IDE 格式脆弱（兼容性风险）

**初始方案**：扩展直接写入 `.cursor/mcp.json`、`.trae/mcp.json`、`.vscode/settings.json`。

**问题**：
- 这些配置文件格式可能随 IDE 版本变化
- 直接写入用户配置文件有覆盖风险
- Cursor 有全局 `~/.cursor/mcp.json` 和项目级 `.cursor/mcp.json` 两处

**优化方案**：
1. **主策略**：在工作区根目录生成 `.cursor/mcp.json`（项目级，不影响全局配置）
2. **备选策略**：提供 "Copy MCP Config" 命令，将配置 JSON 复制到剪贴板
3. **检测逻辑**：先检查是否已有 `dsengine` MCP 配置，有则不覆盖，只在缺失时提示用户

### 问题 3：编辑器自动启动过于激进（用户体验）

**初始方案**：`dsengine.autoStart: true` 打开工作区即启动编辑器。

**问题**：
- 编辑器是重量级 C++ 进程（GPU 渲染），自动启动可能意外消耗资源
- 用户可能只是浏览代码，不需要编辑器
- 多工作区场景下会启动多个编辑器实例

**优化方案**：
- `dsengine.autoStart` 默认 `false`
- 检测到 `.dseproj` 时显示非侵入性通知："DSEngine project detected. [Start Editor] [Not Now] [Don't Ask Again]"
- "Don't Ask Again" 写入工作区级别配置

### 问题 4：Python 依赖未管理（首次使用失败）

**初始方案**：假设用户已安装 `websocket-client`。

**问题**：新用户安装扩展后，如果没有 `websocket-client`，`dsengine_mcp.py` 会立即退出，用户只看到 MCP 不可用，不知道原因。

**优化方案**：
1. 扩展激活时检查 Python 可用性：`python --version`
2. 检查依赖：`python -c "import websocket"` 
3. 缺失时弹出通知："DSEngine MCP requires websocket-client. [Install Now] [Manual]"
4. "Install Now" 执行 `python -m pip install websocket-client` 并显示终端输出

### 问题 5：端口冲突无处理（多实例场景）

**初始方案**：硬编码端口 9527。

**问题**：
- 用户可能已有另一个 DSEngine Editor 占用 9527
- 多工作区同时打开多个项目需要不同端口

**优化方案**：
1. 启动前检查端口占用（`net.createServer` 尝试监听）
2. 若占用，检查是否是 DSEngine Editor（发 `dsengine_ping`）：
   - 是 → 提示 "Editor already running on port 9527. [Connect to it] [Use port 9528]"
   - 否 → 自动递增端口（9528, 9529...）
3. 选定端口传递给 `--api-port` 和 `dsengine_mcp.py --port`

### 问题 6：缺少 .dseproj 文件支持（低挂果实遗漏）

**初始方案**：仅用 `.dseproj` 做激活事件检测。

**问题**：`.dseproj` 是 JSON 格式但 VS Code 不识别，用户打开时看到纯文本。

**优化方案**：
1. 注册 `.dseproj` 文件关联为 `json` 语言
2. 提供 JSON Schema（`$schema` 验证 `ProjectDescriptor` 结构）
3. 在扩展的 `contributes.jsonValidation` 中注册 schema

---

## 3. 最终架构

```
VS Code / Cursor / Trae
│
├── DSEngine Tools Extension
│   │
│   ├── activate()
│   │   ├── 检测 .dseproj → 通知用户
│   │   ├── 检查 Python + websocket-client
│   │   ├── 注册命令
│   │   └── 创建状态栏
│   │
│   ├── EditorProcessManager
│   │   ├── start(projectPath, port)  → spawn dse_editor_cpp.exe
│   │   ├── stop()                    → dsengine_editor_quit via MCP
│   │   ├── isRunning()               → process alive check
│   │   └── onExit(callback)          → 进程退出回调
│   │
│   ├── MCPRegistrar
│   │   ├── ensureConfig(sdkPath)     → 写入 .cursor/mcp.json (if missing)
│   │   ├── copyConfigToClipboard()   → 备选方案
│   │   └── detectIDE()               → VS Code / Cursor / Trae
│   │
│   ├── StatusBarManager
│   │   ├── show(state)               → 图标 + 文字
│   │   ├── onClick()                 → 快速操作菜单
│   │   └── refresh()                 → 进程存活检测
│   │
│   └── DependencyChecker
│       ├── checkPython()             → python --version
│       ├── checkWebsocketClient()    → python -c "import websocket"
│       └── installDeps()             → pip install websocket-client
│
├── dsengine_mcp.py  ← 现有，不改
│   ↓ WebSocket
└── DSEngine Editor  ← 现有，不改
```

---

## 4. 文件结构

```
tools/vscode-extension/
├── package.json                 — 扩展清单
├── tsconfig.json                — TypeScript 配置
├── .vscodeignore                — 打包排除规则
├── schemas/
│   └── dseproj.schema.json      — .dseproj JSON Schema
├── resources/
│   └── icon.png                 — 扩展图标（128x128）
├── src/
│   ├── extension.ts             — activate() / deactivate() 入口
│   ├── editor-manager.ts        — EditorProcessManager
│   ├── mcp-registrar.ts         — MCP 配置自动注册
│   ├── status-bar.ts            — StatusBarManager
│   ├── commands.ts              — 命令注册与实现
│   ├── config.ts                — SDK 发现 + 配置读取
│   ├── dependency-checker.ts    — Python / pip 依赖检查
│   └── constants.ts             — 常量定义
└── README.md                    — Marketplace 页面内容
```

---

## 5. 核心模块设计

### 5.1 extension.ts — 入口

```typescript
import * as vscode from 'vscode';
import { EditorProcessManager } from './editor-manager';
import { MCPRegistrar } from './mcp-registrar';
import { StatusBarManager } from './status-bar';
import { registerCommands } from './commands';
import { DependencyChecker } from './dependency-checker';
import { findSDKPath } from './config';

let editorManager: EditorProcessManager;
let statusBar: StatusBarManager;

export async function activate(context: vscode.ExtensionContext) {
    editorManager = new EditorProcessManager();
    statusBar = new StatusBarManager();
    const mcpRegistrar = new MCPRegistrar();
    const depChecker = new DependencyChecker();

    // 1. 注册命令
    registerCommands(context, editorManager, mcpRegistrar, statusBar);

    // 2. 状态栏
    statusBar.create(context);

    // 3. 检查 Python 依赖（非阻塞）
    depChecker.checkAll().then(result => {
        if (!result.ok) {
            depChecker.showInstallPrompt(result.missing);
        }
    });

    // 4. 检测 .dseproj 并提示
    const dseprojs = await vscode.workspace.findFiles('**/*.dseproj', null, 1);
    if (dseprojs.length > 0) {
        const sdkPath = findSDKPath();
        if (sdkPath) {
            // 自动确保 MCP 配置存在
            await mcpRegistrar.ensureConfig(sdkPath, context);
        }

        // 提示启动编辑器（如果 autoStart 为 false）
        const config = vscode.workspace.getConfiguration('dsengine');
        if (config.get<boolean>('autoStart')) {
            editorManager.start(dseprojs[0].fsPath);
        } else if (!config.get<boolean>('suppressStartPrompt')) {
            showStartPrompt(dseprojs[0].fsPath);
        }
    }

    // 5. 启动状态栏轮询（仅检查进程存活，不建 WS 连接）
    statusBar.startPolling(editorManager);
}

export function deactivate() {
    statusBar?.dispose();
    // 注意：不自动关闭编辑器（用户可能还在用）
}

async function showStartPrompt(dseprojPath: string) {
    const choice = await vscode.window.showInformationMessage(
        'DSEngine project detected. Start the editor?',
        'Start Editor', 'Not Now', "Don't Ask Again"
    );
    if (choice === 'Start Editor') {
        editorManager.start(dseprojPath);
    } else if (choice === "Don't Ask Again") {
        const config = vscode.workspace.getConfiguration('dsengine');
        await config.update('suppressStartPrompt', true,
            vscode.ConfigurationTarget.Workspace);
    }
}
```

### 5.2 editor-manager.ts — 编辑器进程管理

```typescript
import { ChildProcess, spawn } from 'child_process';
import * as vscode from 'vscode';
import * as net from 'net';
import * as path from 'path';
import { findSDKPath, getPort } from './config';
import { DEFAULT_PORT } from './constants';

export class EditorProcessManager {
    private process: ChildProcess | null = null;
    private outputChannel: vscode.OutputChannel;
    private _port: number = DEFAULT_PORT;
    private _onStateChange = new vscode.EventEmitter<'started' | 'stopped'>();
    readonly onStateChange = this._onStateChange.event;

    constructor() {
        this.outputChannel = vscode.window.createOutputChannel('DSEngine Editor');
    }

    get port(): number { return this._port; }

    async start(dseprojPath?: string): Promise<boolean> {
        if (this.process) {
            vscode.window.showWarningMessage('DSEngine Editor is already running.');
            return false;
        }

        const sdkPath = findSDKPath();
        if (!sdkPath) {
            vscode.window.showErrorMessage(
                'DSEngine SDK not found. Set dsengine.sdkPath in settings.');
            return false;
        }

        // 查找可执行文件
        const editorExe = this.findEditorExecutable(sdkPath);
        if (!editorExe) {
            vscode.window.showErrorMessage(
                `Editor executable not found in ${sdkPath}`);
            return false;
        }

        // 端口分配
        this._port = await this.resolvePort(getPort());

        // 构建参数
        const args: string[] = ['--api-port', String(this._port)];
        if (dseprojPath) {
            args.push('--project', dseprojPath);
        }

        this.outputChannel.appendLine(
            `Starting: ${editorExe} ${args.join(' ')}`);
        this.outputChannel.show(true);

        this.process = spawn(editorExe, args, {
            cwd: sdkPath,
            stdio: ['ignore', 'pipe', 'pipe'],
            detached: false
        });

        this.process.stdout?.on('data', (data: Buffer) => {
            this.outputChannel.append(data.toString());
        });
        this.process.stderr?.on('data', (data: Buffer) => {
            this.outputChannel.append(data.toString());
        });

        this.process.on('exit', (code) => {
            this.outputChannel.appendLine(
                `Editor exited with code ${code}`);
            this.process = null;
            this._onStateChange.fire('stopped');
        });

        this._onStateChange.fire('started');
        return true;
    }

    async stop(): Promise<void> {
        if (!this.process) return;

        // 优雅关闭：通过 MCP 发送 quit 命令
        // 如果 3 秒内未退出，强制 kill
        const killTimer = setTimeout(() => {
            if (this.process) {
                this.process.kill('SIGTERM');
                this.process = null;
            }
        }, 3000);

        this.process.on('exit', () => clearTimeout(killTimer));

        // 尝试通过 WebSocket 发 quit
        try {
            const ws = new (await import('ws')).default(
                `ws://127.0.0.1:${this._port}`);
            ws.on('open', () => {
                ws.send(JSON.stringify({
                    jsonrpc: '2.0', id: 1,
                    method: 'dsengine_editor_quit', params: {}
                }));
                ws.close();
            });
            ws.on('error', () => {
                // WS 连接失败，直接 kill
                if (this.process) this.process.kill('SIGTERM');
            });
        } catch {
            if (this.process) this.process.kill('SIGTERM');
        }
    }

    isRunning(): boolean {
        return this.process !== null && !this.process.killed;
    }

    private findEditorExecutable(sdkPath: string): string | null {
        const candidates = [
            // Debug build
            path.join(sdkPath, 'bin', 'dse_editor_cpp.exe'),
            path.join(sdkPath, 'out', 'build', 'windows-x64-debug',
                      'apps', 'editor_cpp', 'dse_editor_cpp.exe'),
            // Release build
            path.join(sdkPath, 'out', 'build', 'windows-x64-release',
                      'apps', 'editor_cpp', 'dse_editor_cpp.exe'),
            // Linux / macOS
            path.join(sdkPath, 'bin', 'dse_editor_cpp'),
        ];

        const fs = require('fs');
        for (const p of candidates) {
            if (fs.existsSync(p)) return p;
        }
        return null;
    }

    private async resolvePort(preferred: number): Promise<number> {
        for (let port = preferred; port < preferred + 10; port++) {
            const available = await this.isPortAvailable(port);
            if (available) return port;
        }
        return preferred; // fallback
    }

    private isPortAvailable(port: number): Promise<boolean> {
        return new Promise((resolve) => {
            const server = net.createServer();
            server.once('error', () => resolve(false));
            server.once('listening', () => {
                server.close(() => resolve(true));
            });
            server.listen(port, '127.0.0.1');
        });
    }

    dispose() {
        this._onStateChange.dispose();
        // 不自动关闭编辑器
    }
}
```

### 5.3 mcp-registrar.ts — MCP 配置自动注册

```typescript
import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

interface MCPConfig {
    mcpServers: {
        [key: string]: {
            command: string;
            args: string[];
            env?: Record<string, string>;
        };
    };
}

export class MCPRegistrar {

    /**
     * 检测当前 IDE 类型
     */
    detectIDE(): 'cursor' | 'trae' | 'windsurf' | 'vscode' {
        const appName = vscode.env.appName.toLowerCase();
        if (appName.includes('cursor')) return 'cursor';
        if (appName.includes('trae')) return 'trae';
        if (appName.includes('windsurf')) return 'windsurf';
        return 'vscode';
    }

    /**
     * 确保工作区有正确的 MCP 配置
     */
    async ensureConfig(
        sdkPath: string,
        context: vscode.ExtensionContext
    ): Promise<void> {
        const wsFolder = vscode.workspace.workspaceFolders?.[0];
        if (!wsFolder) return;

        const ide = this.detectIDE();
        const configDir = this.getConfigDir(wsFolder.uri.fsPath, ide);
        const configFile = path.join(configDir, this.getConfigFileName(ide));

        // 已有 dsengine 配置 → 不覆盖
        if (this.hasExistingConfig(configFile)) {
            return;
        }

        // 生成配置
        const mcpPyPath = path.join(sdkPath,
            'tools', 'mcp_adapter', 'dsengine_mcp.py');
        if (!fs.existsSync(mcpPyPath)) {
            vscode.window.showWarningMessage(
                `dsengine_mcp.py not found at ${mcpPyPath}`);
            return;
        }

        const choice = await vscode.window.showInformationMessage(
            `Register DSEngine MCP for ${ide}?`,
            'Yes', 'Copy to Clipboard', 'No'
        );

        if (choice === 'Yes') {
            await this.writeConfig(configFile, mcpPyPath);
            vscode.window.showInformationMessage(
                'DSEngine MCP registered. Reload window to activate.');
        } else if (choice === 'Copy to Clipboard') {
            await this.copyConfigToClipboard(mcpPyPath);
        }
    }

    private getConfigDir(wsRoot: string, ide: string): string {
        switch (ide) {
            case 'cursor':   return path.join(wsRoot, '.cursor');
            case 'trae':     return path.join(wsRoot, '.trae');
            case 'windsurf': return path.join(wsRoot, '.windsurf');
            default:         return path.join(wsRoot, '.vscode');
        }
    }

    private getConfigFileName(ide: string): string {
        return 'mcp.json';
    }

    private hasExistingConfig(configFile: string): boolean {
        if (!fs.existsSync(configFile)) return false;
        try {
            const content = JSON.parse(
                fs.readFileSync(configFile, 'utf-8'));
            return !!(content?.mcpServers?.dsengine);
        } catch {
            return false;
        }
    }

    private async writeConfig(
        configFile: string, mcpPyPath: string
    ): Promise<void> {
        const dir = path.dirname(configFile);
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }

        let existing: MCPConfig = { mcpServers: {} };
        if (fs.existsSync(configFile)) {
            try {
                existing = JSON.parse(
                    fs.readFileSync(configFile, 'utf-8'));
            } catch { /* start fresh */ }
        }

        existing.mcpServers = existing.mcpServers || {};
        existing.mcpServers.dsengine = {
            command: 'python',
            args: [mcpPyPath.replace(/\\/g, '/')],
            env: {}
        };

        fs.writeFileSync(configFile,
            JSON.stringify(existing, null, 4), 'utf-8');
    }

    async copyConfigToClipboard(mcpPyPath: string): Promise<void> {
        const config: MCPConfig = {
            mcpServers: {
                dsengine: {
                    command: 'python',
                    args: [mcpPyPath.replace(/\\/g, '/')],
                    env: {}
                }
            }
        };
        await vscode.env.clipboard.writeText(
            JSON.stringify(config, null, 4));
        vscode.window.showInformationMessage(
            'MCP config copied to clipboard.');
    }
}
```

### 5.4 status-bar.ts — 状态栏

```typescript
import * as vscode from 'vscode';
import type { EditorProcessManager } from './editor-manager';

type EditorStatus = 'disconnected' | 'running' | 'error';

export class StatusBarManager {
    private item!: vscode.StatusBarItem;
    private pollTimer: NodeJS.Timer | null = null;

    create(context: vscode.ExtensionContext) {
        this.item = vscode.window.createStatusBarItem(
            vscode.StatusBarAlignment.Left, 50);
        this.item.command = 'dsengine.showQuickPick';
        this.update('disconnected');
        this.item.show();
        context.subscriptions.push(this.item);
    }

    update(status: EditorStatus, info?: string) {
        switch (status) {
            case 'running':
                this.item.text = `$(plug) DSEngine: Running`;
                if (info) this.item.text += ` | ${info}`;
                this.item.backgroundColor = undefined;
                this.item.tooltip = 'Click for DSEngine commands';
                break;
            case 'disconnected':
                this.item.text = '$(debug-disconnect) DSEngine: Stopped';
                this.item.backgroundColor = undefined;
                this.item.tooltip =
                    'Editor not running. Click to start.';
                break;
            case 'error':
                this.item.text = '$(error) DSEngine: Error';
                this.item.backgroundColor =
                    new vscode.ThemeColor(
                        'statusBarItem.errorBackground');
                this.item.tooltip = info || 'Connection error';
                break;
        }
    }

    startPolling(manager: EditorProcessManager) {
        // 轮询进程状态（不建 WebSocket 连接）
        this.pollTimer = setInterval(() => {
            if (manager.isRunning()) {
                this.update('running');
            } else {
                this.update('disconnected');
            }
        }, 5000);

        // 监听状态变化事件（即时响应）
        manager.onStateChange((state) => {
            this.update(
                state === 'started' ? 'running' : 'disconnected');
        });
    }

    dispose() {
        if (this.pollTimer) {
            clearInterval(this.pollTimer);
            this.pollTimer = null;
        }
    }
}
```

### 5.5 commands.ts — 命令注册

```typescript
import * as vscode from 'vscode';
import type { EditorProcessManager } from './editor-manager';
import type { MCPRegistrar } from './mcp-registrar';
import type { StatusBarManager } from './status-bar';
import { findSDKPath } from './config';

export function registerCommands(
    context: vscode.ExtensionContext,
    editor: EditorProcessManager,
    mcp: MCPRegistrar,
    statusBar: StatusBarManager
) {
    const register = (
        id: string, handler: (...args: any[]) => any
    ) => {
        context.subscriptions.push(
            vscode.commands.registerCommand(id, handler));
    };

    // 启动编辑器
    register('dsengine.startEditor', async () => {
        const dseprojs = await vscode.workspace.findFiles(
            '**/*.dseproj', null, 1);
        const projectPath = dseprojs.length > 0
            ? dseprojs[0].fsPath : undefined;
        await editor.start(projectPath);
    });

    // 停止编辑器
    register('dsengine.stopEditor', () => editor.stop());

    // 打开项目（文件对话框）
    register('dsengine.openProject', async () => {
        const result = await vscode.window.showOpenDialog({
            filters: { 'DSEngine Project': ['dseproj'] },
            canSelectMany: false
        });
        if (result?.[0]) {
            await editor.start(result[0].fsPath);
        }
    });

    // 配置 MCP
    register('dsengine.configureMcp', async () => {
        const sdkPath = findSDKPath();
        if (!sdkPath) {
            vscode.window.showErrorMessage(
                'Set dsengine.sdkPath first.');
            return;
        }
        await mcp.ensureConfig(sdkPath, context);
    });

    // 复制 MCP 配置到剪贴板
    register('dsengine.copyMcpConfig', async () => {
        const sdkPath = findSDKPath();
        if (!sdkPath) {
            vscode.window.showErrorMessage(
                'Set dsengine.sdkPath first.');
            return;
        }
        const mcpPyPath = require('path').join(sdkPath,
            'tools', 'mcp_adapter', 'dsengine_mcp.py');
        await mcp.copyConfigToClipboard(mcpPyPath);
    });

    // 显示编辑器输出
    register('dsengine.showOutput', () => {
        vscode.commands.executeCommand(
            'workbench.action.output.show.DSEngine Editor');
    });

    // 快速操作菜单（状态栏点击）
    register('dsengine.showQuickPick', async () => {
        const items: vscode.QuickPickItem[] = [];

        if (editor.isRunning()) {
            items.push(
                { label: '$(debug-stop) Stop Editor',
                  description: 'Stop DSEngine Editor' },
                { label: '$(output) Show Output',
                  description: 'View editor output log' },
            );
        } else {
            items.push(
                { label: '$(play) Start Editor',
                  description: 'Launch DSEngine Editor' },
            );
        }

        items.push(
            { label: '$(gear) Configure MCP',
              description: 'Set up MCP for AI IDE' },
            { label: '$(clippy) Copy MCP Config',
              description: 'Copy MCP config JSON to clipboard' },
        );

        const pick = await vscode.window.showQuickPick(items);
        if (!pick) return;

        if (pick.label.includes('Start'))
            vscode.commands.executeCommand('dsengine.startEditor');
        else if (pick.label.includes('Stop'))
            vscode.commands.executeCommand('dsengine.stopEditor');
        else if (pick.label.includes('Output'))
            vscode.commands.executeCommand('dsengine.showOutput');
        else if (pick.label.includes('Configure'))
            vscode.commands.executeCommand('dsengine.configureMcp');
        else if (pick.label.includes('Copy'))
            vscode.commands.executeCommand('dsengine.copyMcpConfig');
    });
}
```

### 5.6 config.ts — SDK 发现

```typescript
import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { DEFAULT_PORT } from './constants';

/**
 * 按优先级查找 DSEngine SDK 路径
 */
export function findSDKPath(): string | null {
    // 1. 用户设置
    const config = vscode.workspace.getConfiguration('dsengine');
    const configured = config.get<string>('sdkPath');
    if (configured && fs.existsSync(configured)) {
        return configured;
    }

    // 2. 工作区根目录有 CMakeLists.txt + engine/ 目录
    //    （开发者在引擎源码目录工作）
    const wsFolder = vscode.workspace.workspaceFolders?.[0];
    if (wsFolder) {
        const wsRoot = wsFolder.uri.fsPath;
        if (fs.existsSync(path.join(wsRoot, 'engine')) &&
            fs.existsSync(path.join(wsRoot, 'tools',
                'mcp_adapter', 'dsengine_mcp.py'))) {
            return wsRoot;
        }
    }

    // 3. 环境变量
    const envPath = process.env.DSENGINE_SDK_PATH;
    if (envPath && fs.existsSync(envPath)) {
        return envPath;
    }

    return null;
}

export function getPort(): number {
    const config = vscode.workspace.getConfiguration('dsengine');
    return config.get<number>('port') || DEFAULT_PORT;
}
```

### 5.7 dependency-checker.ts — 依赖检查

```typescript
import * as vscode from 'vscode';
import { exec } from 'child_process';

interface CheckResult {
    ok: boolean;
    missing: string[];
    pythonVersion?: string;
}

export class DependencyChecker {
    async checkAll(): Promise<CheckResult> {
        const missing: string[] = [];

        // 检查 Python
        const pyVer = await this.execCheck(
            'python --version');
        if (!pyVer) {
            return {
                ok: false,
                missing: ['python'],
            };
        }

        // 检查 websocket-client
        const wsOk = await this.execCheck(
            'python -c "import websocket"');
        if (!wsOk) missing.push('websocket-client');

        return {
            ok: missing.length === 0,
            missing,
            pythonVersion: pyVer,
        };
    }

    showInstallPrompt(missing: string[]) {
        const msg = `DSEngine: missing Python deps: ${
            missing.join(', ')}`;
        vscode.window.showWarningMessage(
            msg, 'Install Now', 'Manual'
        ).then(choice => {
            if (choice === 'Install Now') {
                const terminal =
                    vscode.window.createTerminal('DSEngine Setup');
                terminal.show();
                terminal.sendText(
                    `python -m pip install ${missing.join(' ')}`);
            } else if (choice === 'Manual') {
                vscode.env.openExternal(
                    vscode.Uri.parse(
                        'https://pypi.org/project/websocket-client/'));
            }
        });
    }

    private execCheck(cmd: string): Promise<string | null> {
        return new Promise((resolve) => {
            exec(cmd, { timeout: 5000 }, (err, stdout) => {
                resolve(err ? null : stdout.trim());
            });
        });
    }
}
```

### 5.8 constants.ts

```typescript
export const DEFAULT_PORT = 9527;
export const EXTENSION_ID = 'dsengine.dsengine-tools';
export const MCP_SERVER_NAME = 'dsengine';
```

---

## 6. package.json 完整定义

```json
{
    "name": "dsengine-tools",
    "displayName": "DSEngine Tools",
    "description": "AI-powered game development with DSEngine. Auto MCP integration for Cursor, Trae, and VS Code Copilot.",
    "version": "0.1.0",
    "publisher": "dsengine",
    "license": "MIT",
    "engines": {
        "vscode": "^1.85.0"
    },
    "categories": ["Other"],
    "keywords": ["dsengine", "game-engine", "mcp", "ai", "cursor", "trae"],
    "icon": "resources/icon.png",
    "repository": {
        "type": "git",
        "url": "https://github.com/user/DSEngine"
    },
    "activationEvents": [
        "workspaceContains:**/*.dseproj"
    ],
    "main": "./out/extension.js",
    "contributes": {
        "commands": [
            {
                "command": "dsengine.startEditor",
                "title": "DSEngine: Start Editor"
            },
            {
                "command": "dsengine.stopEditor",
                "title": "DSEngine: Stop Editor"
            },
            {
                "command": "dsengine.openProject",
                "title": "DSEngine: Open Project"
            },
            {
                "command": "dsengine.configureMcp",
                "title": "DSEngine: Configure MCP"
            },
            {
                "command": "dsengine.copyMcpConfig",
                "title": "DSEngine: Copy MCP Config"
            },
            {
                "command": "dsengine.showOutput",
                "title": "DSEngine: Show Editor Output"
            },
            {
                "command": "dsengine.showQuickPick",
                "title": "DSEngine: Quick Actions"
            }
        ],
        "configuration": {
            "title": "DSEngine",
            "properties": {
                "dsengine.sdkPath": {
                    "type": "string",
                    "default": "",
                    "description": "Path to DSEngine SDK root directory. Auto-detected if the workspace is an engine source tree."
                },
                "dsengine.port": {
                    "type": "number",
                    "default": 9527,
                    "description": "WebSocket port for editor Control Server."
                },
                "dsengine.autoStart": {
                    "type": "boolean",
                    "default": false,
                    "description": "Automatically start the editor when a .dseproj is detected."
                },
                "dsengine.autoRegisterMcp": {
                    "type": "boolean",
                    "default": true,
                    "description": "Automatically register MCP configuration when a project is detected."
                },
                "dsengine.suppressStartPrompt": {
                    "type": "boolean",
                    "default": false,
                    "description": "Suppress the 'Start editor?' notification."
                }
            }
        },
        "languages": [
            {
                "id": "json",
                "extensions": [".dseproj"],
                "aliases": ["DSEngine Project"]
            }
        ],
        "jsonValidation": [
            {
                "fileMatch": "*.dseproj",
                "url": "./schemas/dseproj.schema.json"
            }
        ]
    },
    "scripts": {
        "vscode:prepublish": "npm run compile",
        "compile": "tsc -p ./",
        "watch": "tsc -watch -p ./",
        "package": "vsce package"
    },
    "devDependencies": {
        "@types/node": "^20.0.0",
        "@types/vscode": "^1.85.0",
        "typescript": "^5.3.0",
        "@vscode/vsce": "^2.22.0"
    },
    "dependencies": {
        "ws": "^8.16.0"
    }
}
```

---

## 7. .dseproj JSON Schema

```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "DSEngine Project",
    "description": "DSEngine project descriptor (.dseproj)",
    "type": "object",
    "required": ["name"],
    "properties": {
        "format_version": {
            "type": "integer",
            "default": 1
        },
        "name": {
            "type": "string",
            "description": "Project name"
        },
        "version": {
            "type": "string",
            "default": "1.0.0"
        },
        "engine_version": {
            "type": "string"
        },
        "description": {
            "type": "string"
        },
        "features": {
            "type": "array",
            "items": { "type": "string" },
            "description": "Enabled features: lua_scripting, cpp_plugins, ..."
        },
        "entry_script": {
            "type": "string",
            "description": "Entry script path relative to project root"
        },
        "default_scene": {
            "type": "string",
            "default": "scenes/main.json"
        },
        "asset_dir": {
            "type": "string",
            "default": "assets/"
        },
        "scene_dir": {
            "type": "string",
            "default": "scenes/"
        },
        "script_dir": {
            "type": "string",
            "default": "scripts/"
        },
        "build": {
            "type": "object",
            "properties": {
                "output_dir": {
                    "type": "string",
                    "default": "build/"
                },
                "target": {
                    "type": "string",
                    "default": "standalone"
                }
            }
        }
    }
}
```

---

## 8. 与现有代码的关系

| 现有文件 | 扩展如何使用 | 是否修改 |
|----------|-------------|---------|
| `tools/mcp_adapter/dsengine_mcp.py` | 扩展将路径写入 MCP 配置，由 IDE 启动为子进程 | **不改** |
| `apps/editor_cpp/.../editor_control_server.*` | 编辑器进程管理的目标 | **不改** |
| `mcp_config.json` | 参考格式 | **不改** |
| `.cursor/mcp.json`, `.windsurf/mcp.json` | 扩展可能生成/更新这些文件 | 追加写入 |

**零侵入**：扩展不修改引擎、编辑器、MCP adapter 任何源代码。

---

## 9. 技术债分析

| # | 项目 | 类型 | 说明 | 处理 |
|---|------|------|------|------|
| 1 | `ws` npm 依赖 | 技术债 | 仅用于 `stop()` 时发一条 quit 命令，引入整个 `ws` 库不划算 | v0.2 改为 HTTP REST endpoint 或直接 `process.kill()` |
| 2 | 进程发现靠 `ChildProcess` 对象 | 限制 | 无法检测非扩展启动的编辑器实例 | v0.2 添加 PID 文件机制（编辑器启动时写 `.dse_editor.pid`） |
| 3 | 无 Lua 语法支持 | 功能缺口 | 对 DSEngine Lua 脚本开发者有价值 | v0.2 可以集成 `sumneko.lua` 扩展推荐 |
| 4 | 无 Scene TreeView | 功能缺口 | 在 VS Code 侧栏显示场景实体树会很方便 | v0.2 通过 MCP 定期拉取 `scene_get_state` |

所有技术债都是 **有意识的 v0.1 范围控制**，不影响核心功能。

---

## 10. 安全考量

| 风险 | 措施 |
|------|------|
| MCP 配置写入可能覆盖用户文件 | 先检查已有配置，有则不覆盖；写入前确认 |
| 编辑器进程以用户权限运行 | 与手动启动编辑器相同，不提权 |
| WebSocket 仅监听 127.0.0.1 | ControlServer 已限制本地访问 |
| API Key 通过环境变量传递 | 扩展不存储或显示 API Key |

---

## 11. 文件清单与代码量估算

| 文件 | 行数 | 说明 |
|------|------|------|
| `src/extension.ts` | ~70 | 入口 |
| `src/editor-manager.ts` | ~140 | 进程管理 |
| `src/mcp-registrar.ts` | ~120 | MCP 配置 |
| `src/status-bar.ts` | ~70 | 状态栏 |
| `src/commands.ts` | ~100 | 命令 |
| `src/config.ts` | ~50 | 配置 |
| `src/dependency-checker.ts` | ~60 | 依赖检查 |
| `src/constants.ts` | ~5 | 常量 |
| `package.json` | ~120 | 清单 |
| `tsconfig.json` | ~15 | TS 配置 |
| `schemas/dseproj.schema.json` | ~60 | JSON Schema |
| `.vscodeignore` | ~10 | 打包排除 |
| **总计** | **~820** | |

---

## 12. 工期估算

| 阶段 | 工作内容 | 时间 |
|------|---------|------|
| 1 | 脚手架（package.json, tsconfig, 目录结构） | 0.5 天 |
| 2 | 核心功能（extension.ts, config.ts, constants.ts） | 0.5 天 |
| 3 | 编辑器进程管理（editor-manager.ts） | 1 天 |
| 4 | MCP 注册 + 依赖检查 | 0.5 天 |
| 5 | 状态栏 + 命令 + Quick Pick | 0.5 天 |
| 6 | .dseproj Schema + 文件关联 | 0.5 天 |
| 7 | 测试 + 打包 (.vsix) | 0.5 天 |
| **总计** | | **4 天** |

---

## 13. 为什么不做独立仓库 / 子模块？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **子目录（推荐）** | 版本同步零成本、CI 集成简单、代码量小适合 | Marketplace 发布需额外 CI step |
| 独立仓库 | 独立版本号、独立 CI | 版本同步负担、跨仓修改、维护两套 CI |
| Git 子模块 | 介于两者之间 | 子模块管理痛苦、同步负担 |

**结论**：放在 `tools/vscode-extension/`，从主仓 CI 触发 `vsce package` 即可。

---

## 14. 未来路线图

| 版本 | 功能 |
|------|------|
| v0.1 | 本方案全部内容（进程管理 + MCP 注册 + 状态栏 + 命令 + .dseproj schema） |
| v0.2 | Scene TreeView 侧栏、Lua 语言推荐/配置、PID 文件机制 |
| v0.3 | Lua 调试器集成（DAP）、编辑器内截图预览 |
| v0.4 | 资产浏览器 TreeView、纹理/模型预览 |
| v1.0 | 完整的"VS Code 中做 DSEngine 游戏"体验 |
