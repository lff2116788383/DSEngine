import React, { useEffect, useRef, useState } from 'react';

// Declare the electron API we injected via preload
declare global {
  interface Window {
    electronAPI: {
      getEngineVersion: () => Promise<string>;
      initEngine: () => Promise<any>;
      getFrameBuffer: () => Promise<Uint8Array>;
      getEntities: () => Promise<any[]>;
      updateEntityTransform: (id: number, x: number, y: number, z: number) => Promise<boolean>;
      pickEntity: (x: number, y: number) => Promise<number>;
      createEntity: () => Promise<number>;
      deleteEntity: (id: number) => Promise<boolean>;
      buildProject: (target: string) => Promise<any>;
    };
  }
}

interface EntityData {
  id: number;
  name: string;
  position: { x: number, y: number, z: number };
  has_particle?: boolean;
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

  // Gizmo State
  const isDragging = useRef(false);
  const lastMousePos = useRef({ x: 0, y: 0 });

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
        
        // Start render loop
        let animationFrameId: number;
        
        const renderLoop = async () => {
          if (canvasRef.current && window.electronAPI) {
            try {
              const buffer = await window.electronAPI.getFrameBuffer();
              const ctx = canvasRef.current.getContext('2d');
              if (ctx && buffer) {
                // Create ImageData from the Uint8Array buffer
                const imageData = new ImageData(
                  new Uint8ClampedArray(buffer),
                  800, // FRAME_WIDTH
                  600  // FRAME_HEIGHT
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
    logMessage(newPlayState ? "Starting game preview..." : "Stopped game preview.");
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
              Perspective | Top
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

                  {/* Mock Sprite Component */}
                  {selectedEntity.id === 1 && (
                    <>
                      <div style={{ background: theme.headerBg, padding: '5px 10px', fontWeight: 'bold', borderTop: `1px solid ${theme.border}`, borderBottom: `1px solid ${theme.border}`, margin: '0 -10px' }}>
                        SpriteRenderer
                      </div>
                      <div style={{ padding: '10px 0' }}>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Texture</span>
                          <div style={{ flex: 1, background: theme.bgDark, border: `1px solid ${theme.border}`, padding: '4px', borderRadius: '3px', color: theme.accent }}>[ext_resource id=1]</div>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', marginBottom: '5px' }}>
                          <span style={{ width: '80px', color: theme.textMuted }}>Modulate</span>
                          <div style={{ flex: 1, background: '#ffffff', border: `1px solid ${theme.border}`, height: '20px', borderRadius: '3px' }}></div>
                        </div>
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
