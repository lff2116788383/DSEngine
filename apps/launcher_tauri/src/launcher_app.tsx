/**
 * launcher_app.tsx 组件/功能描述
 */

import React, { useEffect, useMemo, useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import {
  Folder, Cpu, Settings, Play, Activity, Box, Globe, Plus, Search, 
  MonitorPlay, Terminal, Zap, Radio, User, ShoppingCart, BookOpen, 
  Download, MoreVertical, Bell, Cloud, Star, ChevronRight, CheckCircle2
} from 'lucide-react';

type EngineVersion = {
  tag: string;
  executable: string;
  available: boolean;
  downloading?: boolean;
  progress?: number;
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

const theme = {
  bg: '#fdf6e3', // 宣纸底色
  sidebar: '#f5ebdb', // 稍深的宣纸色
  card: '#fffbf0', // 卡片底色
  cardHover: '#f0e4d3',
  primary: '#8c3a3a', // 故宫红/朱砂红
  primaryDim: 'rgba(140, 58, 58, 0.1)',
  textMain: '#2c2c2c', // 墨黑
  textMuted: '#6b5d50', // 枯叶褐
  border: '#d4c4b7', // 边框褐
  success: '#4a7a5b', // 黛绿
  danger: '#a64036', // 胭脂红
  warning: '#b8860b' // 藤黄
};

const tabs = [
  { id: 'dashboard', label: '控制台', icon: Activity },
  { id: 'projects', label: '项目库', icon: Box },
  { id: 'engines', label: '引擎版本', icon: Cpu },
  { id: 'store', label: '资源商城', icon: ShoppingCart },
  { id: 'learn', label: '学习中心', icon: BookOpen },
  { id: 'settings', label: '设置', icon: Settings },
];

export function LauncherApp() {
  const [activeTab, setActiveTab] = useState('projects');
  const [versions, setVersions] = useState<EngineVersion[]>([]);
  const [selectedVersion, setSelectedVersion] = useState<string>('');
  const [projectRoot, setProjectRoot] = useState<string>('');
  const [projects, setProjects] = useState<ProjectItem[]>([]);
  const [selectedProjectPath, setSelectedProjectPath] = useState<string>('');
  const [status, setStatus] = useState<string>('System Online');
  const [searchQuery, setSearchQuery] = useState('');

  const canLaunch = useMemo(() => {
    return !!selectedProjectPath && selectedVersion.length > 0;
  }, [selectedProjectPath, selectedVersion]);

  useEffect(() => {
    const loadVersions = async () => {
      if (!window.launcherAPI) return;
      const data = await window.launcherAPI.getEngineVersions();
      // Mock some commercial states for demo
      if (data.length > 0) {
        data.push({ tag: 'v1.3.0-beta', executable: '', available: false, downloading: true, progress: 45 });
        data.push({ tag: 'v1.1.5-lts', executable: '', available: false });
      }
      setVersions(data);
      const available = data.find((item) => item.available);
      if (available) {
        setSelectedVersion(available.tag);
      } else if (data.length > 0) {
        setSelectedVersion(data[0].tag);
      }
    };
    loadVersions().catch((err) => setStatus(`Error loading engines: ${String(err)}`));
  }, []);

  const chooseRoot = async () => {
    if (!window.launcherAPI) return;
    const selected = await window.launcherAPI.chooseProjectRoot();
    if (!selected) return;
    setProjectRoot(selected);
    const items = await window.launcherAPI.scanProjects(selected);
    setProjects(items);
    if (items.length > 0) {
      setSelectedProjectPath(items[0].path);
    } else {
      setSelectedProjectPath('');
    }
    setStatus(`Scanned ${items.length} projects`);
  };

  const launch = async () => {
    if (!window.launcherAPI || !canLaunch) return;
    setStatus('Initializing launch sequence...');
    const result = await window.launcherAPI.launchEditor(selectedProjectPath, selectedVersion);
    setStatus(result.success ? 'Editor deployed successfully' : 'Launch sequence failed');
  };

  const filteredProjects = projects.filter(p => 
    p.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div style={{ display: 'flex', height: '100vh', width: '100vw', background: theme.bg, color: theme.textMain, fontFamily: '"STKaiti", "KaiTi", serif', overflow: 'hidden', backgroundImage: 'url("data:image/svg+xml,%3Csvg width=\'100\' height=\'100\' viewBox=\'0 0 100 100\' xmlns=\'http://www.w3.org/2000/svg\'%3E%3Cfilter id=\'noise\'%3E%3CfeTurbulence type=\'fractalNoise\' baseFrequency=\'0.8\' numOctaves=\'4\' stitchTiles=\'stitch\'/%3E%3C/filter%3E%3Crect width=\'100\' height=\'100\' filter=\'url(%23noise)\' opacity=\'0.08\'/%3E%3C/svg%3E")' }}>
      {/* Sidebar */}
      <div style={{ width: 260, background: theme.sidebar, borderRight: `2px solid ${theme.primary}`, display: 'flex', flexDirection: 'column', position: 'relative' }}>
        <div style={{ padding: '32px 20px', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 16, borderBottom: `1px dashed ${theme.border}` }}>
          <div style={{ width: 64, height: 64, borderRadius: '50%', border: `3px solid ${theme.primary}`, display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#fff', position: 'relative' }}>
            <div style={{ position: 'absolute', width: 54, height: 54, borderRadius: '50%', border: `1px solid ${theme.primary}` }}></div>
            <span style={{ fontSize: 28, color: theme.primary, fontWeight: 'bold' }}>道</span>
          </div>
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: 24, fontWeight: 900, letterSpacing: 4, color: theme.primary, textShadow: '1px 1px 0px rgba(0,0,0,0.1)' }}>大衍引擎</div>
            <div style={{ fontSize: 12, color: theme.textMuted, letterSpacing: 6, marginTop: 4 }}>天工开物</div>
          </div>
        </div>

        <nav style={{ flex: 1, padding: '20px 16px', display: 'flex', flexDirection: 'column', gap: 8 }}>
          {tabs.map((tab) => {
            const isActive = activeTab === tab.id;
            const Icon = tab.icon;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                style={{
                  display: 'flex', alignItems: 'center', gap: 12, padding: '12px 16px', 
                  border: isActive ? `1px solid ${theme.primary}` : '1px solid transparent', 
                  background: isActive ? '#fff' : 'transparent',
                  color: isActive ? theme.primary : theme.textMain,
                  cursor: 'pointer', transition: 'all 0.3s', textAlign: 'left', fontSize: 16, fontWeight: 600,
                  position: 'relative'
                }}
              >
                <Icon size={20} />
                {tab.label}
                {isActive && (
                  <motion.div layoutId="sidebar-indicator" style={{ position: 'absolute', right: -1, top: -1, bottom: -1, width: 4, background: theme.primary }} />
                )}
              </button>
            );
          })}
        </nav>

        {/* User Auth Profile */}
        <div style={{ padding: '16px 20px', borderTop: `1px dashed ${theme.border}`, display: 'flex', alignItems: 'center', gap: 12, cursor: 'pointer' }}>
          <div style={{ width: 40, height: 40, borderRadius: '50%', background: '#fff', display: 'flex', alignItems: 'center', justifyContent: 'center', border: `2px solid ${theme.primary}` }}>
            <User size={20} color={theme.primary} />
          </div>
          <div style={{ flex: 1, overflow: 'hidden' }}>
            <div style={{ fontSize: 15, fontWeight: 600, color: theme.textMain }}>墨客_张三</div>
            <div style={{ fontSize: 12, color: theme.success, display: 'flex', alignItems: 'center', gap: 4, marginTop: 2 }}>
              <CheckCircle2 size={12} /> 巧匠玉牌
            </div>
          </div>
        </div>

        <div style={{ padding: '12px 20px', borderTop: `1px solid ${theme.border}`, background: theme.cardHover }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 12, color: theme.textMuted }}>
            <Radio size={14} color={theme.success} />
            <span>阵法运转正常 ({status})</span>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative' }}>
        {/* Top bar */}
        <header style={{ height: 64, borderBottom: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', padding: '0 24px', justifyContent: 'space-between' }}>
          <div style={{ fontSize: 20, fontWeight: 600 }}>
            {tabs.find(t => t.id === activeTab)?.label}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 20 }}>
            <div style={{ position: 'relative' }}>
              <Search size={16} color={theme.textMuted} style={{ position: 'absolute', left: 12, top: 10 }} />
              <input
                type="text"
                placeholder="搜索项目或资源..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{
                  background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 20,
                  padding: '8px 16px 8px 36px', color: theme.textMain, outline: 'none', width: 220,
                  transition: 'border-color 0.2s'
                }}
                onFocus={(e) => e.target.style.borderColor = theme.primary}
                onBlur={(e) => e.target.style.borderColor = theme.border}
              />
            </div>
            
            {/* Global Notification & Downloads */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, borderLeft: `1px solid ${theme.border}`, paddingLeft: 20 }}>
              <div style={{ position: 'relative', cursor: 'pointer', padding: 4 }} title="Downloads">
                <Download size={20} color={theme.textMuted} />
                <div style={{ position: 'absolute', top: 2, right: 2, width: 8, height: 8, background: theme.primary, borderRadius: '50%' }}></div>
              </div>
              <div style={{ position: 'relative', cursor: 'pointer', padding: 4 }} title="Notifications">
                <Bell size={20} color={theme.textMuted} />
                <div style={{ position: 'absolute', top: 2, right: 4, width: 8, height: 8, background: theme.danger, borderRadius: '50%' }}></div>
              </div>
              <div style={{ width: 32, height: 32, borderRadius: '50%', background: theme.card, border: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center', cursor: 'pointer' }} title="Network Status">
                <Cloud size={16} color={theme.success} />
              </div>
            </div>
          </div>
        </header>

        {/* Content Area */}
        <main style={{ flex: 1, overflowY: 'auto', padding: 32 }}>
          <AnimatePresence mode="wait">
            {activeTab === 'dashboard' && (
              <motion.div key="dashboard" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 24, marginBottom: 32 }}>
                  {[
                    { title: '天机同步', value: '已同步', sub: '刚刚更新', icon: Cloud, color: theme.success },
                    { title: '藏经阁项目', value: projects.length.toString(), sub: '当前工坊', icon: Box, color: theme.primary },
                    { title: '百宝箱资产', value: '142', sub: '已购图纸', icon: ShoppingCart, color: '#8b5a2b' }
                  ].map((stat, i) => (
                    <div key={i} style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 0, padding: 20, position: 'relative', overflow: 'hidden', boxShadow: '2px 2px 0px rgba(0,0,0,0.05)' }}>
                      <div style={{ position: 'absolute', top: -10, right: -10, opacity: 0.05 }}>
                        <stat.icon size={120} color={stat.color} />
                      </div>
                      <div style={{ color: theme.textMuted, fontSize: 14, fontWeight: 600, letterSpacing: 2, marginBottom: 8 }}>{stat.title}</div>
                      <div style={{ fontSize: 32, fontWeight: 700, marginBottom: 4, color: theme.primary }}>{stat.value}</div>
                      <div style={{ fontSize: 12, color: theme.textMuted }}>{stat.sub}</div>
                    </div>
                  ))}
                </div>
                
                <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: 24 }}>
                  <div>
                    <h3 style={{ marginBottom: 16, fontWeight: 600, borderBottom: `2px solid ${theme.primary}`, display: 'inline-block', paddingBottom: 4 }}>邸报 & 传音</h3>
                    <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 0, padding: 20, boxShadow: '2px 2px 0px rgba(0,0,0,0.05)' }}>
                      <div style={{ display: 'flex', gap: 16, alignItems: 'flex-start', borderBottom: `1px dashed ${theme.border}`, paddingBottom: 16, marginBottom: 16 }}>
                        <div style={{ background: theme.primaryDim, padding: 8, border: `1px solid ${theme.primary}` }}><MonitorPlay size={20} color={theme.primary} /></div>
                        <div>
                          <div style={{ fontWeight: 600, marginBottom: 4, fontSize: 16 }}>大衍引擎 甲子版 开放试炼</div>
                          <div style={{ color: theme.textMuted, fontSize: 14, lineHeight: 1.6 }}>重铸天地熔炉（渲染管线）与万物法则（物理引擎），立即获取灵力加持。</div>
                        </div>
                      </div>
                      <div style={{ display: 'flex', gap: 16, alignItems: 'flex-start' }}>
                        <div style={{ background: 'rgba(74, 122, 91, 0.1)', padding: 8, border: `1px solid ${theme.success}` }}><Star size={20} color={theme.success} /></div>
                        <div>
                          <div style={{ fontWeight: 600, marginBottom: 4, fontSize: 16 }}>聚宝阁 春季大集</div>
                          <div style={{ color: theme.textMuted, fontSize: 14, lineHeight: 1.6 }}>上乘仙山楼阁图纸与风雨雷电符箓半价兑换，机不可失！</div>
                        </div>
                      </div>
                    </div>
                  </div>
                  <div>
                    <h3 style={{ marginBottom: 16, fontWeight: 600, borderBottom: `2px solid ${theme.primary}`, display: 'inline-block', paddingBottom: 4 }}>案牍速览</h3>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                      {projects.slice(0, 3).map(p => (
                        <div key={p.path} onClick={() => { setActiveTab('projects'); setSelectedProjectPath(p.path); }} style={{ background: theme.card, border: `1px solid ${theme.border}`, padding: 12, display: 'flex', alignItems: 'center', gap: 12, cursor: 'pointer', transition: 'background 0.2s' }}
                             onMouseEnter={(e) => e.currentTarget.style.background = theme.cardHover}
                             onMouseLeave={(e) => e.currentTarget.style.background = theme.card}>
                          <div style={{ width: 32, height: 32, border: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                            <Box size={16} color={theme.primary} />
                          </div>
                          <div style={{ flex: 1, overflow: 'hidden' }}>
                            <div style={{ fontSize: 15, fontWeight: 600 }}>{p.name}</div>
                            <div style={{ fontSize: 12, color: theme.textMuted, whiteSpace: 'nowrap', textOverflow: 'ellipsis', overflow: 'hidden' }}>{p.path}</div>
                          </div>
                        </div>
                      ))}
                    </div>
                  </div>
                </div>
              </motion.div>
            )}

            {activeTab === 'projects' && (
              <motion.div key="projects" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 24 }}>
                  <div>
                    <h3 style={{ margin: '0 0 4px 0', fontSize: 18, borderBottom: `2px solid ${theme.primary}`, display: 'inline-block', paddingBottom: 4 }}>藏经阁 (项目管理)</h3>
                    <div style={{ color: theme.textMuted, fontSize: 13, marginTop: 8 }}>管理、备份与云端同步您的天工造物</div>
                  </div>
                  <div style={{ display: 'flex', gap: 12 }}>
                    <button style={{ background: theme.card, color: theme.primary, border: `1px solid ${theme.primary}`, padding: '10px 16px', fontWeight: 600, display: 'flex', alignItems: 'center', gap: 8, cursor: 'pointer' }}>
                      <Plus size={16} /> 开坛铸器 (新建)
                    </button>
                    <button
                      onClick={chooseRoot}
                      style={{
                        background: theme.primary, color: '#fff', border: 'none', padding: '10px 20px',
                        fontWeight: 600, display: 'flex', alignItems: 'center', gap: 8, cursor: 'pointer',
                        boxShadow: `2px 2px 0px rgba(0,0,0,0.2)`
                      }}
                    >
                      <Folder size={16} /> 寻脉定穴 (扫描)
                    </button>
                  </div>
                </div>

                {projectRoot && (
                  <div style={{ marginBottom: 24, fontSize: 13, color: theme.textMuted, display: 'flex', alignItems: 'center', gap: 8 }}>
                    <Terminal size={14} />
                    当前阵眼: <span style={{ color: theme.primary, fontFamily: 'monospace' }}>{projectRoot}</span>
                  </div>
                )}

                {filteredProjects.length === 0 ? (
                  <div style={{ background: theme.card, border: `1px dashed ${theme.primary}`, padding: 60, textAlign: 'center' }}>
                    <Box size={48} color={theme.textMuted} style={{ margin: '0 auto 16px' }} />
                    <div style={{ fontSize: 16, fontWeight: 600, marginBottom: 8 }}>未见灵力波动</div>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>请点击上方按钮寻脉定穴，或开坛铸造新物</div>
                  </div>
                ) : (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: 20 }}>
                    {filteredProjects.map((p) => {
                      const isSelected = selectedProjectPath === p.path;
                      return (
                        <motion.div
                          key={p.path}
                          whileHover={{ y: -4, borderColor: theme.primary }}
                          onClick={() => setSelectedProjectPath(p.path)}
                          style={{
                            background: isSelected ? theme.cardHover : theme.card,
                            border: `1px solid ${isSelected ? theme.primary : theme.border}`,
                            overflow: 'hidden', cursor: 'pointer',
                            boxShadow: isSelected ? `2px 2px 0px ${theme.primary}` : '2px 2px 0px rgba(0,0,0,0.05)',
                            position: 'relative'
                          }}
                        >
                          {/* Project Context Menu Mock */}
                          <div style={{ position: 'absolute', top: 12, right: 12, width: 28, height: 28, background: 'rgba(255,255,255,0.8)', border: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center' }} onClick={(e) => e.stopPropagation()}>
                            <MoreVertical size={16} color={theme.primary} />
                          </div>

                          <div style={{ height: 120, background: `linear-gradient(to bottom right, ${theme.sidebar}, #e8dac5)`, display: 'flex', alignItems: 'center', justifyContent: 'center', borderBottom: `1px solid ${theme.border}` }}>
                            <MonitorPlay size={40} color={isSelected ? theme.primary : theme.textMuted} opacity={0.6} />
                          </div>
                          <div style={{ padding: 16 }}>
                            <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 4, color: theme.primary }}>{p.name}</div>
                            <div style={{ color: theme.textMuted, fontSize: 12, fontFamily: 'monospace', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                              {p.path}
                            </div>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: 12, borderTop: `1px dashed ${theme.border}`, paddingTop: 12 }}>
                               <div style={{ fontSize: 12, color: theme.success, display: 'flex', alignItems: 'center', gap: 4 }}><Cloud size={14} /> 天机已合</div>
                               <div style={{ fontSize: 12, border: `1px solid ${theme.primary}`, color: theme.primary, padding: '2px 6px' }}>甲子版</div>
                            </div>
                          </div>
                        </motion.div>
                      );
                    })}
                  </div>
                )}
              </motion.div>
            )}

            {activeTab === 'engines' && (
              <motion.div key="engines" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 24 }}>
                  <div>
                    <h3 style={{ margin: '0 0 4px 0', fontSize: 18 }}>引擎版本</h3>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>管理核心引擎、下载新版本与组件</div>
                  </div>
                </div>

                <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                  {versions.length === 0 ? (
                    <div style={{ color: theme.textMuted, padding: 20 }}>未加载到任何引擎版本信息。</div>
                  ) : (
                    versions.map((v) => {
                      const isSelected = selectedVersion === v.tag;
                      return (
                        <div
                          key={v.tag}
                          onClick={() => { if(v.available) setSelectedVersion(v.tag) }}
                          style={{
                            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                            background: isSelected ? theme.cardHover : theme.card,
                            border: `1px solid ${isSelected ? theme.primary : theme.border}`,
                            padding: 20, borderRadius: 12, cursor: v.available ? 'pointer' : 'default', transition: 'all 0.2s',
                            opacity: (v.available || v.downloading) ? 1 : 0.6
                          }}
                        >
                          <div style={{ display: 'flex', alignItems: 'center', gap: 16, flex: 1 }}>
                            <div style={{ width: 48, height: 48, borderRadius: 12, background: 'rgba(0, 210, 255, 0.1)', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                              <Cpu size={24} color={theme.primary} />
                            </div>
                            <div style={{ flex: 1 }}>
                              <div style={{ fontWeight: 600, fontSize: 16, display: 'flex', alignItems: 'center', gap: 8 }}>
                                {v.tag}
                                {v.available ? (
                                  <span style={{ fontSize: 10, background: 'rgba(16, 185, 129, 0.2)', color: theme.success, padding: '2px 6px', borderRadius: 4 }}>INSTALLED</span>
                                ) : v.downloading ? (
                                  <span style={{ fontSize: 10, background: 'rgba(0, 210, 255, 0.2)', color: theme.primary, padding: '2px 6px', borderRadius: 4 }}>DOWNLOADING</span>
                                ) : (
                                  <span style={{ fontSize: 10, background: 'rgba(239, 68, 68, 0.2)', color: theme.danger, padding: '2px 6px', borderRadius: 4 }}>AVAILABLE</span>
                                )}
                              </div>
                              {v.downloading ? (
                                <div style={{ marginTop: 8, display: 'flex', alignItems: 'center', gap: 12, width: '100%', maxWidth: 300 }}>
                                   <div style={{ height: 4, background: theme.border, borderRadius: 2, flex: 1, overflow: 'hidden' }}>
                                      <div style={{ width: `${v.progress}%`, height: '100%', background: theme.primary, borderRadius: 2 }} />
                                   </div>
                                   <div style={{ fontSize: 12, color: theme.primary }}>{v.progress}% (12MB/s)</div>
                                </div>
                              ) : (
                                <div style={{ color: theme.textMuted, fontSize: 12, marginTop: 4, fontFamily: 'monospace' }}>
                                  {v.executable || 'Cloud Release'}
                                </div>
                              )}
                            </div>
                          </div>
                          {isSelected && <div style={{ color: theme.primary, fontWeight: 600, fontSize: 13 }}>SELECTED</div>}
                          {!v.available && !v.downloading && (
                             <button style={{ background: theme.bg, border: `1px solid ${theme.border}`, color: theme.textMain, padding: '6px 12px', borderRadius: 6, display: 'flex', alignItems: 'center', gap: 6, cursor: 'pointer' }}>
                               <Download size={14} /> Install
                             </button>
                          )}
                        </div>
                      );
                    })
                  )}
                </div>
              </motion.div>
            )}

            {/* Mock Store Tab */}
            {activeTab === 'store' && (
              <motion.div key="store" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 24 }}>
                  <div>
                    <h3 style={{ margin: '0 0 4px 0', fontSize: 18 }}>资源商城</h3>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>发现并购买官方与社区提供的高质量资产</div>
                  </div>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(240px, 1fr))', gap: 20 }}>
                   {[
                     { name: 'Neon City Environment', author: 'DSEngine Official', price: '$29.99', tag: '3D Model' },
                     { name: 'Advanced TPS Controller', author: 'Studio X', price: '$14.99', tag: 'Blueprint' },
                     { name: 'Sci-fi UI Framework', author: 'UI Masters', price: 'Free', tag: 'UI' },
                     { name: 'Dynamic Weather System', author: 'NatureFX', price: '$49.99', tag: 'Plugin' },
                   ].map((item, i) => (
                     <div key={i} style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, overflow: 'hidden', cursor: 'pointer' }}>
                        <div style={{ height: 140, background: `linear-gradient(45deg, #1e293b, ${theme.sidebar})`, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                           <ShoppingCart size={32} color={theme.textMuted} opacity={0.3} />
                        </div>
                        <div style={{ padding: 16 }}>
                           <div style={{ fontSize: 10, color: theme.primary, marginBottom: 4 }}>{item.tag}</div>
                           <div style={{ fontWeight: 600, fontSize: 15, marginBottom: 4 }}>{item.name}</div>
                           <div style={{ fontSize: 12, color: theme.textMuted, marginBottom: 12 }}>by {item.author}</div>
                           <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                              <div style={{ fontWeight: 700 }}>{item.price}</div>
                              <div style={{ background: theme.primaryDim, color: theme.primary, padding: '4px 12px', borderRadius: 16, fontSize: 12, fontWeight: 600 }}>Get</div>
                           </div>
                        </div>
                     </div>
                   ))}
                </div>
              </motion.div>
            )}

            {/* Mock Learn Tab */}
            {activeTab === 'learn' && (
              <motion.div key="learn" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <h3 style={{ margin: '0 0 24px 0', fontSize: 18 }}>学习中心</h3>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 20 }}>
                  <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 24, display: 'flex', gap: 16 }}>
                     <div style={{ width: 120, height: 80, background: '#1e293b', borderRadius: 8, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                        <Play size={24} color={theme.textMuted} />
                     </div>
                     <div>
                        <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 8 }}>Getting Started with DSEngine</div>
                        <div style={{ fontSize: 13, color: theme.textMuted, marginBottom: 12 }}>Learn the basics of the editor, scene management, and basic scripting.</div>
                        <div style={{ fontSize: 12, color: theme.primary, cursor: 'pointer' }}>Watch Tutorial ➔</div>
                     </div>
                  </div>
                  <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 24, display: 'flex', gap: 16 }}>
                     <div style={{ width: 120, height: 80, background: '#1e293b', borderRadius: 8, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                        <BookOpen size={24} color={theme.textMuted} />
                     </div>
                     <div>
                        <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 8 }}>Official Documentation</div>
                        <div style={{ fontSize: 13, color: theme.textMuted, marginBottom: 12 }}>Browse the complete API reference and architectural guides.</div>
                        <div style={{ fontSize: 12, color: theme.primary, cursor: 'pointer' }}>Read Docs ➔</div>
                     </div>
                  </div>
                </div>
              </motion.div>
            )}

            {activeTab === 'settings' && (
              <motion.div key="settings" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <h3 style={{ margin: '0 0 24px 0', fontSize: 18 }}>启动器设置</h3>
                <div style={{ display: 'flex', flexDirection: 'column', gap: 24 }}>
                  <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 24 }}>
                    <h4 style={{ margin: '0 0 16px 0', display: 'flex', alignItems: 'center', gap: 8 }}><Settings size={18} /> 常规</h4>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 16 }}>
                      <div>
                        <div style={{ fontWeight: 600 }}>开机自启</div>
                        <div style={{ fontSize: 12, color: theme.textMuted }}>在系统启动时自动运行 Nexus Launcher</div>
                      </div>
                      <div style={{ width: 40, height: 20, background: theme.primary, borderRadius: 10, position: 'relative' }}>
                        <div style={{ width: 16, height: 16, background: '#fff', borderRadius: '50%', position: 'absolute', top: 2, right: 2 }}></div>
                      </div>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div>
                        <div style={{ fontWeight: 600 }}>语言</div>
                        <div style={{ fontSize: 12, color: theme.textMuted }}>界面显示语言</div>
                      </div>
                      <select style={{ background: theme.bg, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '6px 12px', borderRadius: 6 }}>
                        <option>简体中文</option>
                        <option>English</option>
                      </select>
                    </div>
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>
        </main>

        {/* Launch Bar */}
        <div style={{ height: 80, borderTop: `1px dashed ${theme.border}`, background: theme.card, display: 'flex', alignItems: 'center', padding: '0 32px', justifyContent: 'space-between', zIndex: 10, boxShadow: '0 -2px 10px rgba(0,0,0,0.02)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 24 }}>
            <div>
              <div style={{ fontSize: 12, color: theme.textMuted, marginBottom: 4 }}>点选法器</div>
              <div style={{ fontWeight: 600, fontSize: 15, color: theme.textMain }}>{projects.find(p => p.path === selectedProjectPath)?.name || '未有定夺'}</div>
            </div>
            <div style={{ height: 30, width: 1, borderLeft: `1px dashed ${theme.border}` }}></div>
            <div>
              <div style={{ fontSize: 12, color: theme.textMuted, marginBottom: 4 }}>引擎命门</div>
              <div style={{ fontWeight: 600, fontSize: 15, color: theme.textMain }}>{selectedVersion || '尚缺真火'}</div>
            </div>
          </div>

          <button
            onClick={launch}
            disabled={!canLaunch}
            style={{
              background: canLaunch ? theme.primary : theme.border,
              color: canLaunch ? '#fff' : theme.textMuted,
              border: canLaunch ? `1px solid ${theme.primary}` : 'none',
              padding: '0 40px',
              height: 48,
              fontSize: 18,
              fontWeight: 700,
              letterSpacing: 4,
              display: 'flex',
              alignItems: 'center',
              gap: 12,
              cursor: canLaunch ? 'pointer' : 'not-allowed',
              boxShadow: canLaunch ? `3px 3px 0px rgba(140, 58, 58, 0.3)` : 'none',
              transition: 'all 0.2s',
              fontFamily: 'inherit'
            }}
          >
            {canLaunch ? <Zap size={20} fill="#fff" /> : <Play size={20} />}
            {canLaunch ? '起阵 (LAUNCH)' : '蛰伏'}
          </button>
        </div>
      </div>
    </div>
  );
}