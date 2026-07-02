import * as vscode from 'vscode';
import * as path from 'path';
import type { EditorProcessManager } from './editor-manager';
import type { MCPRegistrar } from './mcp-registrar';
import { findSDKPath } from './config';

export function registerCommands(
    context: vscode.ExtensionContext,
    editor: EditorProcessManager,
    mcp: MCPRegistrar
): void {
    const register = (id: string, handler: (...args: unknown[]) => unknown) => {
        context.subscriptions.push(vscode.commands.registerCommand(id, handler));
    };

    register('dsengine.startEditor', async () => {
        const dseprojs = await vscode.workspace.findFiles('**/*.dseproj', null, 1);
        const projectPath = dseprojs.length > 0 ? dseprojs[0].fsPath : undefined;
        await editor.start(projectPath);
    });

    register('dsengine.stopEditor', () => editor.stop());

    register('dsengine.openProject', async () => {
        const result = await vscode.window.showOpenDialog({
            filters: { 'DSEngine Project': ['dseproj'] },
            canSelectMany: false,
        });
        if (result?.[0]) {
            await editor.start(result[0].fsPath);
        }
    });

    register('dsengine.configureMcp', async () => {
        const sdkPath = findSDKPath();
        if (!sdkPath) {
            vscode.window.showErrorMessage('Set dsengine.sdkPath first.');
            return;
        }
        await mcp.ensureConfig(sdkPath, context);
    });

    register('dsengine.copyMcpConfig', async () => {
        const sdkPath = findSDKPath();
        if (!sdkPath) {
            vscode.window.showErrorMessage('Set dsengine.sdkPath first.');
            return;
        }
        const mcpPyPath = path.join(sdkPath, 'tools', 'mcp_adapter', 'dsengine_mcp.py');
        await mcp.copyConfigToClipboard(mcpPyPath);
    });

    register('dsengine.showOutput', () => {
        vscode.commands.executeCommand('workbench.action.output.toggleOutput');
    });

    register('dsengine.showQuickPick', async () => {
        const items: vscode.QuickPickItem[] = [];

        if (editor.isRunning()) {
            items.push(
                { label: '$(debug-stop) Stop Editor', description: 'Stop DSEngine Editor' },
                { label: '$(output) Show Output', description: 'View editor output log' },
            );
        } else {
            items.push(
                { label: '$(play) Start Editor', description: 'Launch DSEngine Editor' },
            );
        }

        items.push(
            { label: '$(gear) Configure MCP', description: 'Set up MCP for AI IDE' },
            { label: '$(clippy) Copy MCP Config', description: 'Copy MCP config JSON to clipboard' },
        );

        const pick = await vscode.window.showQuickPick(items);
        if (!pick) { return; }

        const label = pick.label;
        if (label.includes('Start')) {
            vscode.commands.executeCommand('dsengine.startEditor');
        } else if (label.includes('Stop')) {
            vscode.commands.executeCommand('dsengine.stopEditor');
        } else if (label.includes('Output')) {
            vscode.commands.executeCommand('dsengine.showOutput');
        } else if (label.includes('Configure')) {
            vscode.commands.executeCommand('dsengine.configureMcp');
        } else if (label.includes('Copy')) {
            vscode.commands.executeCommand('dsengine.copyMcpConfig');
        }
    });
}
