import React from 'react';
import { createRoot } from 'react-dom/client';
import { LauncherApp } from './launcher_app';

const root = document.getElementById('root');
if (root) {
  createRoot(root).render(<LauncherApp />);
}
