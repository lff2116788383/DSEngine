import * as vscode from 'vscode';
import { exec } from 'child_process';

interface CheckResult {
    ok: boolean;
    missing: string[];
    pythonCmd: string | null;
    pythonVersion?: string;
}

/**
 * Checks Python availability and required pip packages.
 *
 * Handles edge cases:
 * - Windows MS Store Python stub (returns exit code 9009)
 * - python vs python3 command name
 * - Missing pip
 * - Offline environments (--user flag, venv guidance)
 */
export class DependencyChecker {
    private _pythonCmd: string | null = null;

    get pythonCmd(): string | null {
        return this._pythonCmd;
    }

    async checkAll(): Promise<CheckResult> {
        const missing: string[] = [];

        // Resolve the correct python command
        this._pythonCmd = await this.findPythonCommand();
        if (!this._pythonCmd) {
            return { ok: false, missing: ['python'], pythonCmd: null };
        }

        const pyVer = await this.execCheck(`${this._pythonCmd} --version`);

        // Check websocket-client
        const wsOk = await this.execCheck(
            `${this._pythonCmd} -c "import websocket"`);
        if (!wsOk) {
            missing.push('websocket-client');
        }

        return {
            ok: missing.length === 0,
            missing,
            pythonCmd: this._pythonCmd,
            pythonVersion: pyVer || undefined,
        };
    }

    showInstallPrompt(missing: string[]): void {
        if (missing.includes('python')) {
            vscode.window.showErrorMessage(
                'DSEngine: Python 3.8+ not found. ' +
                'On Windows, install from python.org (NOT the Microsoft Store version). ' +
                'Ensure "Add Python to PATH" is checked during installation.',
                'Download Python',
                'Troubleshoot'
            ).then(choice => {
                if (choice === 'Download Python') {
                    vscode.env.openExternal(
                        vscode.Uri.parse('https://www.python.org/downloads/'));
                } else if (choice === 'Troubleshoot') {
                    vscode.env.openExternal(
                        vscode.Uri.parse(
                            'https://docs.python.org/3/using/windows.html'));
                }
            });
            return;
        }

        const pyCmd = this._pythonCmd || 'python';
        const msg = `DSEngine: missing Python package: ${missing.join(', ')}`;
        vscode.window.showWarningMessage(
            msg,
            'Install (--user)',
            'Install (venv)',
            'Manual'
        ).then(choice => {
            if (choice === 'Install (--user)') {
                const terminal = vscode.window.createTerminal('DSEngine Setup');
                terminal.show();
                terminal.sendText(
                    `${pyCmd} -m pip install --user ${missing.join(' ')}`);
            } else if (choice === 'Install (venv)') {
                const terminal = vscode.window.createTerminal('DSEngine Setup');
                terminal.show();
                const wsRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || '.';
                const isWin = process.platform === 'win32';
                const activateCmd = isWin
                    ? '.venv\\Scripts\\activate'
                    : 'source .venv/bin/activate';
                terminal.sendText(
                    `cd "${wsRoot}" && ` +
                    `${pyCmd} -m venv .venv && ` +
                    `${activateCmd} && ` +
                    `pip install ${missing.join(' ')}`);
            } else if (choice === 'Manual') {
                vscode.env.openExternal(
                    vscode.Uri.parse('https://pypi.org/project/websocket-client/'));
            }
        });
    }

    /**
     * Find a working Python command, filtering out MS Store stubs.
     * Priority: python3 > python > py (Windows launcher)
     */
    private async findPythonCommand(): Promise<string | null> {
        const candidates = process.platform === 'win32'
            ? ['python', 'python3', 'py -3']
            : ['python3', 'python'];

        for (const cmd of candidates) {
            const result = await this.execCheck(`${cmd} --version`);
            if (result && result.startsWith('Python 3')) {
                // Verify it's not the MS Store stub by checking it can import sys
                const canImport = await this.execCheck(`${cmd} -c "import sys"`);
                if (canImport !== null) {
                    return cmd;
                }
            }
        }
        return null;
    }

    private execCheck(cmd: string): Promise<string | null> {
        return new Promise((resolve) => {
            exec(cmd, { timeout: 10000 }, (err, stdout, stderr) => {
                if (err) {
                    resolve(null);
                } else {
                    resolve((stdout || stderr).trim());
                }
            });
        });
    }
}
