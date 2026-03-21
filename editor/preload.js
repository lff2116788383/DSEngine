const { ipcRenderer } = require('electron');

window.addEventListener('DOMContentLoaded', () => {
  const replaceText = (selector, text) => {
    const element = document.getElementById(selector)
    if (element) element.innerText = text
  }

  for (const type of ['chrome', 'node', 'electron']) {
    replaceText(`${type}-version`, process.versions[type])
  }
})

// Expose IPC to the React frontend
window.electronAPI = {
  getEngineVersion: () => ipcRenderer.invoke('engine:getVersion'),
  initEngine: () => ipcRenderer.invoke('engine:init'),
  getFrameBuffer: () => ipcRenderer.invoke('engine:getFrameBuffer'),
  getEntities: () => ipcRenderer.invoke('engine:getEntities'),
  updateEntityTransform: (id, x, y, z) => ipcRenderer.invoke('engine:updateEntityTransform', id, x, y, z),
  pickEntity: (x, y) => ipcRenderer.invoke('engine:pickEntity', x, y),
  buildProject: (target) => ipcRenderer.invoke('engine:buildProject', target)
};
