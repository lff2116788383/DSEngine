import React, { useEffect, useRef, useState } from 'react';

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
  bgBase: '#202531',
  bgPanel: '#1a1e27',
  bgDark: '#14171f',
  textMain: '#cccccc',
  textMuted: '#888888',
  accent: '#478cbf',
  accentHover: '#416b8c',
  border: '#11141a',
  headerBg: '#242a35'
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
    <div style={{ display: 'flex', flexDirection: 'column', height: '100vh', background: theme.bgBase, color: theme.textMain, fontFamily: 'Segoe UI, Tahoma, Geneva, Verdana, sans-serif', fontSize: '13px' }}>
      
      {/* Top Menu Bar */}
      <div style={{ display: 'flex', alignItems: 'center', padding: '0 10px', height: '30px', background: theme.headerBg, borderBottom: `1px solid ${theme.border}` }}>
        <div style={{ display: 'flex', gap: '15px' }}>
          <span style={{ cursor: 'pointer' }}>Scene</span>
          <span style={{ cursor: 'pointer' }}>Project</span>
          <span style={{ cursor: 'pointer' }}>Debug</span>
          <span style={{ cursor: 'pointer' }}>Editor</span>
          <span style={{ cursor: 'pointer' }}>Help</span>
        </div>
        <div style={{ margin: '0 auto', display: 'flex', gap: '5px' }}>
          {['2D', '3D', 'Script', 'AssetLib'].map(ws => (
            <button 
              key={ws}
              onClick={() => setWorkspace(ws as any)}
              style={{
                background: workspace === ws ? theme.bgPanel : 'transparent',
                color: workspace === ws ? theme.accent : theme.textMain,
                border: 'none',
                padding: '4px 12px',
                borderRadius: '4px',
                cursor: 'pointer',
                fontWeight: workspace === ws ? 'bold' : 'normal'
              }}
            >
              {ws}
            </button>
          ))}
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <button onClick={handlePlayToggle} style={{ background: 'transparent', border: 'none', color: isPlaying ? '#4caf50' : theme.textMain, cursor: 'pointer', fontSize: '16px' }}>
            {isPlaying ? '⏹' : '▶'}
          </button>
          <button onClick={() => logMessage("Pause clicked")} style={{ background: 'transparent', border: 'none', color: theme.textMain, cursor: 'pointer', fontSize: '16px' }}>⏸</button>
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        
        {/* Left Panel: Scene & FileSystem */}
        <div style={{ width: '280px', display: 'flex', flexDirection: 'column', background: theme.bgPanel, borderRight: `1px solid ${theme.border}` }}>
          {/* Scene Tree */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', borderBottom: `1px solid ${theme.border}` }}>
            <div style={{ padding: '6px 10px', background: theme.headerBg, fontWeight: 'bold', borderBottom: `1px solid ${theme.border}` }}>
              Scene
            </div>
            <div style={{ padding: '5px', display: 'flex', gap: '5px', borderBottom: `1px solid ${theme.border}` }}>
              <button onClick={handleAddNode} style={{ flex: 1, background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer' }}>+ Node</button>
              <button onClick={handleAddInstance} style={{ flex: 1, background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer' }}>+ Instance</button>
            </div>
            <ul style={{ listStyle: 'none', padding: '5px', margin: 0, overflowY: 'auto', flex: 1 }}>
              {entities.map(ent => (
                <li 
                  key={ent.id} 
                  style={{ 
                    padding: '4px 8px', 
                    cursor: 'pointer', 
                    background: selectedEntity?.id === ent.id ? theme.accentHover : 'transparent',
                    color: selectedEntity?.id === ent.id ? '#fff' : theme.textMain,
                    borderRadius: '3px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    gap: '8px'
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }} onClick={() => setSelectedEntity(ent)}>
                    <span style={{ color: theme.accent }}>{ent.name.includes('Camera') ? '🎥' : '⬜'}</span>
                    {ent.name}
                  </div>
                  <button 
                    onClick={(e) => { e.stopPropagation(); handleDeleteNode(ent.id); }}
                    style={{ background: 'transparent', border: 'none', color: '#ff6b6b', cursor: 'pointer', padding: '0 5px', opacity: selectedEntity?.id === ent.id ? 1 : 0.3 }}
                    title="Delete Node"
                  >
                    ×
                  </button>
                </li>
              ))}
              {entities.length === 0 && <li style={{ color: theme.textMuted, padding: '10px', textAlign: 'center' }}>No entities</li>}
            </ul>
          </div>

          {/* FileSystem */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <div style={{ padding: '6px 10px', background: theme.headerBg, fontWeight: 'bold', borderBottom: `1px solid ${theme.border}` }}>
              FileSystem
            </div>
            <div style={{ padding: '10px', color: theme.textMuted, overflowY: 'auto', flex: 1 }}>
              <div style={{ display: 'flex', gap: '6px', marginBottom: '8px' }}>
                <button onClick={handleImportTexture} style={{ flex: 1, background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer' }}>Import Texture</button>
              </div>
              {importedTextures.length > 0 && (
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ color: theme.textMain, marginBottom: '4px' }}>Imported Textures</div>
                  {importedTextures.map(texture => (
                    <div key={texture.handle} style={{ padding: '3px 4px', borderRadius: '3px', background: selectedTextureHandle === texture.handle ? theme.accentHover : 'transparent', cursor: 'pointer', color: selectedTextureHandle === texture.handle ? '#fff' : theme.textMain }} onClick={() => setSelectedTextureHandle(texture.handle)}>
                      #{texture.handle} {texture.path.split(/[/\\]/).pop()}
                    </div>
                  ))}
                </div>
              )}
              <div style={{ borderTop: `1px solid ${theme.border}`, margin: '8px 0' }}></div>
              <div style={{ color: theme.textMain, marginBottom: '4px' }}>Material Instances</div>
              <input
                type="text"
                value={newMaterialName}
                onChange={(e) => setNewMaterialName(e.target.value)}
                style={{ width: '100%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '4px', marginBottom: '6px' }}
              />
              <select
                value={newMaterialVariant}
                onChange={(e) => setNewMaterialVariant(e.target.value)}
                style={{ width: '100%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '4px', marginBottom: '6px' }}
              >
                {shaderVariants.map(v => <option key={v} value={v}>{v}</option>)}
              </select>
              <button onClick={handleCreateMaterial} style={{ width: '100%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer', marginBottom: '6px' }}>Create Material</button>
              <button onClick={refreshMaterialInstances} style={{ width: '100%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer', marginBottom: '6px' }}>Refresh Materials</button>
              <button onClick={handleReplayMaterialHotUpdates} style={{ width: '100%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer', marginBottom: '6px' }}>Replay Material Hot Updates</button>
              {materialInstances.map(mat => (
                <div key={mat.id} style={{ padding: '3px 4px', borderRadius: '3px', background: selectedMaterialId === mat.id ? theme.accentHover : 'transparent', cursor: 'pointer', color: selectedMaterialId === mat.id ? '#fff' : theme.textMain }} onClick={() => setSelectedMaterialId(mat.id)}>
                  #{mat.id} {mat.name} ({mat.shaderVariant})
                </div>
              ))}
              {selectedMaterialId > 0 && (
                <div style={{ marginTop: '8px', borderTop: `1px solid ${theme.border}`, paddingTop: '8px' }}>
                  <div style={{ color: theme.textMain, marginBottom: '4px' }}>Edit Material #{selectedMaterialId}</div>
                  <div style={{ display: 'flex', gap: '4px', marginBottom: '6px' }}>
                    {[0, 1, 2, 3].map((i) => (
                      <input
                        key={`tint-${i}`}
                        type="number"
                        step="0.01"
                        value={materialTint[i]}
                        onChange={(e) => {
                          const next: [number, number, number, number] = [...materialTint] as [number, number, number, number];
                          next[i as 0 | 1 | 2 | 3] = Number(e.target.value);
                          setMaterialTint(next);
                        }}
                        style={{ width: '25%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '3px' }}
                      />
                    ))}
                  </div>
                  <div style={{ display: 'flex', gap: '4px', marginBottom: '6px' }}>
                    {[0, 1, 2, 3].map((i) => (
                      <input
                        key={`uv-${i}`}
                        type="number"
                        step="0.01"
                        value={materialUv[i]}
                        onChange={(e) => {
                          const next: [number, number, number, number] = [...materialUv] as [number, number, number, number];
                          next[i as 0 | 1 | 2 | 3] = Number(e.target.value);
                          setMaterialUv(next);
                        }}
                        style={{ width: '25%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '3px' }}
                      />
                    ))}
                  </div>
                  <select
                    value={materialBlendMode}
                    onChange={(e) => setMaterialBlendMode(Number(e.target.value))}
                    style={{ width: '100%', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '4px', marginBottom: '6px' }}
                  >
                    <option value={0}>Alpha</option>
                    <option value={1}>Additive</option>
                    <option value={2}>Multiply</option>
                  </select>
                  <button onClick={handleUpdateMaterial} style={{ width: '100%', background: theme.accent, color: '#fff', border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', cursor: 'pointer' }}>Update Material Params</button>
                </div>
              )}
              <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '4px', cursor: 'pointer' }} onClick={() => logMessage("Opened res://")}>📁 res://</div>
              <div style={{ paddingLeft: '15px', display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '4px', cursor: 'pointer' }} onClick={() => logMessage("Opened scripts folder")}>📁 scripts</div>
              <div style={{ paddingLeft: '15px', display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '4px', cursor: 'pointer' }} onClick={() => logMessage("Opened scenes folder")}>📁 scenes</div>
              <div style={{ paddingLeft: '15px', display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '4px', cursor: 'pointer', color: theme.textMain }} onDoubleClick={() => logMessage("Loading scene: main.tscn")}>📄 main.tscn</div>
              <div style={{ paddingLeft: '15px', display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '4px', cursor: 'pointer', color: theme.textMain }} onDoubleClick={() => logMessage("Opened script: player.lua")}>📄 player.lua</div>
            </div>
          </div>
        </div>
        
        {/* Center Panel: Viewport & Bottom Dock */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', background: theme.bgDark }}>
          {/* Viewport Toolbar */}
          <div style={{ display: 'flex', padding: '4px 10px', background: theme.bgPanel, borderBottom: `1px solid ${theme.border}`, gap: '10px' }}>
            <button onClick={() => { setActiveTool('select'); logMessage("Tool changed to Select"); }} style={{ background: 'transparent', border: 'none', color: activeTool === 'select' ? theme.accent : theme.textMain, cursor: 'pointer' }}>👆 Select</button>
            <button onClick={() => { setActiveTool('move'); logMessage("Tool changed to Move"); }} style={{ background: 'transparent', border: 'none', color: activeTool === 'move' ? theme.accent : theme.textMain, cursor: 'pointer' }}>✛ Move</button>
            <button onClick={() => { setActiveTool('rotate'); logMessage("Tool changed to Rotate"); }} style={{ background: 'transparent', border: 'none', color: activeTool === 'rotate' ? theme.accent : theme.textMain, cursor: 'pointer' }}>↻ Rotate</button>
            <button onClick={() => { setActiveTool('scale'); logMessage("Tool changed to Scale"); }} style={{ background: 'transparent', border: 'none', color: activeTool === 'scale' ? theme.accent : theme.textMain, cursor: 'pointer' }}>⤢ Scale</button>
            <div style={{ borderLeft: `1px solid ${theme.border}`, margin: '0 5px' }}></div>
            <button onClick={() => logMessage("Toggled Grid Snap")} style={{ background: 'transparent', border: 'none', color: theme.textMain, cursor: 'pointer' }}># Grid Snap</button>
          </div>

          {/* Viewport Canvas */}
          <div style={{ flex: 1, display: 'flex', justifyContent: 'center', alignItems: 'center', position: 'relative', overflow: 'hidden' }}>
            <div style={{ position: 'absolute', top: 10, left: 10, color: 'rgba(255,255,255,0.7)', background: 'rgba(0,0,0,0.5)', padding: '5px 10px', borderRadius: '4px', fontSize: '12px', pointerEvents: 'none' }}>
              Perspective | Top | {frameInfo.source} | {frameInfo.width}x{frameInfo.height}
            </div>
            <div style={{ position: 'absolute', top: 36, left: 10, color: 'rgba(255,255,255,0.8)', background: 'rgba(0,0,0,0.5)', padding: '5px 10px', borderRadius: '4px', fontSize: '11px', pointerEvents: 'none' }}>
              Frame#{frameStats.frameId} | Lat {frameStats.latencyMs.toFixed(2)}ms | Copy {frameStats.copyMs.toFixed(2)}ms | BW {frameStats.throughputMBps.toFixed(1)}MB/s | Drop {frameStats.droppedFrames}
            </div>
            <div style={{ position: 'absolute', top: 62, left: 10, color: 'rgba(255,255,255,0.85)', background: 'rgba(0,0,0,0.5)', padding: '5px 10px', borderRadius: '4px', fontSize: '11px', pointerEvents: 'none' }}>
              DrawCalls {frameStats.drawCalls} | MaxBatch {frameStats.maxBatchSprites} | Sprites {frameStats.spriteCount} | Entities {frameStats.entityCount} | Physics {frameStats.physicsBodies}
            </div>
            <canvas 
              ref={canvasRef} 
              width={800} 
              height={600} 
              onMouseDown={handleMouseDown}
              onMouseMove={handleMouseMove}
              onMouseUp={handleMouseUp}
              onMouseLeave={handleMouseUp}
              style={{ 
                border: `1px solid ${theme.border}`, 
                boxShadow: '0 4px 15px rgba(0,0,0,0.5)',
                cursor: isDragging.current ? 'grabbing' : 'crosshair',
                background: '#2d3340'
              }}
            />
          </div>

          {/* Bottom Dock */}
          <div style={{ height: '150px', background: theme.bgPanel, borderTop: `1px solid ${theme.border}`, display: 'flex', flexDirection: 'column' }}>
            <div style={{ display: 'flex', background: theme.headerBg, borderBottom: `1px solid ${theme.border}` }}>
              {['output', 'debugger', 'audio', 'animation'].map(tab => (
                <button 
                  key={tab}
                  onClick={() => setBottomTab(tab as any)}
                  style={{ 
                    padding: '6px 15px', 
                    background: bottomTab === tab ? theme.bgPanel : 'transparent', 
                    color: bottomTab === tab ? theme.textMain : theme.textMuted, 
                    border: 'none', 
                    borderTop: bottomTab === tab ? `2px solid ${theme.accent}` : '2px solid transparent',
                    cursor: 'pointer' 
                  }}>
                  {tab.charAt(0).toUpperCase() + tab.slice(1)}
                </button>
              ))}
            </div>
            <div style={{ padding: '10px', flex: 1, overflowY: 'auto', fontFamily: 'monospace', fontSize: '12px' }}>
              {bottomTab === 'output' && (
                <>
                  <div style={{ color: theme.textMuted }}>--- DSEngine Debug Output ---</div>
                  {outputLogs.map((log, index) => (
                    <div key={index} style={{ color: log.includes('Error') ? '#ff6b6b' : (log.includes('success') ? '#4caf50' : theme.textMain) }}>
                      {log}
                    </div>
                  ))}
                </>
              )}
              {bottomTab !== 'output' && <div style={{ color: theme.textMuted }}>No data for {bottomTab}.</div>}
            </div>
          </div>
        </div>

        {/* Right Panel: Inspector & Node */}
        <div style={{ width: '300px', background: theme.bgPanel, borderLeft: `1px solid ${theme.border}`, display: 'flex', flexDirection: 'column' }}>
          
          {/* Tabs */}
          <div style={{ display: 'flex', background: theme.headerBg, borderBottom: `1px solid ${theme.border}` }}>
            <button 
              onClick={() => setActiveTab('inspector')}
              style={{ flex: 1, padding: '8px', background: activeTab === 'inspector' ? theme.bgPanel : 'transparent', color: activeTab === 'inspector' ? theme.textMain : theme.textMuted, border: 'none' }}>
              Inspector
            </button>
            <button 
              onClick={() => setActiveTab('node')}
              style={{ flex: 1, padding: '8px', background: activeTab === 'node' ? theme.bgPanel : 'transparent', color: activeTab === 'node' ? theme.textMain : theme.textMuted, border: 'none' }}>
              Node
            </button>
            <button 
              onClick={() => setActiveTab('build')}
              style={{ flex: 1, padding: '8px', background: activeTab === 'build' ? theme.bgPanel : 'transparent', color: activeTab === 'build' ? theme.textMain : theme.textMuted, border: 'none' }}>
              Build
            </button>
          </div>

          <div style={{ padding: '0', flex: 1, overflowY: 'auto' }}>
            {activeTab === 'inspector' && (
              selectedEntity ? (
                <div style={{ padding: '10px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', marginBottom: '15px', gap: '10px' }}>
                    <span style={{ fontSize: '20px', color: theme.accent }}>{selectedEntity.name.includes('Camera') ? '🎥' : '⬜'}</span>
                    <input 
                      type="text" 
                      value={selectedEntity.name} 
                      readOnly 
                      style={{ flex: 1, background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px 8px', borderRadius: '3px' }} 
                    />
                  </div>
                  
                  {/* Transform Section */}
                  <div style={{ background: theme.headerBg, padding: '5px 10px', fontWeight: 'bold', borderTop: `1px solid ${theme.border}`, borderBottom: `1px solid ${theme.border}`, margin: '0 -10px' }}>
                    Transform2D
                  </div>
                  <div style={{ padding: '10px 0' }}>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                      <span style={{ width: '80px', color: theme.textMuted }}>Position</span>
                      <div style={{ display: 'flex', flex: 1, gap: '5px' }}>
                        <div style={{ flex: 1, display: 'flex', background: theme.bgDark, border: `1px solid ${theme.border}`, borderRadius: '3px', overflow: 'hidden' }}>
                          <span style={{ padding: '2px 5px', background: '#3b2d2d', color: '#ff6b6b' }}>x</span>
                          <input type="text" value={selectedEntity.position.x.toFixed(2)} readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 5px' }} />
                        </div>
                        <div style={{ flex: 1, display: 'flex', background: theme.bgDark, border: `1px solid ${theme.border}`, borderRadius: '3px', overflow: 'hidden' }}>
                          <span style={{ padding: '2px 5px', background: '#2d3b2d', color: '#6bff6b' }}>y</span>
                          <input type="text" value={selectedEntity.position.y.toFixed(2)} readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 5px' }} />
                        </div>
                      </div>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                      <span style={{ width: '80px', color: theme.textMuted }}>Rotation</span>
                      <div style={{ display: 'flex', flex: 1, background: theme.bgDark, border: `1px solid ${theme.border}`, borderRadius: '3px', overflow: 'hidden' }}>
                        <span style={{ padding: '2px 5px', background: '#2d2d3b', color: '#6b6bff' }}>d</span>
                        <input type="text" value="0.00" readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 5px' }} />
                      </div>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                      <span style={{ width: '80px', color: theme.textMuted }}>Scale</span>
                      <div style={{ display: 'flex', flex: 1, gap: '5px' }}>
                        <div style={{ flex: 1, display: 'flex', background: theme.bgDark, border: `1px solid ${theme.border}`, borderRadius: '3px', overflow: 'hidden' }}>
                          <span style={{ padding: '2px 5px', background: '#3b2d2d', color: '#ff6b6b' }}>x</span>
                          <input type="text" value="1.00" readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 5px' }} />
                        </div>
                        <div style={{ flex: 1, display: 'flex', background: theme.bgDark, border: `1px solid ${theme.border}`, borderRadius: '3px', overflow: 'hidden' }}>
                          <span style={{ padding: '2px 5px', background: '#2d3b2d', color: '#6bff6b' }}>y</span>
                          <input type="text" value="1.00" readOnly style={{ width: '100%', background: 'transparent', border: 'none', color: theme.textMain, padding: '0 5px' }} />
                        </div>
                      </div>
                    </div>
                  </div>

                  {(
                    <>
                      <div style={{ background: theme.headerBg, padding: '5px 10px', fontWeight: 'bold', borderTop: `1px solid ${theme.border}`, borderBottom: `1px solid ${theme.border}`, margin: '0 -10px' }}>
                        SpriteRenderer
                      </div>
                      <div style={{ padding: '10px 0' }}>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Texture</span>
                          <div style={{ flex: 1, background: theme.bgDark, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', color: theme.accent }}>
                            {selectedEntity.textureHandle ? `handle=${selectedEntity.textureHandle}` : 'none'}
                          </div>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Variant</span>
                          <div style={{ flex: 1, background: theme.bgDark, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', color: theme.accent }}>
                            {selectedEntity.shaderVariant ?? 'SPRITE_UNLIT'}
                          </div>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Blend</span>
                          <div style={{ flex: 1, background: theme.bgDark, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', color: theme.accent }}>
                            {selectedEntity.blendMode === 1 ? 'Additive' : selectedEntity.blendMode === 2 ? 'Multiply' : 'Alpha'}
                          </div>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Material</span>
                          <select value={selectedMaterialId} onChange={(e) => setSelectedMaterialId(Number(e.target.value))} style={{ flex: 1, background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '3px' }}>
                            <option value={0}>None</option>
                            {materialInstances.map(mat => (
                              <option key={mat.id} value={mat.id}>#{mat.id} {mat.name}</option>
                            ))}
                          </select>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Assign</span>
                          <select value={selectedTextureHandle} onChange={(e) => setSelectedTextureHandle(Number(e.target.value))} style={{ flex: 1, background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px', padding: '3px' }}>
                            <option value={0}>None</option>
                            {importedTextures.map(tex => (
                              <option key={tex.handle} value={tex.handle}>#{tex.handle} {tex.path.split(/[/\\]/).pop()}</option>
                            ))}
                          </select>
                        </div>
                        <button onClick={handleApplyTexture} disabled={!selectedTextureHandle} style={{ width: '100%', background: selectedTextureHandle ? theme.accent : theme.bgDark, color: selectedTextureHandle ? '#fff' : theme.textMuted, border: `1px solid ${theme.border}`, padding: '6px', borderRadius: '3px', cursor: selectedTextureHandle ? 'pointer' : 'not-allowed' }}>Apply To Entity</button>
                        <button onClick={handleApplyMaterial} disabled={!selectedMaterialId} style={{ width: '100%', background: selectedMaterialId ? theme.accent : theme.bgDark, color: selectedMaterialId ? '#fff' : theme.textMuted, border: `1px solid ${theme.border}`, padding: '6px', borderRadius: '3px', cursor: selectedMaterialId ? 'pointer' : 'not-allowed', marginTop: '6px' }}>Apply Material</button>
                      </div>
                    </>
                  )}
                </div>
              ) : (
                <div style={{ color: theme.textMuted, textAlign: 'center', marginTop: '40px' }}>Select a node to edit its properties.</div>
              )
            )}

            {activeTab === 'node' && (
              <div style={{ padding: '10px' }}>
                <div style={{ display: 'flex', gap: '5px', marginBottom: '10px' }}>
                  <button style={{ flex: 1, background: theme.bgPanel, color: theme.textMain, border: `1px solid ${theme.border}`, padding: '4px' }}>Signals</button>
                  <button style={{ flex: 1, background: theme.bgDark, color: theme.textMuted, border: `1px solid ${theme.border}`, padding: '4px' }}>Groups</button>
                </div>
                {selectedEntity ? (
                  <ul style={{ listStyle: 'none', padding: 0, margin: 0 }}>
                    <li style={{ padding: '5px 0', borderBottom: `1px solid ${theme.border}` }}>
                      <div style={{ color: theme.textMuted, marginBottom: '4px' }}>Node2D</div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}><span style={{ color: '#ffb74d' }}>⚡</span> tree_entered()</div>
                    </li>
                    <li style={{ padding: '5px 0', borderBottom: `1px solid ${theme.border}` }}>
                      <div style={{ color: theme.textMuted, marginBottom: '4px' }}>CanvasItem</div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}><span style={{ color: '#ffb74d' }}>⚡</span> draw()</div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}><span style={{ color: '#ffb74d' }}>⚡</span> visibility_changed()</div>
                    </li>
                  </ul>
                ) : (
                  <div style={{ color: theme.textMuted, textAlign: 'center', marginTop: '20px' }}>Select a node to connect signals.</div>
                )}
              </div>
            )}

            {activeTab === 'build' && (
              <div style={{ padding: '15px' }}>
                <div style={{ marginBottom: '15px' }}>
                  <label style={{ display: 'block', color: theme.textMuted, marginBottom: '5px' }}>Target Platform</label>
                  <select id="buildTarget" style={{ width: '100%', padding: '6px', background: theme.bgDark, color: theme.textMain, border: `1px solid ${theme.border}`, borderRadius: '3px' }}>
                    <option value="win64">Windows Desktop</option>
                    <option value="mac">macOS</option>
                    <option value="wasm">HTML5 (WebAssembly)</option>
                  </select>
                </div>
                <button 
                  onClick={() => {
                    if (window.electronAPI.buildProject) {
                      const target = (document.getElementById('buildTarget') as HTMLSelectElement).value;
                      window.electronAPI.buildProject(target).then(res => {
                        alert(res.message);
                      });
                    }
                  }}
                  style={{ width: '100%', padding: '8px', background: theme.accent, color: 'white', border: 'none', borderRadius: '3px', cursor: 'pointer', fontWeight: 'bold' }}
                >
                  Export Project
                </button>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
