import * as vscode from 'vscode';
import { EditorProcessManager } from './editor-manager';
import { MCPRegistrar } from './mcp-registrar';
import { StatusBarManager } from './status-bar';
import { registerCommands } from './commands';
import { DependencyChecker } from './dependency-checker';
import { findSDKPath } from './config';

let editorManager: EditorProcessManager;
let statusBar: StatusBarManager;

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    editorManager = new EditorProcessManager();
    statusBar = new StatusBarManager();
    const mcpRegistrar = new MCPRegistrar();
    const depChecker = new DependencyChecker();

    // 1. Register commands
    registerCommands(context, editorManager, mcpRegistrar);

    // 2. Status bar
    statusBar.create(context);
    statusBar.startPolling(editorManager);

    // 3. Check Python dependencies (non-blocking)
    depChecker.checkAll().then(result => {
        if (!result.ok) {
            depChecker.showInstallPrompt(result.missing);
        }
    });

    // 4. Detect .dseproj and prompt
    const dseprojs = await vscode.workspace.findFiles('**/*.dseproj', null, 1);
    if (dseprojs.length > 0) {
        const sdkPath = findSDKPath();
        if (sdkPath) {
            await mcpRegistrar.ensureConfig(sdkPath, context);
        }

        const config = vscode.workspace.getConfiguration('dsengine');
        if (config.get<boolean>('autoStart')) {
            editorManager.start(dseprojs[0].fsPath);
        } else if (!config.get<boolean>('suppressStartPrompt')) {
            showStartPrompt(dseprojs[0].fsPath);
        }
    }
}

export function deactivate(): void {
    statusBar?.dispose();
    editorManager?.dispose();
}

async function showStartPrompt(dseprojPath: string): Promise<void> {
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
