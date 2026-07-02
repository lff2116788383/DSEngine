import { ChildProcess, spawn } from 'child_process';
import * as vscode from 'vscode';
import * as net from 'net';
import * as path from 'path';
import * as fs from 'fs';
import { findSDKPath, getPort } from './config';
import { EDITOR_OUTPUT_CHANNEL } from './constants';

export class EditorProcessManager {
    private process: ChildProcess | null = null;
    private outputChannel: vscode.OutputChannel;
    private _port: number;
    private _onStateChange = new vscode.EventEmitter<'started' | 'stopped'>();
    readonly onStateChange = this._onStateChange.event;

    constructor() {
        this.outputChannel = vscode.window.createOutputChannel(EDITOR_OUTPUT_CHANNEL);
        this._port = getPort();
    }

    get port(): number {
        return this._port;
    }

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

        const editorExe = this.findEditorExecutable(sdkPath);
        if (!editorExe) {
            vscode.window.showErrorMessage(
                `Editor executable not found in ${sdkPath}. Build the project first.`);
            return false;
        }

        this._port = await this.resolvePort(getPort());

        const args: string[] = ['--api-port', String(this._port)];
        if (dseprojPath) {
            args.push('--project', dseprojPath);
        }

        this.outputChannel.appendLine(`[DSEngine] Starting: ${editorExe} ${args.join(' ')}`);
        this.outputChannel.show(true);

        this.process = spawn(editorExe, args, {
            cwd: sdkPath,
            stdio: ['ignore', 'pipe', 'pipe'],
            detached: false,
        });

        this.process.stdout?.on('data', (data: Buffer) => {
            this.outputChannel.append(data.toString());
        });
        this.process.stderr?.on('data', (data: Buffer) => {
            this.outputChannel.append(data.toString());
        });

        this.process.on('exit', (code) => {
            this.outputChannel.appendLine(`[DSEngine] Editor exited with code ${code}`);
            this.process = null;
            this._onStateChange.fire('stopped');
        });

        this.process.on('error', (err) => {
            this.outputChannel.appendLine(`[DSEngine] Failed to start editor: ${err.message}`);
            this.process = null;
            this._onStateChange.fire('stopped');
        });

        this._onStateChange.fire('started');
        return true;
    }

    /**
     * Stop the editor process.
     * Since we own the child process, just send SIGTERM directly.
     * The editor handles SIGTERM gracefully (saves state, cleans up GPU resources).
     */
    stop(): void {
        if (!this.process) {
            return;
        }

        this.outputChannel.appendLine('[DSEngine] Stopping editor...');

        // Direct kill — we control the child process, no need for WS roundtrip.
        // On Windows SIGTERM is mapped to TerminateProcess; the editor's
        // atexit / destructor chain handles cleanup.
        this.process.kill('SIGTERM');

        // If still alive after 5s, force kill
        const proc = this.process;
        setTimeout(() => {
            if (proc && !proc.killed) {
                proc.kill('SIGKILL');
            }
        }, 5000);
    }

    isRunning(): boolean {
        return this.process !== null && !this.process.killed;
    }

    dispose(): void {
        this._onStateChange.dispose();
    }

    /**
     * Find the editor executable by scanning known build output directories.
     *
     * Strategy:
     * 1. User-configured path (dsengine.editorPath setting)
     * 2. SDK bin/ directory (installed SDK)
     * 3. CMake build output: out/build/<preset>/apps/editor_cpp/<exe>
     *    - Scans ALL preset directories, not just hardcoded names
     *    - Handles Debug/Release/RelWithDebInfo automatically
     * 4. Common Linux/macOS paths
     */
    private findEditorExecutable(sdkPath: string): string | null {
        const isWin = process.platform === 'win32';
        const exeName = isWin ? 'dse_editor_cpp.exe' : 'dse_editor_cpp';

        // 1. User-configured path
        const config = vscode.workspace.getConfiguration('dsengine');
        const userPath = config.get<string>('editorPath');
        if (userPath && fs.existsSync(userPath)) {
            return userPath;
        }

        // 2. SDK bin/ directory
        const binPath = path.join(sdkPath, 'bin', exeName);
        if (fs.existsSync(binPath)) {
            return binPath;
        }

        // 3. Scan CMake build output directories
        const buildRoot = path.join(sdkPath, 'out', 'build');
        if (fs.existsSync(buildRoot)) {
            try {
                const presetDirs = fs.readdirSync(buildRoot, { withFileTypes: true });
                for (const dir of presetDirs) {
                    if (!dir.isDirectory()) { continue; }
                    const candidate = path.join(
                        buildRoot, dir.name, 'apps', 'editor_cpp', exeName);
                    if (fs.existsSync(candidate)) {
                        return candidate;
                    }
                }
            } catch {
                // Permission error or similar — fall through
            }
        }

        // 4. Common Linux/macOS build paths
        if (!isWin) {
            const unixCandidates = [
                path.join(sdkPath, 'build', 'apps', 'editor_cpp', exeName),
                path.join(sdkPath, 'cmake-build-debug', 'apps', 'editor_cpp', exeName),
                path.join(sdkPath, 'cmake-build-release', 'apps', 'editor_cpp', exeName),
            ];
            for (const p of unixCandidates) {
                if (fs.existsSync(p)) { return p; }
            }
        }

        return null;
    }

    private async resolvePort(preferred: number): Promise<number> {
        for (let port = preferred; port < preferred + 10; port++) {
            const available = await this.isPortAvailable(port);
            if (available) {
                return port;
            }
        }
        return preferred;
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
}
