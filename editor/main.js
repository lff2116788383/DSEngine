const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');

// Load the C++ Addon
const dsengine = require('./build/Release/dsengine_bridge.node');

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

  ipcMain.handle('engine:getEntities', () => {
    return dsengine.getEntities();
  });

  ipcMain.handle('engine:updateEntityTransform', (event, id, x, y, z) => {
    return dsengine.updateEntityTransform(id, x, y, z);
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
    // We execute the build script directly from Node.js (main process)
    // rather than doing it in C++, so we can easily stream output if needed.
    const { exec } = require('child_process');
    const path = require('path');
    
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

  app.on('activate', function () {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', function () {
  if (process.platform !== 'darwin') app.quit();
});
