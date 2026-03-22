const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const { spawn, exec } = require('child_process');

// Load the C++ Addon
const dsengine = require('./build/Release/dsengine_bridge.node');

let runtimeEngineProcess = null;
const runtimeSharedFrameName = 'DSEngineEditorSharedFrameV1';
process.env.DSE_EDITOR_FRAME_SHM_NAME = runtimeSharedFrameName;

function resolveRuntimeEngineExe() {
  const candidates = [
    path.join(__dirname, '..', 'bin', 'DSEngine_debug.exe'),
    path.join(__dirname, '..', 'bin', 'DSEngine.exe')
  ];
  for (const p of candidates) {
    if (fs.existsSync(p)) {
      return p;
    }
  }
  return null;
}

function startRuntimeFrameBridge() {
  const engineExe = resolveRuntimeEngineExe();
  if (!engineExe) {
    return;
  }
  if (runtimeEngineProcess) {
    return;
  }
  runtimeEngineProcess = spawn(engineExe, [], {
    cwd: path.join(__dirname, '..'),
    windowsHide: true,
    env: {
      ...process.env,
      DSE_EDITOR_FRAME_SHM_NAME: runtimeSharedFrameName
    }
  });
  process.env.DSE_EDITOR_FRAME_SHM_NAME = runtimeSharedFrameName;
  runtimeEngineProcess.on('exit', () => {
    runtimeEngineProcess = null;
  });
}

function stopRuntimeFrameBridge() {
  if (runtimeEngineProcess) {
    runtimeEngineProcess.kill();
    runtimeEngineProcess = null;
  }
}

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 1280,
    height: 720,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  // Setup IPC handlers to communicate with the C++ addon
  ipcMain.handle('engine:getVersion', () => {
    return dsengine.getVersion();
  });

  ipcMain.handle('engine:init', () => {
    return dsengine.initEngine();
  });

  ipcMain.handle('engine:getFrameBuffer', () => {
    return dsengine.getFrameBuffer();
  });

  ipcMain.handle('engine:getFrameInfo', () => {
    return dsengine.getFrameInfo();
  });

  ipcMain.handle('engine:pushExternalFrame', (event, frameBuffer, width, height) => {
    return dsengine.pushExternalFrame(frameBuffer, width, height);
  });

  ipcMain.handle('engine:clearExternalFrame', () => {
    return dsengine.clearExternalFrame();
  });

  ipcMain.handle('engine:getEntities', () => {
    return dsengine.getEntities();
  });

  ipcMain.handle('engine:updateEntityTransform', (event, id, x, y, z) => {
    return dsengine.updateEntityTransform(id, x, y, z);
  });

  ipcMain.handle('engine:tick', (event, deltaTime) => {
    return dsengine.tickEngine(deltaTime);
  });

  ipcMain.handle('engine:importTexture', (event, filePath) => {
    return dsengine.importTexture(filePath);
  });

  ipcMain.handle('engine:listImportedTextures', () => {
    return dsengine.listImportedTextures();
  });

  ipcMain.handle('engine:applyTextureToEntity', (event, entityId, textureHandle) => {
    return dsengine.applyTextureToEntity(entityId, textureHandle);
  });

  ipcMain.handle('engine:listShaderVariants', () => {
    return dsengine.listShaderVariants();
  });

  ipcMain.handle('engine:createMaterialInstance', (event, name, shaderVariant, textureHandle) => {
    return dsengine.createMaterialInstance(name, shaderVariant, textureHandle);
  });

  ipcMain.handle('engine:listMaterialInstances', () => {
    return dsengine.listMaterialInstances();
  });

  ipcMain.handle('engine:updateMaterialInstance', (event, materialId, payload) => {
    return dsengine.updateMaterialInstance(materialId, payload);
  });

  ipcMain.handle('engine:applyMaterialToEntity', (event, entityId, materialId) => {
    return dsengine.applyMaterialToEntity(entityId, materialId);
  });

  ipcMain.handle('engine:listMaterialHotUpdateEvents', () => {
    return dsengine.listMaterialHotUpdateEvents();
  });

  ipcMain.handle('engine:clearMaterialHotUpdateEvents', () => {
    return dsengine.clearMaterialHotUpdateEvents();
  });

  ipcMain.handle('engine:replayMaterialHotUpdates', (event, maxSequence) => {
    return dsengine.replayMaterialHotUpdates(maxSequence ?? 0);
  });

  ipcMain.handle('engine:getFrameBridgeStats', () => {
    return dsengine.getFrameBridgeStats();
  });

  ipcMain.handle('engine:pickTextureFile', async () => {
    const result = await dialog.showOpenDialog({
      title: 'Select Texture',
      properties: ['openFile'],
      filters: [
        { name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'bmp', 'tga'] },
        { name: 'All Files', extensions: ['*'] }
      ]
    });
    if (result.canceled || result.filePaths.length === 0) {
      return '';
    }
    return result.filePaths[0];
  });

  ipcMain.handle('engine:pickEntity', (event, x, y) => {
    return dsengine.pickEntity(x, y);
  });

  ipcMain.handle('engine:createEntity', () => {
    return dsengine.createEntity();
  });

  ipcMain.handle('engine:deleteEntity', (event, id) => {
    return dsengine.deleteEntity(id);
  });

  ipcMain.handle('engine:buildProject', (event, target) => {
    const scriptPath = path.join(__dirname, 'scripts', 'build_pipeline.js');
    exec(`node "${scriptPath}" ${target}`, (err, stdout, stderr) => {
      if (err) {
        console.error("Build Pipeline Error:", stderr);
      } else {
        console.log("Build Pipeline Success:", stdout);
      }
    });

    return dsengine.buildProject(target);
  });

  mainWindow.loadFile('index.html');
  
  // Open the DevTools.
  // mainWindow.webContents.openDevTools();
}

app.whenReady().then(() => {
  createWindow();
  startRuntimeFrameBridge();

  app.on('activate', function () {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('before-quit', function () {
  stopRuntimeFrameBridge();
});

app.on('window-all-closed', function () {
  if (process.platform !== 'darwin') app.quit();
});
