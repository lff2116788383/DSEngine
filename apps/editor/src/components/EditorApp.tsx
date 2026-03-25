import React, { useEffect, useRef, useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { Folder, Cpu, Settings, Play, Activity, Box, Globe, Plus, Search, MonitorPlay, Terminal, Zap, Radio, User, ShoppingCart, BookOpen, Download, MoreVertical, Bell, Cloud, Star, ChevronRight, CheckCircle2, MousePointer2, Move, RotateCw, Scaling, Pause, AlignLeft, Layers, PaintBucket, Hammer, Wrench, Trash2, Import, Image as ImageIcon } from 'lucide-react';

// Declare the electron API we injected via preload
declare global {
  interface Window {
    electronAPI: {
      getEngineVersion: () => Promise<string>;
      initEngine: () => Promise<any>;
      getFrameBuffer: () => Promise<Uint8Array>;
      getFrameInfo: () => Promise<{ width: number, height: number, source: string }>;
      pushExternalFrame: (frameBuffer: Uint8Array, width: number, height: number) => Promise<boolean>;
      clearExternalFrame: () => Promise<boolean>;
      getEntities: () => Promise<any[]>;
      updateEntityTransform: (id: number, x: number, y: number, z: number) => Promise<boolean>;
      tickEngine: (deltaTime: number) => Promise<boolean>;
      importTexture: (filePath: string) => Promise<{ success: boolean, handle?: number, path?: string, error?: string }>;
      listImportedTextures: () => Promise<TextureAsset[]>;
      applyTextureToEntity: (entityId: number, textureHandle: number) => Promise<boolean>;
      listShaderVariants: () => Promise<string[]>;
      createMaterialInstance: (name: string, shaderVariant: string, textureHandle: number) => Promise<{ success: boolean, id?: number, error?: string }>;
      listMaterialInstances: () => Promise<MaterialInstance[]>;
      updateMaterialInstance: (materialId: number, payload: any) => Promise<boolean>;
      applyMaterialToEntity: (entityId: number, materialId: number) => Promise<boolean>;
      listMaterialHotUpdateEvents: () => Promise<Array<{ sequence: number, materialId: number, shaderVariant: string, blendMode: number, textureHandle: number }>>;
      clearMaterialHotUpdateEvents: () => Promise<boolean>;
      replayMaterialHotUpdates: (maxSequence?: number) => Promise<{ success: boolean, applied: number }>;
      getFrameBridgeStats: () => Promise<FrameBridgeStats>;
      pickTextureFile: () => Promise<string>;
      pickEntity: (x: number, y: number) => Promise<number>;
      createEntity: () => Promise<number>;
      deleteEntity: (id: number) => Promise<boolean>;
      buildProject: (target: string) => Promise<any>;
      getLaunchContext: () => Promise<{ projectPath: string, engineVersion: string }>;
      setRuntimePlay: (enabled: boolean) => Promise<{ running: boolean }>;
      getRuntimePlayState: () => Promise<{ running: boolean }>;
    };
  }
}

interface EntityData {
  id: number;
  name: string;
  position: { x: number, y: number, z: number };
  textureHandle?: number;
  materialInstanceId?: number;
  shaderVariant?: string;
  blendMode?: number;
  uv?: [number, number, number, number];
  has_particle?: boolean;
}

interface TextureAsset {
  handle: number;
  path: string;
}

interface MaterialInstance {
  id: number;
  name: string;
  shaderVariant: string;
  blendMode: number;
  textureHandle: number;
  tint: [number, number, number, number];
  uv: [number, number, number, number];
}

interface FrameBridgeStats {
  copyMs: number;
  latencyMs: number;
  throughputMBps: number;
  frameId: number;
  droppedFrames: number;
  drawCalls: number;
  maxBatchSprites: number;
  spriteCount: number;
  entityCount: number;
  physicsBodies: number;
}

// Godot-like Theme Constants
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
  warning: '#b8860b', // 藤黄
  headerBg: '#f5ebdb'
};

export const EditorApp: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [engineVersion, setEngineVersion] = useState<string>('Loading...');
  const [entities, setEntities] = useState<EntityData[]>([]);
  const [selectedEntity, setSelectedEntity] = useState<EntityData | null>(null);
  const [activeTab, setActiveTab] = useState<'inspector' | 'node' | 'build'>('inspector');
  const [bottomTab, setBottomTab] = useState<'output' | 'debugger' | 'audio'>('output');
  const [workspace, setWorkspace] = useState<'2D' | '3D' | 'Script' | 'AssetLib'>('2D');
  const [isPlaying, setIsPlaying] = useState(false);
  const [activeTool, setActiveTool] = useState<'select' | 'move' | 'rotate' | 'scale'>('move');
  const [outputLogs, setOutputLogs] = useState<string[]>(['Engine initialized. Bridge connected.']);
  const [frameInfo, setFrameInfo] = useState<{ width: number, height: number, source: string }>({ width: 800, height: 600, source: 'bridge' });
  const [importedTextures, setImportedTextures] = useState<TextureAsset[]>([]);
  const [selectedTextureHandle, setSelectedTextureHandle] = useState<number>(0);
  const [shaderVariants, setShaderVariants] = useState<string[]>([]);
  const [materialInstances, setMaterialInstances] = useState<MaterialInstance[]>([]);
  const [selectedMaterialId, setSelectedMaterialId] = useState<number>(0);
  const [newMaterialName, setNewMaterialName] = useState<string>('Mat_Instance');
  const [newMaterialVariant, setNewMaterialVariant] = useState<string>('SPRITE_UNLIT');
  const [materialTint, setMaterialTint] = useState<[number, number, number, number]>([1, 1, 1, 1]);
  const [materialUv, setMaterialUv] = useState<[number, number, number, number]>([0, 0, 1, 1]);
  const [materialBlendMode, setMaterialBlendMode] = useState<number>(0);
  const [frameStats, setFrameStats] = useState<FrameBridgeStats>({ copyMs: 0, latencyMs: 0, throughputMBps: 0, frameId: 0, droppedFrames: 0, drawCalls: 0, maxBatchSprites: 0, spriteCount: 0, entityCount: 0, physicsBodies: 0 });

  // Gizmo State
  const isDragging = useRef(false);
  const lastMousePos = useRef({ x: 0, y: 0 });
  const isPlayingRef = useRef(isPlaying);
  const lastFrameTimeRef = useRef<number>(0);

  useEffect(() => {
    isPlayingRef.current = isPlaying;
  }, [isPlaying]);

  useEffect(() => {
    if (selectedEntity?.materialInstanceId) {
      setSelectedMaterialId(selectedEntity.materialInstanceId);
    }
  }, [selectedEntity]);

  useEffect(() => {
    const mat = materialInstances.find(m => m.id === selectedMaterialId);
    if (mat) {
      setMaterialTint([mat.tint[0], mat.tint[1], mat.tint[2], mat.tint[3]]);
      setMaterialUv([mat.uv[0], mat.uv[1], mat.uv[2], mat.uv[3]]);
      setMaterialBlendMode(mat.blendMode);
    }
  }, [selectedMaterialId, materialInstances]);

  useEffect(() => {
    // Check if electronAPI is available and call the C++ Addon
    if (window.electronAPI) {
      window.electronAPI.getEngineVersion().then((version) => {
        setEngineVersion(version);
      }).catch(err => {
        setEngineVersion('Error loading engine version');
        console.error(err);
      });

      window.electronAPI.initEngine().then((res) => {
        console.log("Engine init response:", res);
        setOutputLogs(prev => [...prev, "Engine initialization success."]);
        if (window.electronAPI.getLaunchContext) {
          window.electronAPI.getLaunchContext().then((ctx) => {
            setOutputLogs(prev => [...prev, `Launch context => project: ${ctx.projectPath || '<none>'}, version: ${ctx.engineVersion || 'debug'}`]);
          }).catch(() => undefined);
        }
        
        // Fetch initial entities
        if (window.electronAPI.getEntities) {
          window.electronAPI.getEntities().then((data) => {
            setEntities(data);
            setOutputLogs(prev => [...prev, `Loaded ${data.length} entities successfully.`]);
          }).catch(err => {
            console.error(err);
            setOutputLogs(prev => [...prev, `Error loading entities: ${err.message}`]);
          });
        }
        if (window.electronAPI.listImportedTextures) {
          window.electronAPI.listImportedTextures().then((textures) => {
            setImportedTextures(textures);
            if (textures.length > 0) {
              setSelectedTextureHandle(textures[0].handle);
            }
          });
        }
        if (window.electronAPI.listShaderVariants) {
          window.electronAPI.listShaderVariants().then((variants) => {
            setShaderVariants(variants);
            if (variants.length > 0) {
              setNewMaterialVariant(variants[0]);
            }
          });
        }
        if (window.electronAPI.listMaterialInstances) {
          window.electronAPI.listMaterialInstances().then((materials) => {
            setMaterialInstances(materials);
            if (materials.length > 0) {
              setSelectedMaterialId(materials[0].id);
            }
          });
        }
        
        // Start render loop
        let animationFrameId: number;
        
        const renderLoop = async (timestamp?: number) => {
          if (canvasRef.current && window.electronAPI) {
            try {
              if (isPlayingRef.current && window.electronAPI.tickEngine) {
                const currentTime = timestamp ?? performance.now();
                if (lastFrameTimeRef.current === 0) {
                  lastFrameTimeRef.current = currentTime;
                }
                const deltaTime = Math.min(0.05, Math.max(0.0, (currentTime - lastFrameTimeRef.current) / 1000.0));
                lastFrameTimeRef.current = currentTime;
                await window.electronAPI.tickEngine(deltaTime);
              } else if (timestamp) {
                lastFrameTimeRef.current = timestamp;
              }
              const buffer = await window.electronAPI.getFrameBuffer();
              const latestFrameInfo = await window.electronAPI.getFrameInfo();
              const stats = await window.electronAPI.getFrameBridgeStats();
              setFrameStats(stats);
              setFrameInfo(latestFrameInfo);
              const ctx = canvasRef.current.getContext('2d');
              if (ctx && buffer) {
                const frameView = new Uint8ClampedArray(
                  buffer.buffer as ArrayBuffer,
                  buffer.byteOffset,
                  buffer.byteLength
                );
                const imageData = new ImageData(
                  frameView,
                  latestFrameInfo.width,
                  latestFrameInfo.height
                );
                ctx.putImageData(imageData, 0, 0);
              }
            } catch (err) {
              console.error("Failed to get frame buffer:", err);
            }
          }
          animationFrameId = requestAnimationFrame(renderLoop);
        };
        
        renderLoop();

        return () => {
          cancelAnimationFrame(animationFrameId);
        };
      });
    } else {
      // Basic placeholder for the engine viewport render target if not in Electron
      if (canvasRef.current) {
        const ctx = canvasRef.current.getContext('2d');
        if (ctx) {
          ctx.fillStyle = '#2d2d2d';
          ctx.fillRect(0, 0, canvasRef.current.width, canvasRef.current.height);
          ctx.fillStyle = '#fff';
          ctx.font = '20px Arial';
          ctx.fillText('DSEngine Viewport Placeholder', 50, 50);
        }
      }
    }
  }, []);

  const handleMouseDown = async (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!canvasRef.current) return;
    const rect = canvasRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Convert screen coordinates to world coordinates
    // Assuming center of canvas (400, 300) is world (0,0), and scale is 0.05
    const worldX = (x - 400) * 0.05;
    const worldY = -(y - 300) * 0.05;

    if (window.electronAPI && window.electronAPI.pickEntity) {
      try {
        const pickedId = await window.electronAPI.pickEntity(worldX, worldY);
        if (pickedId >= 0) {
          const picked = entities.find(ent => ent.id === pickedId);
          if (picked) {
            setSelectedEntity(picked);
            isDragging.current = true;
            lastMousePos.current = { x: e.clientX, y: e.clientY };
            return;
          }
        } else {
          // Deselect if clicked on empty space
          setSelectedEntity(null);
          return;
        }
      } catch (err) {
        console.error("Pick entity error", err);
      }
    }

    // Fallback if no picking API
    if (!selectedEntity) return;
    isDragging.current = true;
    lastMousePos.current = { x: e.clientX, y: e.clientY };
  };

  const handleMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!isDragging.current || !selectedEntity) return;

    const dx = e.clientX - lastMousePos.current.x;
    const dy = e.clientY - lastMousePos.current.y;
    lastMousePos.current = { x: e.clientX, y: e.clientY };

    // Simple pixel-to-world mapping for Gizmo translation (mock scaling)
    const moveSpeed = 0.05; 
    
    // Update local state for immediate feedback
    const updatedPos = {
      x: selectedEntity.position.x + dx * moveSpeed,
      y: selectedEntity.position.y - dy * moveSpeed, // Invert Y for screen to world
      z: selectedEntity.position.z
    };

    const newEntity = { ...selectedEntity, position: updatedPos };
    setSelectedEntity(newEntity);
    
    // Update entities list state
    setEntities(prev => prev.map(ent => ent.id === newEntity.id ? newEntity : ent));

    // Send to C++ engine
    if (window.electronAPI.updateEntityTransform) {
      window.electronAPI.updateEntityTransform(newEntity.id, updatedPos.x, updatedPos.y, updatedPos.z);
    }
  };

  const handleMouseUp = () => {
    isDragging.current = false;
  };

  const logMessage = (msg: string) => {
    setOutputLogs(prev => [...prev, `[${new Date().toLocaleTimeString()}] ${msg}`]);
    setBottomTab('output'); // Auto switch to output
  };

  const refreshEntities = async () => {
    if (window.electronAPI && window.electronAPI.getEntities) {
      try {
        const data = await window.electronAPI.getEntities();
        setEntities(data);
        if (selectedEntity) {
          const latestSelected = data.find((ent: EntityData) => ent.id === selectedEntity.id) || null;
          setSelectedEntity(latestSelected);
        }
      } catch (err) {
        console.error(err);
      }
    }
  };

  const refreshImportedTextures = async () => {
    if (window.electronAPI && window.electronAPI.listImportedTextures) {
      try {
        const textures = await window.electronAPI.listImportedTextures();
        setImportedTextures(textures);
        if (textures.length > 0 && !textures.find(tex => tex.handle === selectedTextureHandle)) {
          setSelectedTextureHandle(textures[0].handle);
        }
      } catch (err) {
        console.error(err);
      }
    }
  };

  const refreshMaterialInstances = async () => {
    if (window.electronAPI && window.electronAPI.listMaterialInstances) {
      try {
        const materials = await window.electronAPI.listMaterialInstances();
        setMaterialInstances(materials);
        if (materials.length > 0 && !materials.find(m => m.id === selectedMaterialId)) {
          setSelectedMaterialId(materials[0].id);
        }
      } catch (err) {
        console.error(err);
      }
    }
  };

  const handleAddNode = async () => {
    if (window.electronAPI && window.electronAPI.createEntity) {
      try {
        const newId = await window.electronAPI.createEntity();
        if (newId >= 0) {
          logMessage(`Created new Node with ID: ${newId}`);
          await refreshEntities();
        } else {
          logMessage("Failed to create Node.");
        }
      } catch (err) {
        console.error(err);
      }
    } else {
      logMessage("Create Node action triggered. (Requires C++ binding implementation)");
      alert("This would open the 'Create New Node' dialog.");
    }
  };

  const handleDeleteNode = async (id: number) => {
    if (window.electronAPI && window.electronAPI.deleteEntity) {
      try {
        const success = await window.electronAPI.deleteEntity(id);
        if (success) {
          logMessage(`Deleted Node with ID: ${id}`);
          if (selectedEntity?.id === id) {
            setSelectedEntity(null);
          }
          await refreshEntities();
        } else {
          logMessage(`Failed to delete Node with ID: ${id}`);
        }
      } catch (err) {
        console.error(err);
      }
    }
  };

  const handleAddInstance = () => {
    logMessage("Instantiate Scene action triggered.");
  };

  const handlePlayToggle = () => {
    const newPlayState = !isPlaying;
    setIsPlaying(newPlayState);
    if (window.electronAPI?.setRuntimePlay) {
      window.electronAPI.setRuntimePlay(newPlayState).then((runtimeState) => {
        logMessage(newPlayState ? `Starting game preview... runtime=${runtimeState.running}` : `Stopped game preview. runtime=${runtimeState.running}`);
      }).catch(() => {
        logMessage(newPlayState ? "Starting game preview..." : "Stopped game preview.");
      });
      return;
    }
    logMessage(newPlayState ? "Starting game preview..." : "Stopped game preview.");
  };

  const handleImportTexture = async () => {
    if (!window.electronAPI) {
      return;
    }
    const filePath = await window.electronAPI.pickTextureFile();
    if (!filePath) {
      return;
    }
    const result = await window.electronAPI.importTexture(filePath);
    if (result.success && result.handle) {
      logMessage(`Imported texture: ${filePath} -> handle ${result.handle}`);
      await refreshImportedTextures();
      setSelectedTextureHandle(result.handle);
    } else {
      logMessage(`Import texture failed: ${result.error ?? 'unknown_error'}`);
    }
  };

  const handleApplyTexture = async () => {
    if (!selectedEntity || !selectedTextureHandle || !window.electronAPI) {
      return;
    }
    const success = await window.electronAPI.applyTextureToEntity(selectedEntity.id, selectedTextureHandle);
    if (success) {
      logMessage(`Applied texture ${selectedTextureHandle} to entity ${selectedEntity.name}`);
      await refreshEntities();
    } else {
      logMessage('Apply texture failed');
    }
  };

  const handleCreateMaterial = async () => {
    if (!window.electronAPI || !newMaterialName.trim()) {
      return;
    }
    const result = await window.electronAPI.createMaterialInstance(newMaterialName.trim(), newMaterialVariant, selectedTextureHandle);
    if (result.success && result.id) {
      logMessage(`Created material #${result.id} (${newMaterialVariant})`);
      await refreshMaterialInstances();
      setSelectedMaterialId(result.id);
    } else {
      logMessage(`Create material failed: ${result.error ?? 'unknown_error'}`);
    }
  };

  const handleUpdateMaterial = async () => {
    if (!selectedMaterialId || !window.electronAPI) {
      return;
    }
    const current = materialInstances.find(m => m.id === selectedMaterialId);
    if (!current) {
      return;
    }
    const success = await window.electronAPI.updateMaterialInstance(selectedMaterialId, {
      shaderVariant: current.shaderVariant,
      textureHandle: current.textureHandle,
      blendMode: materialBlendMode,
      tint: materialTint,
      uv: materialUv
    });
    if (success) {
      logMessage(`Updated material ${selectedMaterialId}`);
      await refreshMaterialInstances();
    } else {
      logMessage(`Update material ${selectedMaterialId} failed`);
    }
  };

  const handleApplyMaterial = async () => {
    if (!selectedEntity || !selectedMaterialId || !window.electronAPI) {
      return;
    }
    const success = await window.electronAPI.applyMaterialToEntity(selectedEntity.id, selectedMaterialId);
    if (success) {
      logMessage(`Applied material ${selectedMaterialId} to entity ${selectedEntity.name}`);
      await refreshEntities();
    } else {
      logMessage('Apply material failed');
    }
  };

  const handleReplayMaterialHotUpdates = async () => {
    if (!window.electronAPI || !window.electronAPI.replayMaterialHotUpdates) {
      return;
    }
    const result = await window.electronAPI.replayMaterialHotUpdates();
    if (result?.success) {
      logMessage(`Replayed material hot updates: ${result.applied}`);
      await refreshMaterialInstances();
      await refreshEntities();
    } else {
      logMessage('Replay material hot updates failed');
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100vh', width: '100vw', background: theme.bg, color: theme.textMain, fontFamily: '"STKaiti", "KaiTi", serif', overflow: 'hidden', backgroundImage: 'url("data:image/svg+xml,%3Csvg width=\'100\' height=\'100\' viewBox=\'0 0 100 100\' xmlns=\'http://www.w3.org/2000/svg\'%3E%3Cfilter id=\'noise\'%3E%3CfeTurbulence type=\'fractalNoise\' baseFrequency=\'0.8\' numOctaves=\'4\' stitchTiles=\'stitch\'/%3E%3C/filter%3E%3Crect width=\'100\' height=\'100\' filter=\'url(%23noise)\' opacity=\'0.08\'/%3E%3C/svg%3E")' }}>
      
      {/* Top Menu Bar */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 16px', height: '48px', background: theme.sidebar, borderBottom: `2px solid ${theme.primary}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', color: theme.primary, fontWeight: 'bold', fontSize: '18px', letterSpacing: '2px' }}>
            <Activity size={20} />
            <span>天工开物 (DSEngine)</span>
          </div>
          <div style={{ width: '1px', height: '24px', background: theme.border, margin: '0 8px' }}></div>
          <button style={{ background: 'transparent', border: 'none', color: theme.textMain, cursor: 'pointer', fontSize: '14px', fontFamily: 'inherit' }}>卷宗 (File)</button>
          <button style={{ background: 'transparent', border: 'none', color: theme.textMain, cursor: 'pointer', fontSize: '14px', fontFamily: 'inherit' }}>修真 (Edit)</button>
          <button style={{ background: 'transparent', border: 'none', color: theme.textMain, cursor: 'pointer', fontSize: '14px', fontFamily: 'inherit' }}>天道 (Window)</button>
        </div>
        
        {/* Play Toolbar */}
        <div style={{ display: 'flex', gap: '8px', background: theme.card, padding: '4px 8px', borderRadius: '4px', border: `1px solid ${theme.border}` }}>
          <button onClick={() => setIsPlaying(!isPlaying)} style={{ background: isPlaying ? theme.danger : 'transparent', color: isPlaying ? '#fff' : theme.success, border: 'none', borderRadius: '4px', padding: '4px 16px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '6px', fontFamily: 'inherit' }}>
            {isPlaying ? <Pause size={16} /> : <Play size={16} />}
            {isPlaying ? '暂停推演' : '开启阵法'}
          </button>
        </div>

        <div style={{ fontSize: '12px', color: theme.textMuted }}>
          版本: {engineVersion} | 帧率: {frameStats.droppedFrames === 0 ? '平稳' : '波动'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        
        {/* Left Panel: Hierarchy */}
        <div style={{ width: '280px', display: 'flex', flexDirection: 'column', borderRight: `1px solid ${theme.border}`, background: theme.sidebar }}>
          <div style={{ padding: '8px 12px', background: theme.cardHover, borderBottom: `1px solid ${theme.border}`, fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Layers size={16} color={theme.primary} />
            <span>法阵层级 (Hierarchy)</span>
          </div>
          <div style={{ padding: '8px', borderBottom: `1px dashed ${theme.border}` }}>
            <button onClick={handleAddNode} style={{ width: '100%', padding: '6px', background: '#fff', border: `1px solid ${theme.border}`, borderRadius: '4px', color: theme.primary, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', fontFamily: 'inherit' }}>
              <Plus size={14} /> 凝聚新灵体
            </button>
          </div>
          <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
            {entities.map(ent => (
              <div 
                key={ent.id} 
                onClick={() => setSelectedEntity(ent)}
                style={{
                  padding: '6px 8px', 
                  marginBottom: '4px',
                  borderRadius: '4px',
                  background: selectedEntity?.id === ent.id ? theme.primaryDim : 'transparent',
                  border: `1px solid ${selectedEntity?.id === ent.id ? theme.primary : 'transparent'}`,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  color: selectedEntity?.id === ent.id ? theme.primary : theme.textMain
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  {ent.name.includes('Camera') ? <MonitorPlay size={14} /> : <Box size={14} />}
                  <span style={{ fontSize: '14px' }}>{ent.name}</span>
                </div>
                <button onClick={(e) => { e.stopPropagation(); handleDeleteNode(ent.id); }} style={{ background: 'transparent', border: 'none', color: theme.danger, cursor: 'pointer' }}>
                  <Trash2 size={14} />
                </button>
              </div>
            ))}
          </div>
        </div>

        {/* Center: Viewport & Bottom Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', background: theme.bg }}>
          {/* Viewport Toolbar */}
          <div style={{ height: '36px', borderBottom: `1px solid ${theme.border}`, display: 'flex', alignItems: 'center', padding: '0 12px', gap: '16px', background: theme.cardHover }}>
            <div style={{ display: 'flex', gap: '4px' }}>
              {[
                { id: 'select', icon: MousePointer2, label: '点选' },
                { id: 'move', icon: Move, label: '挪移' },
                { id: 'rotate', icon: RotateCw, label: '周转' },
                { id: 'scale', icon: Scaling, label: '伸缩' }
              ].map(tool => (
                <button 
                  key={tool.id}
                  onClick={() => setActiveTool(tool.id as any)}
                  title={tool.label}
                  style={{
                    padding: '4px 8px',
                    background: activeTool === tool.id ? theme.primary : 'transparent',
                    color: activeTool === tool.id ? '#fff' : theme.textMuted,
                    border: `1px solid ${activeTool === tool.id ? theme.primary : 'transparent'}`,
                    borderRadius: '4px',
                    cursor: 'pointer'
                  }}
                >
                  <tool.icon size={16} />
                </button>
              ))}
            </div>
            <div style={{ width: '1px', height: '16px', background: theme.border }}></div>
            <div style={{ display: 'flex', gap: '8px', fontSize: '12px', color: theme.textMuted }}>
              <span>视界: {workspace === '2D' ? '两仪(2D)' : '三才(3D)'}</span>
              <span>尺寸: {frameInfo.width}x{frameInfo.height}</span>
            </div>
          </div>
          
          {/* Viewport Canvas */}
          <div style={{ flex: 1, position: 'relative', overflow: 'hidden', display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '16px' }}>
            <div style={{ 
              boxShadow: '0 8px 32px rgba(0,0,0,0.1)', 
              border: `4px solid ${theme.border}`,
              background: '#000',
              borderRadius: '4px',
              overflow: 'hidden'
            }}>
              <canvas 
                ref={canvasRef}
                width={frameInfo.width} 
                height={frameInfo.height}
                onMouseDown={handleMouseDown}
                onMouseMove={handleMouseMove}
                onMouseUp={handleMouseUp}
                onMouseLeave={handleMouseUp}
                style={{ display: 'block', cursor: activeTool === 'move' ? 'move' : 'default' }}
              />
            </div>
          </div>

          {/* Bottom Panel */}
          <div style={{ height: '200px', borderTop: `1px solid ${theme.border}`, display: 'flex', flexDirection: 'column', background: theme.sidebar }}>
            <div style={{ display: 'flex', background: theme.cardHover, borderBottom: `1px solid ${theme.border}` }}>
              {[
                { id: 'output', label: '推演日志 (Output)', icon: Terminal },
                { id: 'debugger', label: '神念探查 (Debugger)', icon: Activity },
                { id: 'audio', label: '音律 (Audio)', icon: Radio }
              ].map(tab => (
                <button
                  key={tab.id}
                  onClick={() => setBottomTab(tab.id as any)}
                  style={{
                    padding: '8px 16px',
                    background: bottomTab === tab.id ? '#fff' : 'transparent',
                    border: 'none',
                    borderRight: `1px solid ${theme.border}`,
                    borderTop: `2px solid ${bottomTab === tab.id ? theme.primary : 'transparent'}`,
                    color: bottomTab === tab.id ? theme.primary : theme.textMuted,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    fontFamily: 'inherit',
                    fontWeight: bottomTab === tab.id ? 'bold' : 'normal'
                  }}
                >
                  <tab.icon size={14} />
                  {tab.label}
                </button>
              ))}
            </div>
            <div style={{ flex: 1, overflowY: 'auto', padding: '8px', background: '#fff', color: theme.textMain, fontSize: '13px', fontFamily: 'monospace' }}>
              {bottomTab === 'output' && outputLogs.map((log, i) => (
                <div key={i} style={{ padding: '2px 0', borderBottom: '1px dashed #eee' }}>{log}</div>
              ))}
              {bottomTab === 'debugger' && (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', color: theme.textMuted }}>
                  <div>
                    <div style={{ fontWeight: 'bold', marginBottom: '8px', color: theme.primary }}>阵法开销 (Frame Stats)</div>
                    <div>绘制咒令 (Draw Calls): {frameStats.drawCalls}</div>
                    <div>同源显化 (Max Batch Sprites): {frameStats.maxBatchSprites}</div>
                    <div>灵体总数 (Entity Count): {frameStats.entityCount}</div>
                    <div>物理真身 (Physics Bodies): {frameStats.physicsBodies}</div>
                  </div>
                  <div>
                    <div style={{ fontWeight: 'bold', marginBottom: '8px', color: theme.primary }}>桥接脉络 (Bridge Stats)</div>
                    <div>延迟 (Latency): {frameStats.latencyMs.toFixed(2)} ms</div>
                    <div>拷贝耗时 (Copy): {frameStats.copyMs.toFixed(2)} ms</div>
                    <div>吞吐 (Throughput): {frameStats.throughputMBps.toFixed(2)} MB/s</div>
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Right Panel: Inspector & Assets */}
        <div style={{ width: '320px', display: 'flex', flexDirection: 'column', borderLeft: `1px solid ${theme.border}`, background: theme.sidebar }}>
          <div style={{ display: 'flex', background: theme.cardHover, borderBottom: `1px solid ${theme.border}` }}>
            <button 
              onClick={() => setActiveTab('inspector')}
              style={{ flex: 1, padding: '10px', background: activeTab === 'inspector' ? '#fff' : 'transparent', color: activeTab === 'inspector' ? theme.primary : theme.textMuted, border: 'none', borderTop: `2px solid ${activeTab === 'inspector' ? theme.primary : 'transparent'}`, fontWeight: activeTab === 'inspector' ? 'bold' : 'normal', fontFamily: 'inherit', cursor: 'pointer' }}>
              灵力波动 (Inspector)
            </button>
            <button 
              onClick={() => setActiveTab('node')}
              style={{ flex: 1, padding: '10px', background: activeTab === 'node' ? '#fff' : 'transparent', color: activeTab === 'node' ? theme.primary : theme.textMuted, border: 'none', borderTop: `2px solid ${activeTab === 'node' ? theme.primary : 'transparent'}`, fontWeight: activeTab === 'node' ? 'bold' : 'normal', fontFamily: 'inherit', cursor: 'pointer' }}>
              经脉 (Node)
            </button>
            <button 
              onClick={() => setActiveTab('build')}
              style={{ flex: 1, padding: '10px', background: activeTab === 'build' ? '#fff' : 'transparent', color: activeTab === 'build' ? theme.primary : theme.textMuted, border: 'none', borderTop: `2px solid ${activeTab === 'build' ? theme.primary : 'transparent'}`, fontWeight: activeTab === 'build' ? 'bold' : 'normal', fontFamily: 'inherit', cursor: 'pointer' }}>
              飞升 (Build)
            </button>
          </div>

          <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
            {activeTab === 'inspector' && (
              selectedEntity ? (
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', marginBottom: '16px', gap: '8px' }}>
                    <div style={{ width: '32px', height: '32px', background: theme.primary, color: '#fff', borderRadius: '4px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                      {selectedEntity.name.includes('Camera') ? <MonitorPlay size={18} /> : <Box size={18} />}
                    </div>
                    <input 
                      type="text" 
                      value={selectedEntity.name} 
                      readOnly 
                      style={{ flex: 1, background: '#fff', color: theme.textMain, border: `1px solid ${theme.border}`, padding: '6px 8px', borderRadius: '4px', fontFamily: 'inherit', fontSize: '14px' }} 
                    />
                  </div>
                  
                  {/* Transform Section */}
                  <div style={{ background: theme.cardHover, padding: '6px 12px', fontWeight: 'bold', border: `1px solid ${theme.border}`, borderRadius: '4px 4px 0 0', display: 'flex', alignItems: 'center', gap: '6px', color: theme.primary }}>
                    <Move size={14} /> 遁法位置 (Transform2D)
                  </div>
                  <div style={{ padding: '12px', background: '#fff', border: `1px solid ${theme.border}`, borderTop: 'none', borderRadius: '0 0 4px 4px', marginBottom: '16px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                      <span style={{ width: '60px', color: theme.textMuted, fontSize: '13px' }}>位移</span>
                      <div style={{ display: 'flex', flex: 1, gap: '8px' }}>
                        <div style={{ flex: 1, display: 'flex', background: theme.cardHover, border: `1px solid ${theme.border}`, borderRadius: '4px', overflow: 'hidden' }}>
                          <span style={{ padding: '4px 8px', background: theme.danger, color: '#fff', fontSize: '12px' }}>X</span>
                          <input type="text" value={selectedEntity.position.x.toFixed(2)} readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 8px', fontFamily: 'monospace' }} />
                        </div>
                        <div style={{ flex: 1, display: 'flex', background: theme.cardHover, border: `1px solid ${theme.border}`, borderRadius: '4px', overflow: 'hidden' }}>
                          <span style={{ padding: '4px 8px', background: theme.success, color: '#fff', fontSize: '12px' }}>Y</span>
                          <input type="text" value={selectedEntity.position.y.toFixed(2)} readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 8px', fontFamily: 'monospace' }} />
                        </div>
                      </div>
                    </div>
                  </div>

                  <div style={{ background: theme.cardHover, padding: '6px 12px', fontWeight: 'bold', border: `1px solid ${theme.border}`, borderRadius: '4px 4px 0 0', display: 'flex', alignItems: 'center', gap: '6px', color: theme.primary }}>
                    <ImageIcon size={14} /> 幻象显化 (SpriteRenderer)
                  </div>
                  <div style={{ padding: '12px', background: '#fff', border: `1px solid ${theme.border}`, borderTop: 'none', borderRadius: '0 0 4px 4px', fontSize: '13px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                      <span style={{ width: '60px', color: theme.textMuted }}>灵纹(Tex)</span>
                      <div style={{ flex: 1, background: theme.cardHover, border: `1px solid ${theme.border}`, padding: '4px 8px', borderRadius: '4px', color: theme.primary, fontFamily: 'monospace' }}>
                        {selectedEntity.textureHandle ? `卷轴编号: ${selectedEntity.textureHandle}` : '未附灵'}
                      </div>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '12px' }}>
                      <span style={{ width: '60px', color: theme.textMuted }}>法诀(Mat)</span>
                      <select value={selectedMaterialId} onChange={(e) => setSelectedMaterialId(Number(e.target.value))} style={{ flex: 1, background: '#fff', color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '4px', padding: '4px', fontFamily: 'inherit' }}>
                        <option value={0}>无 (None)</option>
                        {materialInstances.map(mat => (
                          <option key={mat.id} value={mat.id}>法诀#{mat.id} {mat.name}</option>
                        ))}
                      </select>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '12px' }}>
                      <span style={{ width: '60px', color: theme.textMuted }}>赋灵纹</span>
                      <select value={selectedTextureHandle} onChange={(e) => setSelectedTextureHandle(Number(e.target.value))} style={{ flex: 1, background: '#fff', color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '4px', padding: '4px', fontFamily: 'inherit' }}>
                        <option value={0}>无 (None)</option>
                        {importedTextures.map(tex => (
                          <option key={tex.handle} value={tex.handle}>卷轴#{tex.handle} {tex.path.split(/[\\/]/).pop()}</option>
                        ))}
                      </select>
                    </div>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <button onClick={handleApplyTexture} disabled={!selectedTextureHandle} style={{ flex: 1, background: selectedTextureHandle ? theme.primary : theme.cardHover, color: selectedTextureHandle ? '#fff' : theme.textMuted, border: `1px solid ${theme.border}`, padding: '6px', borderRadius: '4px', cursor: selectedTextureHandle ? 'pointer' : 'not-allowed', fontFamily: 'inherit' }}>注入灵纹</button>
                      <button onClick={handleApplyMaterial} disabled={!selectedMaterialId} style={{ flex: 1, background: selectedMaterialId ? theme.primary : theme.cardHover, color: selectedMaterialId ? '#fff' : theme.textMuted, border: `1px solid ${theme.border}`, padding: '6px', borderRadius: '4px', cursor: selectedMaterialId ? 'pointer' : 'not-allowed', fontFamily: 'inherit' }}>施加法诀</button>
                    </div>
                  </div>
                </div>
              ) : (
                <div style={{ color: theme.textMuted, textAlign: 'center', marginTop: '40px', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '12px' }}>
                  <Zap size={32} color={theme.border} />
                  <span>请先选择一个灵体节点</span>
                </div>
              )
            )}

            {activeTab === 'node' && (
              <div style={{ color: theme.textMuted, textAlign: 'center', marginTop: '40px', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '12px' }}>
                <Cloud size={32} color={theme.border} />
                <span>经脉系统暂未开放</span>
              </div>
            )}

            {activeTab === 'build' && (
              <div style={{ padding: '4px' }}>
                <div style={{ marginBottom: '16px' }}>
                  <label style={{ display: 'block', color: theme.textMuted, marginBottom: '8px', fontWeight: 'bold' }}>飞升目标界域 (Platform)</label>
                  <select id="buildTarget" style={{ width: '100%', padding: '8px', background: '#fff', color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '4px', fontFamily: 'inherit' }}>
                    <option value="win64">凡界 (Windows Desktop)</option>
                    <option value="mac">灵界 (macOS)</option>
                    <option value="wasm">虚空 (HTML5 WebAssembly)</option>
                  </select>
                </div>
                <button 
                  onClick={() => {
                    if (window.electronAPI && window.electronAPI.buildProject) {
                      const target = (document.getElementById('buildTarget') as HTMLSelectElement).value;
                      window.electronAPI.buildProject(target).then(res => {
                        alert(res.message);
                      });
                    }
                  }}
                  style={{ width: '100%', padding: '10px', background: theme.primary, color: '#fff', border: 'none', borderRadius: '4px', cursor: 'pointer', fontWeight: 'bold', fontSize: '16px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px', fontFamily: 'inherit' }}
                >
                  <Star size={18} /> 开始飞升 (Export Project)
                </button>
              </div>
            )}
          </div>
        </div>

      </div>
    </div>
  );
};
