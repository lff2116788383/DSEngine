const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');

const target = process.argv[2] || 'win64';
const projectRoot = path.resolve(__dirname, '../../');
const buildDir = path.join(projectRoot, `build_export_${target}`);
const reportsDir = path.join(buildDir, 'reports');
const migrationReportPath = path.join(reportsDir, 'scene_schema_migration_report.json');
const migrationTrendPath = path.join(reportsDir, 'scene_schema_migration_trend.json');
const migrationHtmlPath = path.join(reportsDir, 'scene_schema_migration_report.html');
const migrationDashboardPath = path.join(reportsDir, 'scene_schema_migration_dashboard.json');
const migrationDashboardMarkdownPath = path.join(reportsDir, 'scene_schema_migration_dashboard.md');
const qualityDashboardPath = path.join(reportsDir, 'quality_dashboard.json');
const qualityDashboardMarkdownPath = path.join(reportsDir, 'quality_dashboard.md');
const releaseManifestPath = path.join(reportsDir, 'release_manifest.json');
const releasePackageDir = path.join(buildDir, 'package');
const runtimeOutputDir = path.join(projectRoot, 'bin');

function parseThresholdMap(text) {
    if (!text || typeof text !== 'string') {
        return {};
    }
    const map = {};
    for (const pair of text.split(',')) {
        const token = pair.trim();
        if (!token) {
            continue;
        }
        const idx = token.indexOf(':');
        if (idx <= 0 || idx >= token.length - 1) {
            continue;
        }
        const key = token.slice(0, idx).trim();
        const value = Number(token.slice(idx + 1).trim());
        if (!key || !Number.isFinite(value) || value < 0) {
            continue;
        }
        map[key] = Math.floor(value);
    }
    return map;
}

function readThresholdConfig() {
    return {
        maxFailedTotal: Number.isFinite(Number(process.env.DSENGINE_MIGRATION_MAX_FAILED_TOTAL))
            ? Math.max(0, Math.floor(Number(process.env.DSENGINE_MIGRATION_MAX_FAILED_TOTAL)))
            : 0,
        maxFailedByReason: parseThresholdMap(process.env.DSENGINE_MIGRATION_MAX_FAILED_BY_REASON || ''),
        maxFailedByDirectory: parseThresholdMap(process.env.DSENGINE_MIGRATION_MAX_FAILED_BY_DIRECTORY || ''),
        requireMaterialReplay: process.env.DSENGINE_REQUIRE_MATERIAL_REPLAY === '1'
    };
}

function toMap(items, keyField, valueField) {
    const result = {};
    if (!Array.isArray(items)) {
        return result;
    }
    for (const item of items) {
        const key = typeof item?.[keyField] === 'string' ? item[keyField] : '';
        const value = Number(item?.[valueField] ?? 0);
        if (!key || !Number.isFinite(value)) {
            continue;
        }
        result[key] = value;
    }
    return result;
}

function evaluateAlerts(report, thresholds) {
    const alerts = [];
    const totalFailed = Number(report?.summary?.failed?.length ?? 0);
    if (totalFailed > thresholds.maxFailedTotal) {
        alerts.push({
            level: 'error',
            key: 'total_failed',
            actual: totalFailed,
            threshold: thresholds.maxFailedTotal,
            message: `total failed ${totalFailed} > threshold ${thresholds.maxFailedTotal}`
        });
    }
    const reasonCounts = toMap(report?.aggregations?.failuresByReason, 'reason', 'count');
    for (const [reason, maxAllowed] of Object.entries(thresholds.maxFailedByReason)) {
        const actual = Number(reasonCounts[reason] || 0);
        if (actual > maxAllowed) {
            alerts.push({
                level: 'error',
                key: `reason:${reason}`,
                actual,
                threshold: maxAllowed,
                message: `reason ${reason} failed ${actual} > threshold ${maxAllowed}`
            });
        }
    }
    const directoryCounts = toMap(report?.aggregations?.failuresByDirectory, 'directory', 'count');
    for (const [directory, maxAllowed] of Object.entries(thresholds.maxFailedByDirectory)) {
        const actual = Number(directoryCounts[directory] || 0);
        if (actual > maxAllowed) {
            alerts.push({
                level: 'error',
                key: `directory:${directory}`,
                actual,
                threshold: maxAllowed,
                message: `directory ${directory} failed ${actual} > threshold ${maxAllowed}`
            });
        }
    }
    return alerts;
}

function ensureParentDirectory(filePath) {
    fs.mkdirSync(path.dirname(filePath), { recursive: true });
}

function writeDashboardArtifacts(report, trend, thresholds, alerts) {
    const trendHistory = Array.isArray(trend?.history) ? trend.history : [];
    const dashboard = {
        generatedAt: new Date().toISOString(),
        target,
        reportPath: migrationReportPath,
        trendPath: migrationTrendPath,
        htmlPath: migrationHtmlPath,
        thresholds,
        health: alerts.length === 0 ? 'ok' : 'error',
        alerts,
        latest: {
            scanned: Number(report?.summary?.scanned ?? 0),
            changed: Number(report?.summary?.changed ?? 0),
            unchanged: Number(report?.summary?.unchanged ?? 0),
            failed: Number(report?.summary?.failed?.length ?? 0),
            failuresByReason: report?.aggregations?.failuresByReason || [],
            failuresByDirectory: report?.aggregations?.failuresByDirectory || []
        },
        trend: trendHistory.slice(-20)
    };
    ensureParentDirectory(migrationDashboardPath);
    fs.writeFileSync(migrationDashboardPath, `${JSON.stringify(dashboard, null, 2)}\n`, 'utf8');

    const md = [
        '# Scene Schema Migration Dashboard',
        '',
        `- target: ${target}`,
        `- generatedAt: ${dashboard.generatedAt}`,
        `- health: ${dashboard.health}`,
        `- scanned: ${dashboard.latest.scanned}`,
        `- changed: ${dashboard.latest.changed}`,
        `- unchanged: ${dashboard.latest.unchanged}`,
        `- failed: ${dashboard.latest.failed}`,
        '',
        '## Thresholds',
        `- maxFailedTotal: ${thresholds.maxFailedTotal}`,
        `- maxFailedByReason: ${JSON.stringify(thresholds.maxFailedByReason)}`,
        `- maxFailedByDirectory: ${JSON.stringify(thresholds.maxFailedByDirectory)}`,
        '',
        '## Alerts',
        ...(alerts.length > 0 ? alerts.map((a) => `- ${a.message}`) : ['- none']),
        '',
        '## Top Failure Reasons',
        ...((dashboard.latest.failuresByReason.length > 0
            ? dashboard.latest.failuresByReason
            : [{ reason: 'none', count: 0 }]).map((r) => `- ${r.reason}: ${r.count}`)),
        '',
        '## Top Failure Directories',
        ...((dashboard.latest.failuresByDirectory.length > 0
            ? dashboard.latest.failuresByDirectory
            : [{ directory: 'none', count: 0 }]).map((d) => `- ${d.directory}: ${d.count}`))
    ].join('\n');
    ensureParentDirectory(migrationDashboardMarkdownPath);
    fs.writeFileSync(migrationDashboardMarkdownPath, `${md}\n`, 'utf8');
}

function runShellCapture(command) {
    console.log(`Running: ${command}`);
    return new Promise((resolve, reject) => {
        exec(command, (error, stdout, stderr) => {
            if (stdout) {
                console.log(stdout);
            }
            if (stderr) {
                console.warn(stderr);
            }
            if (error) {
                reject({ error, stdout, stderr });
                return;
            }
            resolve({ stdout, stderr });
        });
    });
}

function readJson(filePath) {
    return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function runShell(command) {
    console.log(`Running: ${command}`);
    return new Promise((resolve, reject) => {
        exec(command, (error, stdout, stderr) => {
            if (stdout) {
                console.log(stdout);
            }
            if (stderr) {
                console.warn(stderr);
            }
            if (error) {
                reject(error);
                return;
            }
            resolve();
        });
    });
}

function hashFile(filePath) {
    const crypto = require('crypto');
    const data = fs.readFileSync(filePath);
    return crypto.createHash('sha256').update(data).digest('hex');
}

function collectReleaseArtifacts(rootDir) {
    if (!fs.existsSync(rootDir)) {
        return [];
    }
    const queue = [rootDir];
    const files = [];
    while (queue.length > 0) {
        const dir = queue.pop();
        for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
            const fullPath = path.join(dir, entry.name);
            if (entry.isDirectory()) {
                queue.push(fullPath);
                continue;
            }
            files.push(fullPath);
        }
    }
    return files;
}

function copyDirectoryIfExists(src, dst) {
    if (!fs.existsSync(src)) {
        return false;
    }
    fs.mkdirSync(path.dirname(dst), { recursive: true });
    fs.cpSync(src, dst, { recursive: true });
    return true;
}

function buildReleasePackage(targetName) {
    fs.rmSync(releasePackageDir, { recursive: true, force: true });
    fs.mkdirSync(releasePackageDir, { recursive: true });
    const copied = [];
    const copyTargets = [
        { src: runtimeOutputDir, dst: path.join(releasePackageDir, 'bin') },
        { src: path.join(projectRoot, 'data'), dst: path.join(releasePackageDir, 'data') },
        { src: path.join(projectRoot, 'samples'), dst: path.join(releasePackageDir, 'samples') },
        { src: path.join(projectRoot, 'script'), dst: path.join(releasePackageDir, 'script') }
    ];
    for (const item of copyTargets) {
        if (copyDirectoryIfExists(item.src, item.dst)) {
            copied.push(item.dst);
        }
    }
    const artifactFiles = collectReleaseArtifacts(releasePackageDir).map((fullPath) => {
        const relPath = path.relative(releasePackageDir, fullPath).replace(/\\/g, '/');
        const stat = fs.statSync(fullPath);
        return {
            path: relPath,
            size: stat.size,
            sha256: hashFile(fullPath)
        };
    });
    const manifest = {
        generatedAt: new Date().toISOString(),
        target: targetName,
        packageRoot: releasePackageDir,
        copiedRoots: copied.map((p) => path.relative(projectRoot, p).replace(/\\/g, '/')),
        artifactCount: artifactFiles.length,
        artifacts: artifactFiles
    };
    fs.writeFileSync(releaseManifestPath, `${JSON.stringify(manifest, null, 2)}\n`, 'utf8');
    return manifest;
}

async function runMaterialReplayRegression() {
    const regressionScript = path.join(projectRoot, 'editor', 'scripts', 'material_replay_regression.js');
    if (!fs.existsSync(regressionScript)) {
        return { status: 'skipped', reason: 'script_missing' };
    }
    try {
        const result = await runShellCapture(`node "${regressionScript}"`);
        const parsed = JSON.parse(String(result.stdout || '{}').trim());
        return {
            status: parsed.ok ? 'ok' : 'error',
            detail: parsed
        };
    } catch (err) {
        return {
            status: 'error',
            reason: 'execution_failed',
            detail: {
                message: err?.error?.message || 'unknown_error',
                stdout: String(err?.stdout || ''),
                stderr: String(err?.stderr || '')
            }
        };
    }
}

function writeQualityDashboard({ migrationDashboard, materialReplay, releaseManifest, thresholds, alerts }) {
    const qualityAlerts = [];
    const qualityWarnings = [];
    if (migrationDashboard.health !== 'ok') {
        qualityAlerts.push('migration_health_error');
    }
    if (materialReplay.status === 'error') {
        if (thresholds.requireMaterialReplay) {
            qualityAlerts.push('material_replay_error');
        } else {
            qualityWarnings.push('material_replay_error');
        }
    }
    if (thresholds.requireMaterialReplay && materialReplay.status !== 'ok') {
        qualityAlerts.push('material_replay_required');
    }
    const dashboard = {
        generatedAt: new Date().toISOString(),
        target,
        health: qualityAlerts.length === 0 ? 'ok' : 'error',
        alerts: qualityAlerts,
        warnings: qualityWarnings,
        migration: {
            health: migrationDashboard.health,
            alerts: alerts
        },
        materialReplay,
        release: {
            artifactCount: Number(releaseManifest?.artifactCount || 0),
            manifestPath: releaseManifestPath,
            packageRoot: releasePackageDir
        }
    };
    fs.writeFileSync(qualityDashboardPath, `${JSON.stringify(dashboard, null, 2)}\n`, 'utf8');
    const md = [
        '# DSEngine Quality Dashboard',
        '',
        `- target: ${dashboard.target}`,
        `- generatedAt: ${dashboard.generatedAt}`,
        `- health: ${dashboard.health}`,
        `- migration_health: ${dashboard.migration.health}`,
        `- material_replay: ${dashboard.materialReplay.status}`,
        `- release_artifact_count: ${dashboard.release.artifactCount}`,
        '',
        '## Alerts',
        ...(dashboard.alerts.length > 0 ? dashboard.alerts.map((a) => `- ${a}`) : ['- none']),
        '',
        '## Warnings',
        ...(dashboard.warnings.length > 0 ? dashboard.warnings.map((a) => `- ${a}`) : ['- none']),
        '',
        '## Paths',
        `- release_manifest: ${releaseManifestPath}`,
        `- quality_dashboard: ${qualityDashboardPath}`,
        `- migration_dashboard: ${migrationDashboardPath}`
    ].join('\n');
    fs.writeFileSync(qualityDashboardMarkdownPath, `${md}\n`, 'utf8');
    return dashboard;
}

function getCmakeArgs(targetName) {
    if (targetName === 'win64') {
        return '-G "Visual Studio 17 2022" -A x64';
    }
    if (targetName === 'wasm') {
        return '-DCMAKE_TOOLCHAIN_FILE=path/to/emscripten/cmake/Modules/Platform/emscripten.cmake';
    }
    if (targetName === 'mac') {
        return '-G Xcode';
    }
    throw new Error(`Unknown target: ${targetName}`);
}

async function main() {
    console.log(`Starting DSEngine export pipeline for target: ${target}`);
    console.log(`Project root: ${projectRoot}`);
    fs.mkdirSync(buildDir, { recursive: true });
    fs.mkdirSync(reportsDir, { recursive: true });

    const cmakeArgs = getCmakeArgs(target);
    await runShell(`cmake -S "${projectRoot}" -B "${buildDir}" ${cmakeArgs}`);
    await runShell(`cmake --build "${buildDir}" --config Release`);
    console.log('Build completed successfully!');

    const migrateCmd = `node "${path.join(projectRoot, 'editor', 'scripts', 'scene_material_schema_migrate.js')}" --write --root "${projectRoot}" --report "${migrationReportPath}" --trend-file "${migrationTrendPath}" --html-report "${migrationHtmlPath}"`;
    await runShell(migrateCmd);

    const report = readJson(migrationReportPath);
    const trend = readJson(migrationTrendPath);
    const thresholds = readThresholdConfig();
    const alerts = evaluateAlerts(report, thresholds);
    writeDashboardArtifacts(report, trend, thresholds, alerts);
    const migrationDashboard = readJson(migrationDashboardPath);
    const materialReplay = await runMaterialReplayRegression();
    const releaseManifest = buildReleasePackage(target);
    const qualityDashboard = writeQualityDashboard({
        migrationDashboard,
        materialReplay,
        releaseManifest,
        thresholds,
        alerts
    });

    console.log(`Migration report generated at ${migrationReportPath}`);
    console.log(`Migration trend generated at ${migrationTrendPath}`);
    console.log(`Migration html generated at ${migrationHtmlPath}`);
    console.log(`Migration dashboard generated at ${migrationDashboardPath}`);
    console.log(`Migration dashboard markdown generated at ${migrationDashboardMarkdownPath}`);
    console.log(`Release manifest generated at ${releaseManifestPath}`);
    console.log(`Quality dashboard generated at ${qualityDashboardPath}`);
    console.log(`Quality dashboard markdown generated at ${qualityDashboardMarkdownPath}`);
    if (alerts.length > 0) {
        for (const alert of alerts) {
            console.error(`Migration alert: ${alert.message}`);
        }
        throw new Error('Migration alert threshold exceeded');
    }
    if (qualityDashboard.health !== 'ok') {
        throw new Error('Quality dashboard health check failed');
    }
    console.log(`Export pipeline finished. Executable is located in ${path.join(buildDir, 'Release')}`);
}

main().catch((err) => {
    console.error(`Export pipeline failed: ${err.message}`);
    process.exit(1);
});
