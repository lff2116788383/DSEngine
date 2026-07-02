import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { DEFAULT_PORT } from './constants';

/**
 * Find DSEngine SDK path by priority:
 * 1. User setting dsengine.sdkPath
 * 2. Workspace root contains engine/ + tools/mcp_adapter/dsengine_mcp.py
 * 3. Environment variable DSENGINE_SDK_PATH
 */
export function findSDKPath(): string | null {
    const config = vscode.workspace.getConfiguration('dsengine');
    const configured = config.get<string>('sdkPath');
    if (configured && fs.existsSync(configured)) {
        return configured;
    }

    const wsFolder = vscode.workspace.workspaceFolders?.[0];
    if (wsFolder) {
        const wsRoot = wsFolder.uri.fsPath;
        const mcpPy = path.join(wsRoot, 'tools', 'mcp_adapter', 'dsengine_mcp.py');
        if (fs.existsSync(path.join(wsRoot, 'engine')) && fs.existsSync(mcpPy)) {
            return wsRoot;
        }
    }

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
