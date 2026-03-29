/**
 * index.tsx 组件/功能描述
 */

import React from 'react';
import { createRoot } from 'react-dom/client';
import { invoke } from '@tauri-apps/api/core';
import { LauncherApp } from './launcher_app';

// Set up the launcher API bridge to Rust backend
window.launcherAPI = {
  getEngineVersions: () => invoke('get_engine_versions'),
  chooseProjectRoot: () => invoke('choose_project_root'),
  scanProjects: (rootDir: string) => invoke('scan_projects', { rootDir }),
  launchEditor: (projectPath: string, executable: string) => 
    invoke('launch_editor', { projectPath, executable }),
};

// Global style reset for Tech Dark UI
const style = document.createElement('style');
style.textContent = `
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap');
  
  * { box-sizing: border-box; }
  body, html { margin: 0; padding: 0; width: 100%; height: 100%; background: #18181c; overflow: hidden; font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  ::-webkit-scrollbar { width: 8px; height: 8px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: #333344; border-radius: 4px; }
  ::-webkit-scrollbar-thumb:hover { background: #00a8ff; }
`;
document.head.appendChild(style);

const root = document.getElementById('root');
if (root) {
  createRoot(root).render(<LauncherApp />);
}
