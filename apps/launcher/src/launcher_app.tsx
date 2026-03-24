import React, { useEffect, useMemo, useState } from 'react';

type EngineVersion = {
  tag: string;
  executable: string;
  available: boolean;
};

type ProjectItem = {
  name: string;
  path: string;
};

declare global {
  interface Window {
    launcherAPI?: {
      getEngineVersions: () => Promise<EngineVersion[]>;
      chooseProjectRoot: () => Promise<string>;
      scanProjects: (rootDir: string) => Promise<ProjectItem[]>;
      launchEditor: (projectPath: string, versionTag: string) => Promise<{ success: boolean }>;
    };
  }
}

export function LauncherApp() {
  const [versions, setVersions] = useState<EngineVersion[]>([]);
  const [selectedVersion, setSelectedVersion] = useState<string>('debug');
  const [projectRoot, setProjectRoot] = useState<string>('');
  const [projects, setProjects] = useState<ProjectItem[]>([]);
  const [selectedProjectPath, setSelectedProjectPath] = useState<string>('');
  const [status, setStatus] = useState<string>('就绪');

  const canLaunch = useMemo(() => {
    return !!selectedProjectPath && selectedVersion.length > 0;
  }, [selectedProjectPath, selectedVersion]);

  useEffect(() => {
    const loadVersions = async () => {
      if (!window.launcherAPI) {
        return;
      }
      const data = await window.launcherAPI.getEngineVersions();
      setVersions(data);
      const available = data.find((item) => item.available);
      if (available) {
        setSelectedVersion(available.tag);
      } else if (data.length > 0) {
        setSelectedVersion(data[0].tag);
      }
    };
    loadVersions().catch((err) => setStatus(`加载引擎版本失败: ${String(err)}`));
  }, []);

  const chooseRoot = async () => {
    if (!window.launcherAPI) {
      return;
    }
    const selected = await window.launcherAPI.chooseProjectRoot();
    if (!selected) {
      return;
    }
    setProjectRoot(selected);
    const items = await window.launcherAPI.scanProjects(selected);
    setProjects(items);
    if (items.length > 0) {
      setSelectedProjectPath(items[0].path);
    } else {
      setSelectedProjectPath('');
    }
    setStatus(`已扫描项目: ${items.length}`);
  };

  const launch = async () => {
    if (!window.launcherAPI || !canLaunch) {
      return;
    }
    const result = await window.launcherAPI.launchEditor(selectedProjectPath, selectedVersion);
    setStatus(result.success ? '已启动 Editor' : '启动失败');
  };

  return (
    <div style={{ color: '#e6e8ef', fontFamily: 'Segoe UI, sans-serif', padding: 16 }}>
      <h2 style={{ margin: '0 0 12px 0' }}>DSEngine Launcher</h2>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: 12 }}>
        <section style={{ border: '1px solid #2a3242', borderRadius: 8, padding: 12, background: '#161b24' }}>
          <div style={{ fontWeight: 600, marginBottom: 8 }}>引擎版本</div>
          <select
            value={selectedVersion}
            onChange={(e) => setSelectedVersion(e.target.value)}
            style={{ width: '100%', background: '#0f1420', color: '#e6e8ef', border: '1px solid #2a3242', padding: 8, borderRadius: 6 }}
          >
            {versions.map((v) => (
              <option key={v.tag} value={v.tag}>
                {v.tag} {v.available ? '(可用)' : '(缺失)'}
              </option>
            ))}
          </select>
        </section>

        <section style={{ border: '1px solid #2a3242', borderRadius: 8, padding: 12, background: '#161b24' }}>
          <div style={{ fontWeight: 600, marginBottom: 8 }}>项目管理</div>
          <div style={{ display: 'flex', gap: 8, marginBottom: 8 }}>
            <button
              onClick={chooseRoot}
              style={{ background: '#2d6cdf', color: '#fff', border: 'none', padding: '8px 10px', borderRadius: 6, cursor: 'pointer' }}
            >
              选择项目根目录
            </button>
            <div style={{ alignSelf: 'center', color: '#9fb0d1', fontSize: 12 }}>{projectRoot || '未选择目录'}</div>
          </div>
          <select
            value={selectedProjectPath}
            onChange={(e) => setSelectedProjectPath(e.target.value)}
            style={{ width: '100%', background: '#0f1420', color: '#e6e8ef', border: '1px solid #2a3242', padding: 8, borderRadius: 6 }}
          >
            <option value="">请选择项目</option>
            {projects.map((p) => (
              <option key={p.path} value={p.path}>
                {p.name}
              </option>
            ))}
          </select>
        </section>

        <section style={{ border: '1px solid #2a3242', borderRadius: 8, padding: 12, background: '#161b24', display: 'flex', gap: 8, alignItems: 'center' }}>
          <button
            onClick={launch}
            disabled={!canLaunch}
            style={{
              background: canLaunch ? '#18a05e' : '#334',
              color: '#fff',
              border: 'none',
              padding: '10px 14px',
              borderRadius: 6,
              cursor: canLaunch ? 'pointer' : 'not-allowed'
            }}
          >
            启动 Editor
          </button>
          <span style={{ color: '#9fb0d1' }}>状态: {status}</span>
        </section>
      </div>
    </div>
  );
}
