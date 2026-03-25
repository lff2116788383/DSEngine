const fs = require('fs');
const path = require('path');

function readText(filePath) {
  return fs.readFileSync(filePath, 'utf8');
}

function assertContains(text, token, label, failures) {
  if (!text.includes(token)) {
    failures.push(label);
  }
}

const root = path.resolve(__dirname, '..');
const editorAppPath = path.join(root, 'src', 'components', 'EditorApp.tsx');
const mainPath = path.join(root, 'main.js');
const preloadPath = path.join(root, 'preload.js');

const editorApp = readText(editorAppPath);
const main = readText(mainPath);
const preload = readText(preloadPath);

const failures = [];
assertContains(editorApp, 'handlePlayToggle', 'play_toggle', failures);
assertContains(editorApp, 'handleAddNode', 'create_entity', failures);
assertContains(editorApp, 'handleDeleteNode', 'delete_entity', failures);
assertContains(editorApp, 'handleApplyTexture', 'texture_apply', failures);
assertContains(editorApp, 'handleReplayMaterialHotUpdates', 'material_replay', failures);
assertContains(editorApp, 'pickEntity', 'viewport_picking', failures);
assertContains(preload, 'setRuntimePlay', 'runtime_play_ipc_preload', failures);
assertContains(main, "ipcMain.handle('engine:setRuntimePlay'", 'runtime_play_ipc_main', failures);

if (failures.length > 0) {
  console.error(`[EditorSmoke] FAIL missing=${failures.join(',')}`);
  process.exit(1);
}

console.log('[EditorSmoke] PASS checks=8');
