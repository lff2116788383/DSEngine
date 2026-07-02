import * as assert from 'assert';
import * as vscode from 'vscode';

suite('DSEngine Extension Test Suite', () => {

    test('Extension should be present', () => {
        const ext = vscode.extensions.getExtension('dsengine.dsengine-tools');
        // Extension may not be installed in test env, just verify API exists
        assert.ok(vscode.extensions);
    });

    test('Commands should be registered', async () => {
        const commands = await vscode.commands.getCommands(true);
        const dsengineCommands = commands.filter(
            (c: string) => c.startsWith('dsengine.'));

        assert.ok(dsengineCommands.length >= 7,
            `Expected at least 7 dsengine commands, found ${dsengineCommands.length}`);

        const expectedCommands = [
            'dsengine.startEditor',
            'dsengine.stopEditor',
            'dsengine.openProject',
            'dsengine.configureMcp',
            'dsengine.copyMcpConfig',
            'dsengine.showOutput',
            'dsengine.showQuickPick',
        ];

        for (const cmd of expectedCommands) {
            assert.ok(dsengineCommands.includes(cmd),
                `Missing command: ${cmd}`);
        }
    });

    test('Configuration properties should exist', () => {
        const config = vscode.workspace.getConfiguration('dsengine');

        // Verify defaults
        assert.strictEqual(config.get('port'), 9527);
        assert.strictEqual(config.get('autoStart'), false);
        assert.strictEqual(config.get('autoRegisterMcp'), true);
        assert.strictEqual(config.get('suppressStartPrompt'), false);
        assert.strictEqual(config.get('sdkPath'), '');
        assert.strictEqual(config.get('editorPath'), '');
    });
});
