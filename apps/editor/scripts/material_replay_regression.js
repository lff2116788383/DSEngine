const crypto = require('crypto');
const path = require('path');

const bridgePath = path.resolve(__dirname, '../build/Release/dsengine_bridge.node');
const dsengine = require(bridgePath);

function hashBuffer(buffer) {
  return crypto.createHash('sha256').update(Buffer.from(buffer)).digest('hex');
}

function fail(message, detail) {
  console.error(JSON.stringify({ ok: false, message, detail }, null, 2));
  process.exit(1);
}

const init = dsengine.initEngine();
if (!init || !init.status) {
  fail('init_failed', init);
}

dsengine.clearMaterialHotUpdateEvents();

const entities = dsengine.getEntities();
if (!Array.isArray(entities) || entities.length === 0) {
  fail('entities_empty', entities);
}

const targetEntity = entities.find((e) => e.name === 'Player') || entities.find((e) => typeof e.id === 'number');
if (!targetEntity) {
  fail('target_entity_not_found', entities);
}

const createResult = dsengine.createMaterialInstance('regression_replay_material', 'SPRITE_TINT', 0);
if (!createResult || !createResult.success || !createResult.id) {
  fail('create_material_failed', createResult);
}
const materialId = createResult.id;

if (!dsengine.applyMaterialToEntity(targetEntity.id, materialId)) {
  fail('apply_material_failed', { targetEntity, materialId });
}

const firstUpdate = dsengine.updateMaterialInstance(materialId, {
  shaderVariant: 'SPRITE_TINT',
  blendMode: 0,
  textureHandle: 0,
  tint: [0.9, 0.2, 0.2, 1.0],
  uv: [0.0, 0.0, 1.0, 1.0]
});
if (!firstUpdate) {
  fail('first_update_failed');
}

const secondUpdate = dsengine.updateMaterialInstance(materialId, {
  shaderVariant: 'SPRITE_TINT',
  blendMode: 2,
  textureHandle: 0,
  tint: [0.2, 0.9, 0.3, 1.0],
  uv: [0.1, 0.1, 0.8, 0.8]
});
if (!secondUpdate) {
  fail('second_update_failed');
}

const eventsBeforeDiverge = dsengine.listMaterialHotUpdateEvents();
if (!Array.isArray(eventsBeforeDiverge) || eventsBeforeDiverge.length < 2) {
  fail('events_insufficient', eventsBeforeDiverge);
}
const replayToSequence = eventsBeforeDiverge[eventsBeforeDiverge.length - 1].sequence;

const expectedHash = hashBuffer(dsengine.getFrameBuffer());

const divergeUpdate = dsengine.updateMaterialInstance(materialId, {
  shaderVariant: 'SPRITE_TINT',
  blendMode: 0,
  textureHandle: 0,
  tint: [0.05, 0.05, 0.95, 1.0],
  uv: [0.25, 0.25, 0.5, 0.5]
});
if (!divergeUpdate) {
  fail('diverge_update_failed');
}

const divergedHash = hashBuffer(dsengine.getFrameBuffer());
if (divergedHash === expectedHash) {
  fail('diverge_hash_not_changed', { expectedHash, divergedHash });
}

const replayResult = dsengine.replayMaterialHotUpdates(replayToSequence);
if (!replayResult || !replayResult.success) {
  fail('replay_failed', replayResult);
}

const replayedHash = hashBuffer(dsengine.getFrameBuffer());
if (replayedHash !== expectedHash) {
  fail('replay_hash_mismatch', { expectedHash, replayedHash, divergedHash, replayResult, replayToSequence });
}

console.log(JSON.stringify({
  ok: true,
  materialId,
  targetEntityId: targetEntity.id,
  replayToSequence,
  replayApplied: replayResult.applied,
  expectedHash,
  divergedHash,
  replayedHash
}, null, 2));
