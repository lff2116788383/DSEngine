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
  bg: '#18181c', // 深色主背景
  sidebar: '#111114', // 侧边栏背景
  card: '#222228', // 卡片背景
  cardHover: '#2a2a32', // 卡片悬停
  primary: '#00a8ff', // 科技蓝
  primaryDim: 'rgba(0, 168, 255, 0.15)',
  secondary: '#8a2be2', // 霓虹紫点缀
  textMain: '#e2e2e2', // 主要文本
  textMuted: '#8b8b9b', // 次要文本
  border: '#2d2d36', // 边框
  success: '#00e676', // 成功绿
  danger: '#ff4757', // 危险红
  warning: '#fffa65' // 警告黄
};

const translations = {
  en: {
    tabs: {
      projects: 'Projects',
      engines: 'Installs',
      learn: 'Learn',
      store: 'Store',
      dashboard: 'News',
      settings: 'Settings'
    },
    user: {
      role: 'Developer',
      status: 'Online'
    },
    search: 'Search...',
    dashboard: {
      syncTitle: 'Cloud Sync',
      syncValue: 'Synced',
      syncSub: 'Just now',
      projectsTitle: 'Local Projects',
      projectsSub: 'In Workspace',
      assetsTitle: 'Assets Owned',
      assetsSub: 'Store items',
      newsTitle: 'Latest News',
      recentProjects: 'Recent Projects'
    },
    projects: {
      title: 'Projects',
      subtitle: 'Manage and launch your engine projects',
      newBtn: 'New',
      scanBtn: 'Scan Directory',
      workspace: 'Workspace:',
      notFound: 'No Projects Found',
      notFoundHint: 'Scan a directory or create a new project to get started.',
      updated: 'Updated',
      none: 'None'
    },
    engines: {
      title: 'Installs',
      subtitle: 'Manage editor versions and components',
      notFound: 'No engine versions available.',
      installed: 'INSTALLED',
      downloading: 'DOWNLOADING',
      available: 'AVAILABLE',
      cloudRelease: 'Cloud Release',
      selected: 'SELECTED',
      installBtn: 'Install',
      version: 'Engine Version',
      notInstalled: 'Not Installed'
    },
    store: {
      title: 'Asset Store',
      subtitle: 'Discover high-quality assets from official and community creators',
      by: 'by',
      getBtn: 'Get'
    },
    learn: {
      title: 'Learn',
      gettingStarted: 'Getting Started with DSEngine',
      gettingStartedDesc: 'Learn the basics of the editor, scene management, and basic scripting.',
      watchTutorial: 'Watch Tutorial ➔',
      officialDocs: 'Official Documentation',
      officialDocsDesc: 'Browse the complete API reference and architectural guides.',
      readDocs: 'Read Docs ➔'
    },
    settings: {
      title: 'Settings',
      general: 'General',
      startup: 'Launch on Startup',
      startupDesc: 'Automatically start DSEngine Dashboard when computer starts',
      language: 'Language',
      languageDesc: 'Interface display language'
    },
    launch: {
      selectedProject: 'Selected Project',
      launchBtn: 'LAUNCH',
      waitingBtn: 'WAITING'
    }
  },
  zh: {
    tabs: {
      projects: '项目',
      engines: '引擎',
      learn: '学习',
      store: '商城',
      dashboard: '动态',
      settings: '设置'
    },
    user: {
      role: '开发者',
      status: '在线'
    },
    search: '搜索...',
    dashboard: {
      syncTitle: '云端同步',
      syncValue: '已同步',
      syncSub: '刚刚',
      projectsTitle: '本地项目',
      projectsSub: '工作区内',
      assetsTitle: '已购资产',
      assetsSub: '商城物品',
      newsTitle: '最新动态',
      recentProjects: '最近项目'
    },
    projects: {
      title: '项目',
      subtitle: '管理并启动您的引擎项目',
      newBtn: '新建',
      scanBtn: '扫描目录',
      workspace: '工作区:',
      notFound: '未找到项目',
      notFoundHint: '请扫描目录或新建项目以开始。',
      updated: '更新于',
      none: '无'
    },
    engines: {
      title: '引擎版本',
      subtitle: '管理编辑器版本及组件',
      notFound: '暂无可用引擎版本。',
      installed: '已安装',
      downloading: '下载中',
      available: '可安装',
      cloudRelease: '云端发布版',
      selected: '当前选择',
      installBtn: '安装',
      version: '引擎版本',
      notInstalled: '未安装'
    },
    store: {
      title: '资源商城',
      subtitle: '发现来自官方与社区创作者的高质量资产',
      by: '作者：',
      getBtn: '获取'
    },
    learn: {
      title: '学习中心',
      gettingStarted: 'DSEngine 入门指南',
      gettingStartedDesc: '学习编辑器的基本操作、场景管理和基础脚本编写。',
      watchTutorial: '观看教程 ➔',
      officialDocs: '官方文档',
      officialDocsDesc: '浏览完整的 API 参考和架构指南。',
      readDocs: '阅读文档 ➔'
    },
    settings: {
      title: '设置',
      general: '常规',
      startup: '开机启动',
      startupDesc: '计算机启动时自动运行 DSEngine Dashboard',
      language: '语言',
      languageDesc: '界面显示语言'
    },
    launch: {
      selectedProject: '当前项目',
      launchBtn: '启动',
      waitingBtn: '等待中'
    }
  }
};

const tabsConfig = [
  { id: 'projects', icon: Box },
  { id: 'engines', icon: Cpu },
  { id: 'learn', icon: BookOpen },
  { id: 'store', icon: ShoppingCart },
  { id: 'dashboard', icon: Activity },
  { id: 'settings', icon: Settings },
];

export function LauncherApp() {
  const [lang, setLang] = useState<'en' | 'zh'>('zh');
  const t = translations[lang];

  const tabs = tabsConfig.map(tab => ({
    ...tab,
    label: t.tabs[tab.id as keyof typeof t.tabs]
  }));
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
    try {
      const selected = await window.launcherAPI.chooseProjectRoot();
      if (!selected) return;
      setProjectRoot(selected);
      const items = await window.launcherAPI.scanProjects(selected);
      setProjects(items);
      const nextSelectedPath = items.find((item) => item.path === selectedProjectPath)?.path ?? items[0]?.path ?? '';
      setSelectedProjectPath(nextSelectedPath);
      setStatus(`Scanned ${items.length} projects`);
    } catch (err) {
      setStatus(`Failed to scan projects: ${String(err)}`);
    }
  };

  const launch = async () => {
    if (!window.launcherAPI || !canLaunch) return;
    setStatus('Initializing launch sequence...');
    try {
      const result = await window.launcherAPI.launchEditor(selectedProjectPath, selectedVersion);
      setStatus(result.success ? 'Editor deployed successfully' : 'Launch sequence failed');
    } catch (err) {
      setStatus(`Launch sequence failed: ${String(err)}`);
    }
  };

  const filteredProjects = projects.filter(p =>
    p.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  useEffect(() => {
    if (filteredProjects.length === 0) {
      if (!projects.some((project) => project.path === selectedProjectPath)) {
        setSelectedProjectPath('');
      }
      return;
    }

    if (!filteredProjects.some((project) => project.path === selectedProjectPath)) {
      setSelectedProjectPath(filteredProjects[0].path);
    }
  }, [filteredProjects, projects, selectedProjectPath]);

  return (
    <div style={{ display: 'flex', height: '100vh', width: '100vw', background: theme.bg, color: theme.textMain, fontFamily: '"Inter", -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif', overflow: 'hidden' }}>
      {/* Sidebar */}
      <div style={{ width: 240, background: theme.sidebar, borderRight: `1px solid ${theme.border}`, display: 'flex', flexDirection: 'column', position: 'relative' }}>
        <div style={{ padding: '32px 20px', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 16 }}>
          <div style={{ width: 64, height: 64, borderRadius: 16, background: `linear-gradient(135deg, ${theme.primary}, ${theme.secondary})`, display: 'flex', alignItems: 'center', justifyContent: 'center', position: 'relative', boxShadow: `0 8px 16px ${theme.primaryDim}` }}>
            <Zap size={32} color="#fff" />
          </div>
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: 20, fontWeight: 800, letterSpacing: 1, color: '#fff' }}>DSEngine</div>
            <div style={{ fontSize: 11, color: theme.primary, letterSpacing: 2, marginTop: 4, fontWeight: 600 }}>DASHBOARD</div>
          </div>
        </div>

        <nav style={{ flex: 1, padding: '10px 12px', display: 'flex', flexDirection: 'column', gap: 4 }}>
          {tabs.map((tab) => {
            const isActive = activeTab === tab.id;
            const Icon = tab.icon;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                style={{
                  display: 'flex', alignItems: 'center', gap: 12, padding: '10px 16px', 
                  border: 'none', borderRadius: 8,
                  background: isActive ? theme.primaryDim : 'transparent',
                  color: isActive ? theme.primary : theme.textMuted,
                  cursor: 'pointer', transition: 'all 0.2s', textAlign: 'left', fontSize: 14, fontWeight: isActive ? 600 : 500,
                  position: 'relative'
                }}
                onMouseEnter={(e) => { if(!isActive) e.currentTarget.style.color = theme.textMain; }}
                onMouseLeave={(e) => { if(!isActive) e.currentTarget.style.color = theme.textMuted; }}
              >
                <Icon size={18} />
                {tab.label}
              </button>
            );
          })}
        </nav>

        {/* User Auth Profile */}
        <div style={{ padding: '16px 20px', borderTop: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', gap: 12, cursor: 'pointer', transition: 'background 0.2s' }}
             onMouseEnter={(e) => e.currentTarget.style.background = theme.cardHover}
             onMouseLeave={(e) => e.currentTarget.style.background = 'transparent'}>
          <div style={{ width: 36, height: 36, borderRadius: '50%', background: theme.card, display: 'flex', alignItems: 'center', justifyContent: 'center', border: `1px solid ${theme.primary}` }}>
            <User size={18} color={theme.primary} />
          </div>
          <div style={{ flex: 1, overflow: 'hidden' }}>
            <div style={{ fontSize: 14, fontWeight: 600, color: theme.textMain }}>{t.user.role}</div>
            <div style={{ fontSize: 11, color: theme.textMuted, display: 'flex', alignItems: 'center', gap: 4, marginTop: 2 }}>
              <div style={{ width: 6, height: 6, borderRadius: '50%', background: theme.success }}></div> {t.user.status}
            </div>
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative', background: `radial-gradient(circle at top right, ${theme.primaryDim}, transparent 40%)` }}>
        {/* Top bar */}
        <header style={{ height: 60, borderBottom: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', padding: '0 24px', justifyContent: 'space-between', background: 'rgba(24, 24, 28, 0.8)', backdropFilter: 'blur(10px)' }}>
          <div style={{ fontSize: 18, fontWeight: 600, color: '#fff' }}>
            {tabs.find(t => t.id === activeTab)?.label}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 20 }}>
            <div style={{ position: 'relative' }}>
              <Search size={16} color={theme.textMuted} style={{ position: 'absolute', left: 12, top: 10 }} />
              <input
                type="text"
                placeholder={t.search}
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{
                  background: theme.cardHover, border: `1px solid ${theme.border}`, borderRadius: 6,
                  padding: '6px 16px 6px 36px', color: theme.textMain, outline: 'none', width: 200,
                  transition: 'all 0.2s', fontSize: 13
                }}
                onFocus={(e) => { e.target.style.borderColor = theme.primary; e.target.style.boxShadow = `0 0 0 2px ${theme.primaryDim}`; }}
                onBlur={(e) => { e.target.style.borderColor = theme.border; e.target.style.boxShadow = 'none'; }}
              />
            </div>
            
            {/* Global Notification & Downloads */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 16, borderLeft: `1px solid ${theme.border}`, paddingLeft: 20 }}>
              <div style={{ position: 'relative', cursor: 'pointer' }} title="Downloads">
                <Download size={18} color={theme.textMuted} />
              </div>
              <div style={{ position: 'relative', cursor: 'pointer' }} title="Notifications">
                <Bell size={18} color={theme.textMuted} />
                <div style={{ position: 'absolute', top: 0, right: 0, width: 6, height: 6, background: theme.primary, borderRadius: '50%' }}></div>
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
                    { title: t.dashboard.syncTitle, value: t.dashboard.syncValue, sub: t.dashboard.syncSub, icon: Cloud, color: theme.primary },
                    { title: t.dashboard.projectsTitle, value: projects.length.toString(), sub: t.dashboard.projectsSub, icon: Box, color: theme.secondary },
                    { title: t.dashboard.assetsTitle, value: '142', sub: t.dashboard.assetsSub, icon: ShoppingCart, color: theme.success }
                  ].map((stat, i) => (
                    <div key={i} style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 20, position: 'relative', overflow: 'hidden' }}>
                      <div style={{ position: 'absolute', top: -10, right: -10, opacity: 0.1 }}>
                        <stat.icon size={100} color={stat.color} />
                      </div>
                      <div style={{ color: theme.textMuted, fontSize: 12, fontWeight: 600, textTransform: 'uppercase', letterSpacing: 1, marginBottom: 8 }}>{stat.title}</div>
                      <div style={{ fontSize: 28, fontWeight: 700, marginBottom: 4, color: '#fff' }}>{stat.value}</div>
                      <div style={{ fontSize: 12, color: theme.textMuted }}>{stat.sub}</div>
                    </div>
                  ))}
                </div>
                
                <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: 24 }}>
                  <div>
                    <h3 style={{ marginBottom: 16, fontWeight: 600, color: '#fff', fontSize: 16 }}>{t.dashboard.newsTitle}</h3>
                    <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 20 }}>
                      <div style={{ display: 'flex', gap: 16, alignItems: 'flex-start', borderBottom: `1px solid ${theme.border}`, paddingBottom: 16, marginBottom: 16 }}>
                        <div style={{ background: theme.primaryDim, padding: 10, borderRadius: 8, color: theme.primary }}><MonitorPlay size={20} /></div>
                        <div>
                          <div style={{ fontWeight: 600, marginBottom: 4, fontSize: 15, color: '#fff' }}>DSEngine v1.3.0 Beta is here</div>
                          <div style={{ color: theme.textMuted, fontSize: 13, lineHeight: 1.5 }}>Featuring the new Vulkan render backend and improved physics deterministic simulation.</div>
                        </div>
                      </div>
                      <div style={{ display: 'flex', gap: 16, alignItems: 'flex-start' }}>
                        <div style={{ background: 'rgba(138, 43, 226, 0.15)', padding: 10, borderRadius: 8, color: theme.secondary }}><Star size={20} /></div>
                        <div>
                          <div style={{ fontWeight: 600, marginBottom: 4, fontSize: 15, color: '#fff' }}>Asset Store Spring Sale</div>
                          <div style={{ color: theme.textMuted, fontSize: 13, lineHeight: 1.5 }}>Get up to 50% off on premium environments and blueprint templates.</div>
                        </div>
                      </div>
                    </div>
                  </div>
                  <div>
                    <h3 style={{ marginBottom: 16, fontWeight: 600, color: '#fff', fontSize: 16 }}>{t.dashboard.recentProjects}</h3>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                      {projects.slice(0, 3).map(p => (
                        <div key={p.path} onClick={() => { setActiveTab('projects'); setSelectedProjectPath(p.path); }} style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 8, padding: 12, display: 'flex', alignItems: 'center', gap: 12, cursor: 'pointer', transition: 'background 0.2s' }}
                             onMouseEnter={(e) => e.currentTarget.style.background = theme.cardHover}
                             onMouseLeave={(e) => e.currentTarget.style.background = theme.card}>
                          <div style={{ width: 36, height: 36, borderRadius: 8, background: theme.primaryDim, display: 'flex', alignItems: 'center', justifyContent: 'center', color: theme.primary }}>
                            <Box size={18} />
                          </div>
                          <div style={{ flex: 1, overflow: 'hidden' }}>
                            <div style={{ fontSize: 14, fontWeight: 600, color: '#fff' }}>{p.name}</div>
                            <div style={{ fontSize: 11, color: theme.textMuted, whiteSpace: 'nowrap', textOverflow: 'ellipsis', overflow: 'hidden' }}>{p.path}</div>
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
                    <h3 style={{ margin: '0 0 4px 0', fontSize: 20, color: '#fff' }}>{t.projects.title}</h3>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>{t.projects.subtitle}</div>
                  </div>
                  <div style={{ display: 'flex', gap: 12 }}>
                    <button style={{ background: theme.card, color: '#fff', border: `1px solid ${theme.border}`, borderRadius: 6, padding: '8px 16px', fontSize: 13, fontWeight: 600, display: 'flex', alignItems: 'center', gap: 8, cursor: 'pointer', transition: 'all 0.2s' }}
                            onMouseEnter={e => e.currentTarget.style.borderColor = theme.textMuted}
                            onMouseLeave={e => e.currentTarget.style.borderColor = theme.border}>
                      <Plus size={16} /> {t.projects.newBtn}
                    </button>
                    <button
                      onClick={chooseRoot}
                      style={{
                        background: theme.primary, color: '#fff', border: 'none', borderRadius: 6, padding: '8px 16px', fontSize: 13,
                        fontWeight: 600, display: 'flex', alignItems: 'center', gap: 8, cursor: 'pointer', transition: 'background 0.2s'
                      }}
                      onMouseEnter={e => e.currentTarget.style.background = '#0097e6'}
                      onMouseLeave={e => e.currentTarget.style.background = theme.primary}
                    >
                      <Folder size={16} /> {t.projects.scanBtn}
                    </button>
                  </div>
                </div>

                {projectRoot && (
                  <div style={{ marginBottom: 24, fontSize: 12, color: theme.textMuted, display: 'flex', alignItems: 'center', gap: 8, background: theme.card, padding: '8px 12px', borderRadius: 6, width: 'fit-content' }}>
                    <Terminal size={14} />
                    {t.projects.workspace} <span style={{ color: '#fff', fontFamily: 'monospace' }}>{projectRoot}</span>
                  </div>
                )}

                {filteredProjects.length === 0 ? (
                  <div style={{ background: theme.card, border: `1px dashed ${theme.border}`, borderRadius: 12, padding: 60, textAlign: 'center' }}>
                    <Box size={48} color={theme.border} style={{ margin: '0 auto 16px' }} />
                    <div style={{ fontSize: 16, fontWeight: 600, color: '#fff', marginBottom: 8 }}>{t.projects.notFound}</div>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>{t.projects.notFoundHint}</div>
                  </div>
                ) : (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: 20 }}>
                    {filteredProjects.map((p) => {
                      const isSelected = selectedProjectPath === p.path;
                      return (
                        <motion.div
                          key={p.path}
                          whileHover={{ y: -2 }}
                          onClick={() => setSelectedProjectPath(p.path)}
                          style={{
                            background: isSelected ? theme.cardHover : theme.card,
                            border: `1px solid ${isSelected ? theme.primary : theme.border}`,
                            borderRadius: 12, overflow: 'hidden', cursor: 'pointer',
                            boxShadow: isSelected ? `0 0 0 1px ${theme.primaryDim}` : 'none',
                            position: 'relative', transition: 'all 0.2s'
                          }}
                        >
                          <div style={{ position: 'absolute', top: 12, right: 12, width: 28, height: 28, borderRadius: 6, background: 'rgba(0,0,0,0.4)', display: 'flex', alignItems: 'center', justifyContent: 'center', backdropFilter: 'blur(4px)' }} onClick={(e) => e.stopPropagation()}>
                            <MoreVertical size={16} color="#fff" />
                          </div>

                          <div style={{ height: 120, background: `linear-gradient(to bottom right, ${theme.sidebar}, #1a1a24)`, display: 'flex', alignItems: 'center', justifyContent: 'center', borderBottom: `1px solid ${theme.border}` }}>
                            <MonitorPlay size={40} color={isSelected ? theme.primary : theme.border} />
                          </div>
                          <div style={{ padding: 16 }}>
                            <div style={{ fontWeight: 600, fontSize: 15, marginBottom: 4, color: '#fff' }}>{p.name}</div>
                            <div style={{ color: theme.textMuted, fontSize: 11, fontFamily: 'monospace', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                              {p.path}
                            </div>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: 12, paddingTop: 12, borderTop: `1px solid ${theme.border}` }}>
                               <div style={{ fontSize: 11, color: theme.textMuted }}>{t.projects.updated} 2h ago</div>
                               <div style={{ fontSize: 10, background: theme.primaryDim, color: theme.primary, padding: '2px 8px', borderRadius: 4, fontWeight: 600 }}>v1.3.0</div>
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
                    <h3 style={{ margin: '0 0 4px 0', fontSize: 20, color: '#fff' }}>{t.engines.title}</h3>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>{t.engines.subtitle}</div>
                  </div>
                </div>

                <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                  {versions.length === 0 ? (
                    <div style={{ color: theme.textMuted, padding: 20 }}>{t.engines.notFound}</div>
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
                            <div style={{ width: 48, height: 48, borderRadius: 12, background: theme.primaryDim, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                              <Cpu size={24} color={theme.primary} />
                            </div>
                            <div style={{ flex: 1 }}>
                              <div style={{ fontWeight: 600, fontSize: 16, color: '#fff', display: 'flex', alignItems: 'center', gap: 8 }}>
                                {v.tag}
                                {v.available ? (
                                  <span style={{ fontSize: 10, background: 'rgba(0, 230, 118, 0.15)', color: theme.success, padding: '2px 6px', borderRadius: 4, fontWeight: 600 }}>{t.engines.installed}</span>
                                ) : v.downloading ? (
                                  <span style={{ fontSize: 10, background: theme.primaryDim, color: theme.primary, padding: '2px 6px', borderRadius: 4, fontWeight: 600 }}>{t.engines.downloading}</span>
                                ) : (
                                  <span style={{ fontSize: 10, background: 'rgba(255, 71, 87, 0.15)', color: theme.danger, padding: '2px 6px', borderRadius: 4, fontWeight: 600 }}>{t.engines.available}</span>
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
                                  {v.executable || t.engines.cloudRelease}
                                </div>
                              )}
                            </div>
                          </div>
                          {isSelected && <div style={{ color: theme.primary, fontWeight: 600, fontSize: 13, background: theme.primaryDim, padding: '4px 12px', borderRadius: 12 }}>{t.engines.selected}</div>}
                          {!v.available && !v.downloading && (
                             <button style={{ background: theme.primary, border: 'none', color: '#fff', padding: '6px 16px', borderRadius: 6, display: 'flex', alignItems: 'center', gap: 6, cursor: 'pointer', fontSize: 13, fontWeight: 600 }}>
                               <Download size={14} /> {t.engines.installBtn}
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
                    <h3 style={{ margin: '0 0 4px 0', fontSize: 20, color: '#fff' }}>{t.store.title}</h3>
                    <div style={{ color: theme.textMuted, fontSize: 13 }}>{t.store.subtitle}</div>
                  </div>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(240px, 1fr))', gap: 20 }}>
                   {[
                     { name: 'Neon City Environment', author: 'DSEngine Official', price: '$29.99', tag: '3D Model' },
                     { name: 'Advanced TPS Controller', author: 'Studio X', price: '$14.99', tag: 'Blueprint' },
                     { name: 'Sci-fi UI Framework', author: 'UI Masters', price: 'Free', tag: 'UI' },
                     { name: 'Dynamic Weather System', author: 'NatureFX', price: '$49.99', tag: 'Plugin' },
                   ].map((item, i) => (
                     <div key={i} style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, overflow: 'hidden', cursor: 'pointer', transition: 'all 0.2s' }}
                          onMouseEnter={e => e.currentTarget.style.transform = 'translateY(-4px)'}
                          onMouseLeave={e => e.currentTarget.style.transform = 'translateY(0)'}>
                        <div style={{ height: 140, background: `linear-gradient(45deg, #1e293b, ${theme.sidebar})`, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                           <ShoppingCart size={32} color={theme.textMuted} opacity={0.3} />
                        </div>
                        <div style={{ padding: 16 }}>
                           <div style={{ fontSize: 10, color: theme.secondary, marginBottom: 4, fontWeight: 600 }}>{item.tag}</div>
                           <div style={{ fontWeight: 600, fontSize: 15, marginBottom: 4, color: '#fff' }}>{item.name}</div>
                           <div style={{ fontSize: 12, color: theme.textMuted, marginBottom: 12 }}>{t.store.by} {item.author}</div>
                           <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                              <div style={{ fontWeight: 700, color: '#fff' }}>{item.price}</div>
                              <div style={{ background: theme.primary, color: '#fff', padding: '4px 16px', borderRadius: 6, fontSize: 12, fontWeight: 600 }}>{t.store.getBtn}</div>
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
                <h3 style={{ margin: '0 0 24px 0', fontSize: 20, color: '#fff' }}>{t.learn.title}</h3>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 20 }}>
                  <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 24, display: 'flex', gap: 16, cursor: 'pointer', transition: 'all 0.2s' }}
                       onMouseEnter={e => e.currentTarget.style.borderColor = theme.primary}
                       onMouseLeave={e => e.currentTarget.style.borderColor = theme.border}>
                     <div style={{ width: 120, height: 80, background: theme.sidebar, borderRadius: 8, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                        <Play size={24} color={theme.textMuted} />
                     </div>
                     <div>
                        <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 8, color: '#fff' }}>{t.learn.gettingStarted}</div>
                        <div style={{ fontSize: 13, color: theme.textMuted, marginBottom: 12 }}>{t.learn.gettingStartedDesc}</div>
                        <div style={{ fontSize: 12, color: theme.primary, fontWeight: 600 }}>{t.learn.watchTutorial}</div>
                     </div>
                  </div>
                  <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 24, display: 'flex', gap: 16, cursor: 'pointer', transition: 'all 0.2s' }}
                       onMouseEnter={e => e.currentTarget.style.borderColor = theme.primary}
                       onMouseLeave={e => e.currentTarget.style.borderColor = theme.border}>
                     <div style={{ width: 120, height: 80, background: theme.sidebar, borderRadius: 8, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                        <BookOpen size={24} color={theme.textMuted} />
                     </div>
                     <div>
                        <div style={{ fontWeight: 600, fontSize: 16, marginBottom: 8, color: '#fff' }}>{t.learn.officialDocs}</div>
                        <div style={{ fontSize: 13, color: theme.textMuted, marginBottom: 12 }}>{t.learn.officialDocsDesc}</div>
                        <div style={{ fontSize: 12, color: theme.primary, fontWeight: 600 }}>{t.learn.readDocs}</div>
                     </div>
                  </div>
                </div>
              </motion.div>
            )}

            {activeTab === 'settings' && (
              <motion.div key="settings" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -10 }}>
                <h3 style={{ margin: '0 0 24px 0', fontSize: 20, color: '#fff' }}>{t.settings.title}</h3>
                <div style={{ display: 'flex', flexDirection: 'column', gap: 24 }}>
                  <div style={{ background: theme.card, border: `1px solid ${theme.border}`, borderRadius: 12, padding: 24 }}>
                    <h4 style={{ margin: '0 0 16px 0', display: 'flex', alignItems: 'center', gap: 8, color: '#fff' }}><Settings size={18} /> {t.settings.general}</h4>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 16 }}>
                      <div>
                        <div style={{ fontWeight: 600, color: '#fff' }}>{t.settings.startup}</div>
                        <div style={{ fontSize: 12, color: theme.textMuted }}>{t.settings.startupDesc}</div>
                      </div>
                      <div style={{ width: 40, height: 20, background: theme.primary, borderRadius: 10, position: 'relative', cursor: 'pointer' }}>
                        <div style={{ width: 16, height: 16, background: '#fff', borderRadius: '50%', position: 'absolute', top: 2, right: 2 }}></div>
                      </div>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div>
                        <div style={{ fontWeight: 600, color: '#fff' }}>{t.settings.language}</div>
                        <div style={{ fontSize: 12, color: theme.textMuted }}>{t.settings.languageDesc}</div>
                      </div>
                      <select 
                        value={lang}
                        onChange={(e) => setLang(e.target.value as 'en' | 'zh')}
                        style={{ background: theme.sidebar, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '8px 12px', borderRadius: 6, outline: 'none' }}
                      >
                        <option value="en">English</option>
                        <option value="zh">简体中文</option>
                      </select>
                    </div>
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>
        </main>

        {/* Launch Bar */}
        <div style={{ height: 80, borderTop: `1px solid ${theme.border}`, background: 'rgba(34, 34, 40, 0.95)', display: 'flex', alignItems: 'center', padding: '0 32px', justifyContent: 'space-between', zIndex: 10, backdropFilter: 'blur(10px)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 24 }}>
            <div>
              <div style={{ fontSize: 11, color: theme.textMuted, marginBottom: 4, textTransform: 'uppercase', letterSpacing: 1 }}>{t.launch.selectedProject}</div>
              <div style={{ fontWeight: 600, fontSize: 15, color: '#fff' }}>{projects.find(p => p.path === selectedProjectPath)?.name || t.projects.none}</div>
            </div>
            <div style={{ height: 30, width: 1, borderLeft: `1px solid ${theme.border}` }}></div>
            <div>
              <div style={{ fontSize: 11, color: theme.textMuted, marginBottom: 4, textTransform: 'uppercase', letterSpacing: 1 }}>{t.engines.version}</div>
              <div style={{ fontWeight: 600, fontSize: 15, color: '#fff' }}>{selectedVersion || t.engines.notInstalled}</div>
            </div>
          </div>

          <button
            onClick={launch}
            disabled={!canLaunch}
            style={{
              background: canLaunch ? theme.primary : theme.sidebar,
              color: canLaunch ? '#fff' : theme.textMuted,
              border: 'none',
              borderRadius: 8,
              padding: '0 40px',
              height: 48,
              fontSize: 16,
              fontWeight: 700,
              letterSpacing: 1,
              display: 'flex',
              alignItems: 'center',
              gap: 12,
              cursor: canLaunch ? 'pointer' : 'not-allowed',
              transition: 'all 0.2s',
              fontFamily: 'inherit'
            }}
            onMouseEnter={e => { if(canLaunch) e.currentTarget.style.background = '#0097e6'; }}
            onMouseLeave={e => { if(canLaunch) e.currentTarget.style.background = theme.primary; }}
          >
            {canLaunch ? <Zap size={20} fill="#fff" /> : <Play size={20} />}
            {canLaunch ? t.launch.launchBtn : t.launch.waitingBtn}
          </button>
        </div>
      </div>
    </div>
  );
}