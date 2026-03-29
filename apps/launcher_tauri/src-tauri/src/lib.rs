use std::path::{Path, PathBuf};
use std::process::Command;
use serde::{Deserialize, Serialize};
use tauri_plugin_dialog::DialogExt;

#[derive(Serialize, Deserialize, Debug)]
pub struct EngineVersion {
    pub tag: String,
    pub executable: String,
    pub available: bool,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct ProjectItem {
    pub name: String,
    pub path: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct EngineMetadata {
    pub engine: String,
    pub version: String,
    pub executable: String,
}

#[tauri::command]
fn get_engine_versions() -> Vec<EngineVersion> {
    let mut versions = Vec::new();
    
    // We are looking for the bin directory relative to the current working directory of the launcher
    // In dev mode, this might be DSEngine/apps/launcher_tauri
    // In production, it might be the install dir. We'll check multiple common relative paths.
    
    let current_dir = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    
    let possible_bin_paths = vec![
        current_dir.join("../../bin"), // If running from apps/launcher_tauri
        current_dir.join("bin"),       // If running from DSEngine root
        current_dir.join("../bin"),    // Just in case
    ];
    
    for bin_path in possible_bin_paths {
        if !bin_path.exists() || !bin_path.is_dir() {
            continue;
        }
        
        let metadata_path = bin_path.join("engine_version.json");
        let editor_exe = bin_path.join("dsengine-editor.exe");
        
        if editor_exe.exists() {
            let mut tag = "Local Dev Build".to_string();
            
            // Try to read metadata if it exists
            if metadata_path.exists() {
                if let Ok(contents) = std::fs::read_to_string(&metadata_path) {
                    if let Ok(metadata) = serde_json::from_str::<EngineMetadata>(&contents) {
                        tag = metadata.version;
                    }
                }
            }
            
            // Normalize path to remove .. and .
            let canonical_exe = std::fs::canonicalize(&editor_exe).unwrap_or(editor_exe);
            
            versions.push(EngineVersion {
                tag,
                executable: canonical_exe.to_string_lossy().to_string().replace("\\\\?\\", ""),
                available: true,
            });
            
            // Stop after finding the first valid bin directory
            break;
        }
    }
    
    versions
}

#[tauri::command]
async fn choose_project_root(app: tauri::AppHandle) -> Result<Option<String>, String> {
    // In Tauri v2, blocking dialogs in async commands require careful handling.
    // The correct API for blocking pick_folder is blocking_pick_folder().
    let file_path = app.dialog().file().blocking_pick_folder();
    
    if let Some(path) = file_path {
        Ok(Some(path.into_path().unwrap().to_string_lossy().to_string()))
    } else {
        Ok(None)
    }
}

#[tauri::command]
fn scan_projects(root_dir: String) -> Vec<ProjectItem> {
    let mut projects = Vec::new();
    let root_path = Path::new(&root_dir);
    
    if !root_path.exists() || !root_path.is_dir() {
        return projects;
    }
    
    if let Ok(entries) = std::fs::read_dir(root_path) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                // Check if it's a valid project (contains main.lua or CMakeLists.txt)
                let is_project = path.join("main.lua").exists() || path.join("CMakeLists.txt").exists();
                
                if is_project {
                    projects.push(ProjectItem {
                        name: entry.file_name().to_string_lossy().to_string(),
                        path: path.to_string_lossy().to_string(),
                    });
                }
            }
        }
    }
    
    projects
}

#[tauri::command]
fn launch_editor(project_path: String, executable: String) -> Result<serde_json::Value, String> {
    let exe_path = Path::new(&executable);
    if !exe_path.exists() {
        return Err("Engine executable not found".to_string());
    }
    
    let mut cmd = Command::new(executable);
    cmd.current_dir(project_path); // Set the working directory to the project root
    
    match cmd.spawn() {
        Ok(_) => {
            Ok(serde_json::json!({ "success": true }))
        },
        Err(e) => {
            Err(format!("Failed to launch engine: {}", e))
        }
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .plugin(tauri_plugin_dialog::init())
    .invoke_handler(tauri::generate_handler![
        get_engine_versions,
        choose_project_root,
        scan_projects,
        launch_editor
    ])
    .setup(|app| {
            if cfg!(debug_assertions) {
                app.handle().plugin(
                    tauri_plugin_log::Builder::default()
                        .level(log::LevelFilter::Info)
                        .build(),
                )?;
            }
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
