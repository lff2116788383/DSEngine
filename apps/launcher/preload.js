const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('launcherAPI', {
  getEngineVersions: () => ipcRenderer.invoke('launcher:getEngineVersions'),
  chooseProjectRoot: () => ipcRenderer.invoke('launcher:chooseProjectRoot'),
  scanProjects: (rootDir) => ipcRenderer.invoke('launcher:scanProjects', rootDir),
  launchEditor: (projectPath, versionTag) => ipcRenderer.invoke('launcher:launchEditor', projectPath, versionTag)
});
