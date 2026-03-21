const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');

const target = process.argv[2] || 'win64';
const projectRoot = path.resolve(__dirname, '../../');
const buildDir = path.join(projectRoot, `build_export_${target}`);

console.log(`Starting DSEngine export pipeline for target: ${target}`);
console.log(`Project root: ${projectRoot}`);

if (!fs.existsSync(buildDir)) {
    fs.mkdirSync(buildDir, { recursive: true });
}

// Map targets to CMake generators/toolchains
let cmakeArgs = '';
if (target === 'win64') {
    cmakeArgs = '-G "Visual Studio 17 2022" -A x64';
} else if (target === 'wasm') {
    // Assuming emscripten is in PATH
    cmakeArgs = '-DCMAKE_TOOLCHAIN_FILE=path/to/emscripten/cmake/Modules/Platform/emscripten.cmake';
} else if (target === 'mac') {
    cmakeArgs = '-G Xcode';
} else {
    console.error(`Unknown target: ${target}`);
    process.exit(1);
}

// 1. Run CMake Configure
const configureCmd = `cmake -S "${projectRoot}" -B "${buildDir}" ${cmakeArgs}`;
console.log(`Running: ${configureCmd}`);

exec(configureCmd, (error, stdout, stderr) => {
    if (error) {
        console.error(`CMake configuration failed: ${error.message}`);
        return;
    }
    console.log(stdout);

    // 2. Run CMake Build
    const buildCmd = `cmake --build "${buildDir}" --config Release`;
    console.log(`Running: ${buildCmd}`);
    
    exec(buildCmd, (error, stdout, stderr) => {
        if (error) {
            console.error(`Build failed: ${error.message}`);
            return;
        }
        console.log(stdout);
        console.log('Build completed successfully!');
        
        // 3. Asset Packaging (Copying to release folder)
        console.log('Packaging assets...');
        const assetDir = path.join(projectRoot, 'example', 'data');
        const exportAssetDir = path.join(buildDir, 'Release', 'data');
        
        if (fs.existsSync(assetDir)) {
            try {
                fs.cpSync(assetDir, exportAssetDir, { recursive: true });
                console.log(`Assets copied to ${exportAssetDir}`);
            } catch (cpErr) {
                console.warn(`Asset copy failed (requires Node 16+): ${cpErr.message}`);
            }
        } else {
            console.warn(`Asset directory not found: ${assetDir}`);
        }
        
        console.log(`Export pipeline finished. Executable is located in ${path.join(buildDir, 'Release')}`);
    });
});
