const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

function getProjectRoot() {
  if (app.isPackaged) {
    const portableRoot = path.dirname(process.execPath);
    if (fs.existsSync(path.join(portableRoot, 'bin'))) {
      return portableRoot;
    }
  }
  return path.resolve(__dirname, '../..');
}

function resolveEngineVersions() {
  const projectRoot = getProjectRoot();
  const candidates = [
    { tag: 'debug', exe: path.join(projectRoot, 'bin', 'DSEngine_debug.exe') },
    { tag: 'release', exe: path.join(projectRoot, 'bin', 'DSEngine_release.exe') },
    { tag: 'default', exe: path.join(projectRoot, 'bin', 'DSEngine.exe') }
  ];
  return candidates.map((item) => ({
    tag: item.tag,
    executable: item.exe,
    available: fs.existsSync(item.exe)
  }));
}

function scanProjectDirectories(rootDir) {
  if (!rootDir || !fs.existsSync(rootDir)) {
    return [];
  }
  const entries = fs.readdirSync(rootDir, { withFileTypes: true });
  const result = [];
  for (const entry of entries) {
    if (!entry.isDirectory()) {
      continue;
    }
    const fullPath = path.join(rootDir, entry.name);
    const hasScene = fs.existsSync(path.join(fullPath, 'scenes'));
    const hasData = fs.existsSync(path.join(fullPath, 'data'));
    const hasConfig = fs.existsSync(path.join(fullPath, 'project.json')) || fs.existsSync(path.join(fullPath, 'project.dseproj'));
    if (hasScene || hasData || hasConfig) {
      result.push({
        name: entry.name,
        path: fullPath
      });
    }
  }
  return result;
}

function resolveEditorExecutable() {
  const projectRoot = getProjectRoot();
  const candidates = [
    path.join(projectRoot, 'bin', 'editor_pkg', 'win-unpacked', 'DSEngineEditor.exe'),
    path.join(projectRoot, 'bin', 'DSEngineEditor.exe')
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return '';
}

function spawnEditor(projectPath, versionTag) {
  const launchEnv = {
    ...process.env,
    DSE_LAUNCHER_PROJECT_PATH: projectPath || '',
    DSE_LAUNCHER_ENGINE_VERSION: versionTag || 'debug'
  };
  const editorExe = resolveEditorExecutable();
  if (editorExe) {
    const child = spawn(editorExe, [], {
      cwd: path.dirname(editorExe),
      detached: true,
      stdio: 'ignore',
      env: launchEnv
    });
    child.unref();
    return;
  }
  const editorDir = path.join(getProjectRoot(), 'apps', 'editor');
  const npmCmd = process.platform === 'win32' ? 'npm.cmd' : 'npm';
  const child = spawn(npmCmd, ['start'], {
    cwd: editorDir,
    detached: true,
    stdio: 'ignore',
    env: launchEnv
  });
  child.unref();
}

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 980,
    height: 660,
    autoHideMenuBar: true, // Hide the default menu bar
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });
  
  // Alternatively, you can completely remove the menu
  mainWindow.setMenu(null);
  
  mainWindow.loadFile('index.html');
}

ipcMain.handle('launcher:getEngineVersions', () => resolveEngineVersions());

ipcMain.handle('launcher:chooseProjectRoot', async () => {
  const result = await dialog.showOpenDialog({
    title: '选择项目根目录',
    properties: ['openDirectory']
  });
  if (result.canceled || result.filePaths.length === 0) {
    return '';
  }
  return result.filePaths[0];
});

ipcMain.handle('launcher:scanProjects', (event, rootDir) => scanProjectDirectories(rootDir));

ipcMain.handle('launcher:launchEditor', (event, projectPath, versionTag) => {
  spawnEditor(projectPath, versionTag);
  return { success: true };
});

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
