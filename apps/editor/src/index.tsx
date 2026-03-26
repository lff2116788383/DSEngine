/**
 * index.tsx 组件/功能描述
 */

import React from 'react';
import { createRoot } from 'react-dom/client';
import { EditorApp } from './components/EditorApp';

const container = document.getElementById('root');
if (container) {
  const root = createRoot(container);
  root.render(<EditorApp />);
}
