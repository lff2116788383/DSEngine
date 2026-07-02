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

        const pyVer = await this.execCheck('python --version');
        if (!pyVer) {
            // Try python3 on Unix-like systems
            const py3Ver = await this.execCheck('python3 --version');
            if (!py3Ver) {
                return { ok: false, missing: ['python'] };
            }
        }

        const wsOk = await this.execCheck('python -c "import websocket"');
        if (!wsOk) {
            const ws3Ok = await this.execCheck('python3 -c "import websocket"');
            if (!ws3Ok) {
                missing.push('websocket-client');
            }
        }

        return {
            ok: missing.length === 0,
            missing,
            pythonVersion: pyVer || undefined,
        };
    }

    showInstallPrompt(missing: string[]): void {
        if (missing.includes('python')) {
            vscode.window.showErrorMessage(
                'DSEngine: Python not found. Install Python 3.8+ to use MCP tools.',
                'Download Python'
            ).then(choice => {
                if (choice === 'Download Python') {
                    vscode.env.openExternal(
                        vscode.Uri.parse('https://www.python.org/downloads/'));
                }
            });
            return;
        }

        const msg = `DSEngine: missing Python package: ${missing.join(', ')}`;
        vscode.window.showWarningMessage(msg, 'Install Now', 'Manual').then(choice => {
            if (choice === 'Install Now') {
                const terminal = vscode.window.createTerminal('DSEngine Setup');
                terminal.show();
                terminal.sendText(`python -m pip install ${missing.join(' ')}`);
            } else if (choice === 'Manual') {
                vscode.env.openExternal(
                    vscode.Uri.parse('https://pypi.org/project/websocket-client/'));
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
