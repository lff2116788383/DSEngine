/**
 * index.tsx 组件/功能描述
 */

import React from 'react';
import { createRoot } from 'react-dom/client';
import { LauncherApp } from './launcher_app';

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
