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

export const EditorApp: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [engineVersion, setEngineVersion] = useState<string>('Loading...');
  const [entities, setEntities] = useState<EntityData[]>([]);
  const [selectedEntity, setSelectedEntity] = useState<EntityData | null>(null);
  const [activeTab, setActiveTab] = useState<'inspector' | 'particle' | 'build'>('inspector');

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
        
        // Fetch initial entities
        if (window.electronAPI.getEntities) {
          window.electronAPI.getEntities().then((data) => {
            setEntities(data);
          }).catch(console.error);
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

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{ background: '#333', padding: '10px', borderBottom: '1px solid #111', display: 'flex', justifyContent: 'space-between' }}>
        <strong>DSEngine Editor (Electron + React)</strong>
        <span style={{ fontSize: '0.85em', color: '#aaa' }}>{engineVersion}</span>
      </div>
      <div style={{ display: 'flex', flex: 1 }}>
        {/* Left Panel: Hierarchy */}
        <div style={{ width: '250px', background: '#252526', borderRight: '1px solid #111', padding: '10px' }}>
          <h3>Hierarchy</h3>
          <ul style={{ listStyle: 'none', padding: 0 }}>
            {entities.map(ent => (
              <li 
                key={ent.id} 
                onClick={() => setSelectedEntity(ent)}
                style={{ 
                  padding: '5px', 
                  cursor: 'pointer', 
                  background: selectedEntity?.id === ent.id ? '#007acc' : 'transparent' 
                }}
              >
                {ent.name} (ID: {ent.id})
              </li>
            ))}
            {entities.length === 0 && <li style={{ color: '#888' }}>No entities</li>}
          </ul>
        </div>
        
        {/* Center Panel: Viewport */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', justifyContent: 'center', alignItems: 'center', background: '#1e1e1e', position: 'relative' }}>
          <div style={{ position: 'absolute', top: 10, left: 10, color: 'white', background: 'rgba(0,0,0,0.5)', padding: '5px', borderRadius: '4px', fontSize: '12px', pointerEvents: 'none' }}>
            WYSIWYG Viewport (Click and drag entities to move)
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
              border: '1px solid #333', 
              boxShadow: '0 0 10px rgba(0,0,0,0.5)',
              cursor: isDragging.current ? 'grabbing' : 'grab'
            }}
          />
        </div>

        {/* Right Panel: Inspector & Tools */}
        <div style={{ width: '300px', background: '#252526', borderLeft: '1px solid #111', display: 'flex', flexDirection: 'column' }}>
          
          {/* Tabs */}
          <div style={{ display: 'flex', background: '#2d2d2d', borderBottom: '1px solid #111' }}>
            <button 
              onClick={() => setActiveTab('inspector')}
              style={{ flex: 1, padding: '8px', background: activeTab === 'inspector' ? '#1e1e1e' : 'transparent', color: 'white', border: 'none', borderRight: '1px solid #111' }}>
              Inspector
            </button>
            <button 
              onClick={() => setActiveTab('particle')}
              style={{ flex: 1, padding: '8px', background: activeTab === 'particle' ? '#1e1e1e' : 'transparent', color: 'white', border: 'none', borderRight: '1px solid #111' }}>
              Particle
            </button>
            <button 
              onClick={() => setActiveTab('build')}
              style={{ flex: 1, padding: '8px', background: activeTab === 'build' ? '#1e1e1e' : 'transparent', color: 'white', border: 'none' }}>
              Build
            </button>
          </div>

          <div style={{ padding: '10px', flex: 1, overflowY: 'auto' }}>
            {activeTab === 'inspector' && (
              selectedEntity ? (
                <div>
                  <div style={{ marginBottom: '10px' }}>
                    <label>Name:</label>
                    <input 
                      type="text" 
                      value={selectedEntity.name} 
                      readOnly 
                      style={{ width: '100%', background: '#3c3c3c', color: 'white', border: '1px solid #555', padding: '4px', boxSizing: 'border-box' }} 
                    />
                  </div>
                  <div style={{ marginBottom: '10px' }}>
                    <label>Transform Position:</label>
                    <div style={{ display: 'flex', gap: '5px', marginTop: '5px' }}>
                      <span style={{flex: 1, background: '#333', padding: '2px 5px'}}>X: {selectedEntity.position.x.toFixed(2)}</span>
                      <span style={{flex: 1, background: '#333', padding: '2px 5px'}}>Y: {selectedEntity.position.y.toFixed(2)}</span>
                      <span style={{flex: 1, background: '#333', padding: '2px 5px'}}>Z: {selectedEntity.position.z.toFixed(2)}</span>
                    </div>
                  </div>
                  <button style={{ width: '100%', marginTop: '10px', padding: '5px', background: '#0e639c', color: 'white', border: 'none', cursor: 'pointer' }}>
                    Add Component
                  </button>
                </div>
              ) : (
                <div style={{ color: '#888', textAlign: 'center', marginTop: '20px' }}>Select an entity to inspect</div>
              )
            )}

            {activeTab === 'particle' && (
              <div>
                <h4 style={{ margin: '0 0 10px 0' }}>Particle Editor</h4>
                <div style={{ marginBottom: '10px' }}>
                  <label>Max Particles:</label>
                  <input type="range" min="10" max="1000" defaultValue="100" style={{ width: '100%' }} />
                </div>
                <div style={{ marginBottom: '10px' }}>
                  <label>Emit Rate:</label>
                  <input type="range" min="1" max="100" defaultValue="20" style={{ width: '100%' }} />
                </div>
                <div style={{ marginBottom: '10px' }}>
                  <label>Life Time:</label>
                  <input type="range" min="0.1" max="5.0" step="0.1" defaultValue="2.0" style={{ width: '100%' }} />
                </div>
                <div style={{ marginBottom: '10px' }}>
                  <label>Start Color:</label>
                  <input type="color" defaultValue="#ffffff" style={{ width: '100%', height: '30px' }} />
                </div>
                <button style={{ width: '100%', marginTop: '10px', padding: '5px', background: '#0e639c', color: 'white', border: 'none', cursor: 'pointer' }}>
                  Apply to Selected
                </button>
              </div>
            )}

            {activeTab === 'build' && (
              <div>
                <h4 style={{ margin: '0 0 10px 0' }}>Export Project</h4>
                <select id="buildTarget" style={{ width: '100%', marginBottom: '10px', padding: '5px', background: '#333', color: 'white', border: '1px solid #555' }}>
                  <option value="win64">Windows (.exe)</option>
                  <option value="mac">macOS (.app)</option>
                  <option value="wasm">Web (WASM)</option>
                </select>
                <button 
                  onClick={() => {
                    if (window.electronAPI.buildProject) {
                      const target = (document.getElementById('buildTarget') as HTMLSelectElement).value;
                      window.electronAPI.buildProject(target).then(res => {
                        alert(res.message);
                      });
                    } else {
                      alert('Build pipeline not fully linked in bridge yet.');
                    }
                  }}
                  style={{ width: '100%', padding: '10px', background: '#28a745', color: 'white', border: 'none', cursor: 'pointer', fontWeight: 'bold' }}
                >
                  Build Now
                </button>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
