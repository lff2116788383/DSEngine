const fs = require('fs');
const path = require('path');
const os = require('os');

const CURRENT_SCHEMA_VERSION = 2;
const IGNORE_DIRS = new Set(['node_modules', '.git', '.svn', '.hg', 'build', 'dist', 'out', '.trae']);

function normalizePathForReport(filePath) {
  return filePath.replace(/\\/g, '/');
}

function ensureParentDirectory(targetPath) {
  const dir = path.dirname(targetPath);
  fs.mkdirSync(dir, { recursive: true });
}

function aggregateByDirectory(details) {
  const map = new Map();
  for (const item of details) {
    const normalized = normalizePathForReport(item.file);
    const first = normalized.split('/')[0] || '.';
    map.set(first, (map.get(first) || 0) + 1);
  }
  return Array.from(map.entries())
    .map(([directory, count]) => ({ directory, count }))
    .sort((a, b) => b.count - a.count || a.directory.localeCompare(b.directory));
}

function aggregateFailureReasons(failed) {
  const map = new Map();
  for (const item of failed) {
    const reason = item.reason || 'unknown';
    map.set(reason, (map.get(reason) || 0) + 1);
  }
  return Array.from(map.entries())
    .map(([reason, count]) => ({ reason, count }))
    .sort((a, b) => b.count - a.count || a.reason.localeCompare(b.reason));
}

function aggregateFailureDirectories(failed) {
  const map = new Map();
  for (const item of failed) {
    const normalized = normalizePathForReport(item.file || '');
    const first = normalized.split('/')[0] || '.';
    map.set(first, (map.get(first) || 0) + 1);
  }
  return Array.from(map.entries())
    .map(([directory, count]) => ({ directory, count }))
    .sort((a, b) => b.count - a.count || a.directory.localeCompare(b.directory));
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function walkJsonFiles(root, output) {
  const entries = fs.readdirSync(root, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(root, entry.name);
    if (entry.isDirectory()) {
      if (IGNORE_DIRS.has(entry.name)) {
        continue;
      }
      walkJsonFiles(fullPath, output);
      continue;
    }
    if (entry.isFile() && entry.name.toLowerCase().endsWith('.json')) {
      output.push(fullPath);
    }
  }
}

function normalizeArray4(input, fallback) {
  if (!Array.isArray(input) || input.length !== 4) {
    return [...fallback];
  }
  return [
    Number(input[0] ?? fallback[0]),
    Number(input[1] ?? fallback[1]),
    Number(input[2] ?? fallback[2]),
    Number(input[3] ?? fallback[3])
  ];
}

function migrateSceneDoc(doc) {
  if (!doc || typeof doc !== 'object' || Array.isArray(doc)) {
    return { changed: false, reason: 'not_object' };
  }
  const hasEntitiesArray = Array.isArray(doc.entities);
  const hasMaterialsArray = Array.isArray(doc.materials);
  const looksLikeScene = hasEntitiesArray || hasMaterialsArray;
  if (!looksLikeScene) {
    return { changed: false, reason: 'not_scene' };
  }

  let changed = false;
  let beforeVersion = Number.isInteger(doc.material_schema_version) ? doc.material_schema_version : 1;
  if (!Number.isInteger(doc.material_schema_version)) {
    doc.material_schema_version = beforeVersion;
    changed = true;
  }

  if (!Array.isArray(doc.materials)) {
    doc.materials = [];
    changed = true;
  }
  const materialMap = new Map();
  for (const material of doc.materials) {
    if (!material || typeof material !== 'object') {
      continue;
    }
    const materialId = Number(material.material_id || 0);
    if (materialId <= 0) {
      continue;
    }
    material.material_id = materialId;
    if (typeof material.name !== 'string' || material.name.length === 0) {
      material.name = `migrated_material_${materialId}`;
      changed = true;
    }
    if (typeof material.shader_variant !== 'string' || material.shader_variant.length === 0) {
      material.shader_variant = 'SPRITE_UNLIT';
      changed = true;
    }
    const blendMode = Number(material.blend_mode ?? 0);
    material.blend_mode = blendMode >= 2 ? 2 : (blendMode <= 0 ? 0 : 1);
    const textureHandle = Number(material.texture_handle ?? 0);
    material.texture_handle = textureHandle > 0 ? textureHandle : 0;
    material.tint = normalizeArray4(material.tint, [1, 1, 1, 1]);
    material.uv_rect = normalizeArray4(material.uv_rect, [0, 0, 1, 1]);
    materialMap.set(materialId, material);
  }

  if (Array.isArray(doc.entities)) {
    for (const entity of doc.entities) {
      if (!entity || typeof entity !== 'object' || !entity.components || typeof entity.components !== 'object') {
        continue;
      }
      const sprite = entity.components.SpriteRendererComponent;
      if (!sprite || typeof sprite !== 'object') {
        continue;
      }
      const materialId = Number(sprite.material_instance_id ?? 0);
      if (materialId <= 0) {
        continue;
      }
      if (!materialMap.has(materialId)) {
        const migrated = {
          material_id: materialId,
          name: `migrated_material_${materialId}`,
          shader_variant: typeof sprite.shader_variant === 'string' ? sprite.shader_variant : 'SPRITE_UNLIT',
          blend_mode: Number(sprite.blend_mode ?? 0),
          texture_handle: Number(sprite.texture_handle ?? 0),
          tint: normalizeArray4(sprite.color, [1, 1, 1, 1]),
          uv_rect: normalizeArray4(sprite.uv, [0, 0, 1, 1])
        };
        migrated.blend_mode = migrated.blend_mode >= 2 ? 2 : (migrated.blend_mode <= 0 ? 0 : 1);
        migrated.texture_handle = migrated.texture_handle > 0 ? migrated.texture_handle : 0;
        doc.materials.push(migrated);
        materialMap.set(materialId, migrated);
        changed = true;
      }
    }
  }

  if (beforeVersion < CURRENT_SCHEMA_VERSION) {
    doc.material_schema_version = CURRENT_SCHEMA_VERSION;
    changed = true;
  }

  return {
    changed,
    beforeVersion,
    afterVersion: doc.material_schema_version,
    materialCount: Array.isArray(doc.materials) ? doc.materials.length : 0
  };
}

function runMigration(rootDir, writeMode) {
  const files = [];
  walkJsonFiles(rootDir, files);
  let scanned = 0;
  let changedCount = 0;
  const details = [];
  const failed = [];

  for (const filePath of files) {
    scanned += 1;
    let raw = '';
    try {
      raw = fs.readFileSync(filePath, 'utf8');
    } catch (err) {
      failed.push({
        file: normalizePathForReport(path.relative(rootDir, filePath)),
        reason: 'read_failed',
        message: err.message
      });
      continue;
    }
    let doc;
    try {
      doc = JSON.parse(raw);
    } catch (err) {
      failed.push({
        file: normalizePathForReport(path.relative(rootDir, filePath)),
        reason: 'parse_failed',
        message: err.message
      });
      continue;
    }
    const result = migrateSceneDoc(doc);
    if (!result || result.reason === 'not_object' || result.reason === 'not_scene') {
      continue;
    }
    if (result.changed) {
      changedCount += 1;
      if (writeMode) {
        try {
          fs.writeFileSync(filePath, `${JSON.stringify(doc, null, 2)}\n`, 'utf8');
        } catch (err) {
          failed.push({
            file: normalizePathForReport(path.relative(rootDir, filePath)),
            reason: 'write_failed',
            message: err.message
          });
          continue;
        }
      }
      details.push({
        file: normalizePathForReport(path.relative(rootDir, filePath)),
        beforeVersion: result.beforeVersion,
        afterVersion: result.afterVersion,
        materialCount: result.materialCount
      });
    }
  }

  return {
    scanned,
    changed: changedCount,
    unchanged: Math.max(0, scanned - changedCount - failed.length),
    details,
    failed
  };
}

function runSelfTest() {
  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'dse-scene-migrate-'));
  const sceneDir = path.join(tempRoot, 'scenes');
  fs.mkdirSync(sceneDir, { recursive: true });
  const samplePath = path.join(sceneDir, 'legacy_scene.json');
  const sample = {
    name: 'legacy_scene',
    entities: [
      {
        id: 1,
        components: {
          TransformComponent: {
            position: [0, 0, 0],
            rotation: [0, 0, 0, 1],
            scale: [1, 1, 1]
          },
          SpriteRendererComponent: {
            material_instance_id: 4242,
            shader_variant: 'SPRITE_TINT',
            blend_mode: 2,
            texture_handle: 77,
            color: [0.3, 0.4, 0.5, 1.0],
            uv: [0.1, 0.2, 0.8, 0.9]
          }
        }
      }
    ]
  };
  fs.writeFileSync(samplePath, `${JSON.stringify(sample, null, 2)}\n`, 'utf8');

  const summary = runMigration(sceneDir, true);
  const migratedDoc = JSON.parse(fs.readFileSync(samplePath, 'utf8'));
  const ok = summary.changed === 1 &&
    migratedDoc.material_schema_version === CURRENT_SCHEMA_VERSION &&
    Array.isArray(migratedDoc.materials) &&
    migratedDoc.materials.length === 1 &&
    migratedDoc.materials[0].material_id === 4242 &&
    migratedDoc.materials[0].shader_variant === 'SPRITE_TINT' &&
    migratedDoc.materials[0].blend_mode === 2 &&
    migratedDoc.materials[0].texture_handle === 77;

  console.log(JSON.stringify({
    ok,
    mode: 'self-test',
    root: sceneDir,
    summary
  }, null, 2));

  fs.rmSync(tempRoot, { recursive: true, force: true });
  if (!ok) {
    process.exit(1);
  }
}

function writeReport(reportPath, payload) {
  if (!reportPath) {
    return;
  }
  ensureParentDirectory(reportPath);
  fs.writeFileSync(reportPath, `${JSON.stringify(payload, null, 2)}\n`, 'utf8');
}

function updateTrendReport(trendPath, payload) {
  if (!trendPath) {
    return [];
  }
  const history = [];
  if (fs.existsSync(trendPath)) {
    try {
      const existing = JSON.parse(fs.readFileSync(trendPath, 'utf8'));
      if (existing && Array.isArray(existing.history)) {
        for (const item of existing.history) {
          history.push(item);
        }
      }
    } catch {
    }
  }

  const snapshot = {
    timestamp: payload.generatedAt,
    mode: payload.mode,
    schemaVersion: payload.materialSchemaVersion,
    scanned: payload.summary.scanned,
    changed: payload.summary.changed,
    unchanged: payload.summary.unchanged,
    failedCount: payload.summary.failed.length,
    changedByDirectory: payload.aggregations.changedByDirectory,
    failuresByReason: payload.aggregations.failuresByReason
  };
  history.push(snapshot);
  if (history.length > 50) {
    history.splice(0, history.length - 50);
  }
  const trendPayload = {
    schemaVersion: payload.materialSchemaVersion,
    updatedAt: payload.generatedAt,
    history
  };
  ensureParentDirectory(trendPath);
  fs.writeFileSync(trendPath, `${JSON.stringify(trendPayload, null, 2)}\n`, 'utf8');
  return history;
}

function buildTableRows(rows, columns) {
  if (!rows || rows.length === 0) {
    return '<tr><td colspan="99">No data</td></tr>';
  }
  return rows.map((row) => `<tr>${columns.map((c) => `<td>${escapeHtml(row[c] ?? '')}</td>`).join('')}</tr>`).join('\n');
}

function writeHtmlReport(htmlPath, payload, trendHistory) {
  if (!htmlPath) {
    return;
  }
  const latestTrend = trendHistory.slice(-10).map((item) => ({
    timestamp: item.timestamp,
    mode: item.mode,
    changed: item.changed,
    failedCount: item.failedCount
  }));
  const detailsPreview = payload.summary.details.slice(0, 50);
  const failedPreview = payload.summary.failed.slice(0, 50);
  const html = `<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>Scene Schema Migration Report</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f7f8fb; color: #222; }
    h1, h2 { margin: 12px 0; }
    .card { background: #fff; border: 1px solid #ddd; border-radius: 8px; padding: 12px; margin-bottom: 16px; }
    .grid { display: grid; grid-template-columns: repeat(4, minmax(120px, 1fr)); gap: 10px; }
    .k { font-size: 12px; color: #666; }
    .v { font-size: 20px; font-weight: bold; }
    table { width: 100%; border-collapse: collapse; background: #fff; }
    th, td { border: 1px solid #ddd; padding: 6px 8px; font-size: 12px; text-align: left; vertical-align: top; }
    th { background: #f0f2f7; }
  </style>
</head>
<body>
  <h1>Scene Schema Migration Report</h1>
  <div class="card">
    <div><strong>Generated:</strong> ${escapeHtml(payload.generatedAt)}</div>
    <div><strong>Root:</strong> ${escapeHtml(payload.root)}</div>
    <div><strong>Mode:</strong> ${escapeHtml(payload.mode)}</div>
    <div><strong>Schema Version:</strong> ${escapeHtml(payload.materialSchemaVersion)}</div>
  </div>
  <div class="card grid">
    <div><div class="k">Scanned</div><div class="v">${escapeHtml(payload.summary.scanned)}</div></div>
    <div><div class="k">Changed</div><div class="v">${escapeHtml(payload.summary.changed)}</div></div>
    <div><div class="k">Unchanged</div><div class="v">${escapeHtml(payload.summary.unchanged)}</div></div>
    <div><div class="k">Failed</div><div class="v">${escapeHtml(payload.summary.failed.length)}</div></div>
  </div>
  <h2>Changed By Directory</h2>
  <table><thead><tr><th>Directory</th><th>Count</th></tr></thead><tbody>${buildTableRows(payload.aggregations.changedByDirectory, ['directory', 'count'])}</tbody></table>
  <h2>Failures By Reason</h2>
  <table><thead><tr><th>Reason</th><th>Count</th></tr></thead><tbody>${buildTableRows(payload.aggregations.failuresByReason, ['reason', 'count'])}</tbody></table>
  <h2>Failures By Directory</h2>
  <table><thead><tr><th>Directory</th><th>Count</th></tr></thead><tbody>${buildTableRows(payload.aggregations.failuresByDirectory, ['directory', 'count'])}</tbody></table>
  <h2>Trend (Last 10 Runs)</h2>
  <table><thead><tr><th>Timestamp</th><th>Mode</th><th>Changed</th><th>Failed</th></tr></thead><tbody>${buildTableRows(latestTrend, ['timestamp', 'mode', 'changed', 'failedCount'])}</tbody></table>
  <h2>Changed Details (Preview)</h2>
  <table><thead><tr><th>File</th><th>Before</th><th>After</th><th>Materials</th></tr></thead><tbody>${buildTableRows(detailsPreview, ['file', 'beforeVersion', 'afterVersion', 'materialCount'])}</tbody></table>
  <h2>Failed Details (Preview)</h2>
  <table><thead><tr><th>File</th><th>Reason</th><th>Message</th></tr></thead><tbody>${buildTableRows(failedPreview, ['file', 'reason', 'message'])}</tbody></table>
</body>
</html>`;
  ensureParentDirectory(htmlPath);
  fs.writeFileSync(htmlPath, html, 'utf8');
}

const args = process.argv.slice(2);
if (args.includes('--self-test')) {
  runSelfTest();
  process.exit(0);
}

let rootDir = path.resolve(__dirname, '../../');
const rootIndex = args.indexOf('--root');
if (rootIndex >= 0 && args[rootIndex + 1]) {
  rootDir = path.resolve(args[rootIndex + 1]);
}
const writeMode = args.includes('--write');
const reportIndex = args.indexOf('--report');
const reportPath = reportIndex >= 0 && args[reportIndex + 1]
  ? path.resolve(args[reportIndex + 1])
  : '';
const trendIndex = args.indexOf('--trend-file');
const trendPath = trendIndex >= 0 && args[trendIndex + 1]
  ? path.resolve(args[trendIndex + 1])
  : '';
const htmlIndex = args.indexOf('--html-report');
const htmlPath = htmlIndex >= 0 && args[htmlIndex + 1]
  ? path.resolve(args[htmlIndex + 1])
  : '';
const failOnErrors = args.includes('--fail-on-errors');

const summary = runMigration(rootDir, writeMode);
const payload = {
  ok: true,
  generatedAt: new Date().toISOString(),
  mode: writeMode ? 'write' : 'dry-run',
  materialSchemaVersion: CURRENT_SCHEMA_VERSION,
  root: rootDir,
  summary,
  aggregations: {
    changedByDirectory: aggregateByDirectory(summary.details),
    failuresByReason: aggregateFailureReasons(summary.failed),
    failuresByDirectory: aggregateFailureDirectories(summary.failed)
  }
};
if (summary.failed.length > 0) {
  payload.ok = !failOnErrors;
}
writeReport(reportPath, payload);
const trendHistory = updateTrendReport(trendPath, payload);
writeHtmlReport(htmlPath, payload, trendHistory);
console.log(JSON.stringify(payload, null, 2));
if (!payload.ok) {
  process.exit(1);
}
