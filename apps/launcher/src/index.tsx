import React from 'react';
import { createRoot } from 'react-dom/client';
import { LauncherApp } from './launcher_app';

// Global style reset for Chinese Retro UI
const style = document.createElement('style');
style.textContent = `
  @import url('https://fonts.googleapis.com/css2?family=ZCOOL+XiaoWei&display=swap');
  
  * { box-sizing: border-box; }
  body, html { margin: 0; padding: 0; width: 100%; height: 100%; background: #fdf6e3; overflow: hidden; font-family: 'ZCOOL XiaoWei', "STKaiti", "KaiTi", serif; }
  ::-webkit-scrollbar { width: 8px; height: 8px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: #d4c4b7; border-radius: 0px; border: 1px solid #8c3a3a; }
  ::-webkit-scrollbar-thumb:hover { background: #bfa793; }
`;
document.head.appendChild(style);

const root = document.getElementById('root');
if (root) {
  createRoot(root).render(<LauncherApp />);
}
