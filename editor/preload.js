const { ipcRenderer } = require('electron');
const dsengine = require('./build/Release/dsengine_bridge.node');

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
  getFrameBuffer: () => Promise.resolve(dsengine.getFrameBuffer()),
  getFrameInfo: () => Promise.resolve(dsengine.getFrameInfo()),
  pushExternalFrame: (frameBuffer, width, height) => ipcRenderer.invoke('engine:pushExternalFrame', frameBuffer, width, height),
  clearExternalFrame: () => ipcRenderer.invoke('engine:clearExternalFrame'),
  getEntities: () => ipcRenderer.invoke('engine:getEntities'),
  updateEntityTransform: (id, x, y, z) => ipcRenderer.invoke('engine:updateEntityTransform', id, x, y, z),
  tickEngine: (deltaTime) => ipcRenderer.invoke('engine:tick', deltaTime),
  importTexture: (filePath) => ipcRenderer.invoke('engine:importTexture', filePath),
  listImportedTextures: () => ipcRenderer.invoke('engine:listImportedTextures'),
  applyTextureToEntity: (entityId, textureHandle) => ipcRenderer.invoke('engine:applyTextureToEntity', entityId, textureHandle),
  listShaderVariants: () => ipcRenderer.invoke('engine:listShaderVariants'),
  createMaterialInstance: (name, shaderVariant, textureHandle) => ipcRenderer.invoke('engine:createMaterialInstance', name, shaderVariant, textureHandle),
  listMaterialInstances: () => ipcRenderer.invoke('engine:listMaterialInstances'),
  updateMaterialInstance: (materialId, payload) => ipcRenderer.invoke('engine:updateMaterialInstance', materialId, payload),
  applyMaterialToEntity: (entityId, materialId) => ipcRenderer.invoke('engine:applyMaterialToEntity', entityId, materialId),
  listMaterialHotUpdateEvents: () => ipcRenderer.invoke('engine:listMaterialHotUpdateEvents'),
  clearMaterialHotUpdateEvents: () => ipcRenderer.invoke('engine:clearMaterialHotUpdateEvents'),
  replayMaterialHotUpdates: (maxSequence) => ipcRenderer.invoke('engine:replayMaterialHotUpdates', maxSequence),
  getFrameBridgeStats: () => ipcRenderer.invoke('engine:getFrameBridgeStats'),
  pickTextureFile: () => ipcRenderer.invoke('engine:pickTextureFile'),
  pickEntity: (x, y) => ipcRenderer.invoke('engine:pickEntity', x, y),
  createEntity: () => ipcRenderer.invoke('engine:createEntity'),
  deleteEntity: (id) => ipcRenderer.invoke('engine:deleteEntity', id),
  buildProject: (target) => ipcRenderer.invoke('engine:buildProject', target)
};
