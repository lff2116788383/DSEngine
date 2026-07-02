import * as vscode from 'vscode';
import type { EditorProcessManager } from './editor-manager';

type EditorStatus = 'disconnected' | 'running' | 'error';

export class StatusBarManager {
    private item!: vscode.StatusBarItem;
    private pollTimer: ReturnType<typeof setInterval> | null = null;

    create(context: vscode.ExtensionContext): void {
        this.item = vscode.window.createStatusBarItem(
            vscode.StatusBarAlignment.Left, 50);
        this.item.command = 'dsengine.showQuickPick';
        this.update('disconnected');
        this.item.show();
        context.subscriptions.push(this.item);
    }

    update(status: EditorStatus, info?: string): void {
        switch (status) {
            case 'running':
                this.item.text = '$(plug) DSEngine: Running';
                if (info) { this.item.text += ` | ${info}`; }
                this.item.backgroundColor = undefined;
                this.item.tooltip = 'Click for DSEngine commands';
                break;
            case 'disconnected':
                this.item.text = '$(debug-disconnect) DSEngine: Stopped';
                this.item.backgroundColor = undefined;
                this.item.tooltip = 'Editor not running. Click to start.';
                break;
            case 'error':
                this.item.text = '$(error) DSEngine: Error';
                this.item.backgroundColor = new vscode.ThemeColor(
                    'statusBarItem.errorBackground');
                this.item.tooltip = info || 'Connection error';
                break;
        }
    }

    startPolling(manager: EditorProcessManager): void {
        this.pollTimer = setInterval(() => {
            if (manager.isRunning()) {
                this.update('running');
            } else {
                this.update('disconnected');
            }
        }, 5000);

        manager.onStateChange((state) => {
            this.update(state === 'started' ? 'running' : 'disconnected');
        });
    }

    dispose(): void {
        if (this.pollTimer) {
            clearInterval(this.pollTimer);
            this.pollTimer = null;
        }
    }
}
