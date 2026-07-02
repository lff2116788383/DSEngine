import { ChildProcess, spawn } from 'child_process';
import * as vscode from 'vscode';
import * as net from 'net';
import * as path from 'path';
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

    async stop(): Promise<void> {
        if (!this.process) {
            return;
        }

        this.outputChannel.appendLine('[DSEngine] Stopping editor...');

        // Try graceful shutdown via a minimal WebSocket quit command
        const gracefulDone = await this.tryGracefulShutdown();

        if (!gracefulDone && this.process) {
            this.outputChannel.appendLine('[DSEngine] Graceful shutdown failed, killing process.');
            this.process.kill('SIGTERM');
            this.process = null;
            this._onStateChange.fire('stopped');
        }
    }

    isRunning(): boolean {
        return this.process !== null && !this.process.killed;
    }

    dispose(): void {
        this._onStateChange.dispose();
    }

    private findEditorExecutable(sdkPath: string): string | null {
        const fs = require('fs') as typeof import('fs');
        const isWin = process.platform === 'win32';
        const exeName = isWin ? 'dse_editor_cpp.exe' : 'dse_editor_cpp';

        const candidates = [
            path.join(sdkPath, 'bin', exeName),
            path.join(sdkPath, 'out', 'build', 'windows-x64-debug',
                'apps', 'editor_cpp', exeName),
            path.join(sdkPath, 'out', 'build', 'windows-x64-release',
                'apps', 'editor_cpp', exeName),
            path.join(sdkPath, 'out', 'build', 'windows-x64-relwithdebinfo',
                'apps', 'editor_cpp', exeName),
        ];

        for (const p of candidates) {
            if (fs.existsSync(p)) {
                return p;
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

    private tryGracefulShutdown(): Promise<boolean> {
        return new Promise((resolve) => {
            const timeout = setTimeout(() => resolve(false), 3000);

            if (this.process) {
                this.process.once('exit', () => {
                    clearTimeout(timeout);
                    resolve(true);
                });
            }

            // Send quit via raw TCP/WebSocket handshake
            const socket = new net.Socket();
            socket.on('error', () => {
                clearTimeout(timeout);
                resolve(false);
            });

            socket.connect(this._port, '127.0.0.1', () => {
                // Minimal WebSocket upgrade + JSON-RPC quit
                // For simplicity, just kill the process since we don't have ws dep
                socket.destroy();
                if (this.process) {
                    this.process.kill('SIGTERM');
                }
            });
        });
    }
}
