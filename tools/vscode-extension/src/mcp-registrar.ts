import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { MCP_SERVER_NAME } from './constants';

interface MCPServerEntry {
    command: string;
    args: string[];
    env?: Record<string, string>;
}

interface MCPConfig {
    mcpServers?: Record<string, MCPServerEntry>;
}

export class MCPRegistrar {

    /**
     * Detect the current IDE variant.
     */
    detectIDE(): 'cursor' | 'trae' | 'windsurf' | 'vscode' {
        const appName = vscode.env.appName.toLowerCase();
        if (appName.includes('cursor')) { return 'cursor'; }
        if (appName.includes('trae')) { return 'trae'; }
        if (appName.includes('windsurf')) { return 'windsurf'; }
        return 'vscode';
    }

    /**
     * Ensure workspace has a valid MCP config pointing to dsengine_mcp.py.
     * Does NOT overwrite if a dsengine entry already exists.
     */
    async ensureConfig(
        sdkPath: string,
        context: vscode.ExtensionContext
    ): Promise<void> {
        const config = vscode.workspace.getConfiguration('dsengine');
        if (!config.get<boolean>('autoRegisterMcp')) {
            return;
        }

        const wsFolder = vscode.workspace.workspaceFolders?.[0];
        if (!wsFolder) { return; }

        const ide = this.detectIDE();
        const configDir = this.getConfigDir(wsFolder.uri.fsPath, ide);
        const configFile = path.join(configDir, 'mcp.json');

        if (this.hasExistingConfig(configFile)) {
            return;
        }

        const mcpPyPath = path.join(sdkPath, 'tools', 'mcp_adapter', 'dsengine_mcp.py');
        if (!fs.existsSync(mcpPyPath)) {
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

    /**
     * Copy a ready-to-paste MCP config JSON to the clipboard.
     */
    async copyConfigToClipboard(mcpPyPath: string): Promise<void> {
        const config: MCPConfig = {
            mcpServers: {
                [MCP_SERVER_NAME]: {
                    command: 'python',
                    args: [mcpPyPath.replace(/\\/g, '/')],
                    env: {},
                },
            },
        };
        await vscode.env.clipboard.writeText(JSON.stringify(config, null, 4));
        vscode.window.showInformationMessage('MCP config copied to clipboard.');
    }

    private getConfigDir(wsRoot: string, ide: string): string {
        switch (ide) {
            case 'cursor':   return path.join(wsRoot, '.cursor');
            case 'trae':     return path.join(wsRoot, '.trae');
            case 'windsurf': return path.join(wsRoot, '.windsurf');
            default:         return path.join(wsRoot, '.vscode');
        }
    }

    private hasExistingConfig(configFile: string): boolean {
        if (!fs.existsSync(configFile)) { return false; }
        try {
            const content = JSON.parse(fs.readFileSync(configFile, 'utf-8')) as MCPConfig;
            return !!(content?.mcpServers?.[MCP_SERVER_NAME]);
        } catch {
            return false;
        }
    }

    private async writeConfig(configFile: string, mcpPyPath: string): Promise<void> {
        const dir = path.dirname(configFile);
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }

        let existing: MCPConfig = {};
        if (fs.existsSync(configFile)) {
            try {
                existing = JSON.parse(fs.readFileSync(configFile, 'utf-8')) as MCPConfig;
            } catch {
                existing = {};
            }
        }

        if (!existing.mcpServers) {
            existing.mcpServers = {};
        }

        existing.mcpServers[MCP_SERVER_NAME] = {
            command: 'python',
            args: [mcpPyPath.replace(/\\/g, '/')],
            env: {},
        };

        fs.writeFileSync(configFile, JSON.stringify(existing, null, 4), 'utf-8');
    }
}
