-- 3D sample: modular character outfit / clothing swap
--
-- Demonstrates the cross-entity skeleton reference (MeshRendererComponent.skeleton_entity):
-- one skeleton-owning root entity (Animator3DComponent) drives many clothing part
-- entities. Each part is a skinned mesh whose bone matrices are read from the root's
-- Animator3D via dse.mesh_renderer_set_skeleton(part, root) -- no per-part animator.
local CharacterOutfit = {}

CharacterOutfit._meta = {
    name     = "character outfit swap",
    category = "animation",
    config   = {
        camera_distance = 6.0,
        skel_path     = "animation/minimal_rig/two_bone.dskel",
        base_anim_path= "animation/minimal_rig/two_bone_idle_walk.danim",
        body_mesh     = "animation/minimal_rig/two_bone.dmesh",
        top_mesh      = "animation/minimal_rig/two_bone.dmesh",
        hair_mesh     = "animation/minimal_rig/two_bone.dmesh",
    },
}

-- UINT32_MAX: sentinel meaning "no skeleton reference" (matches the C ABI /
-- dse_ik_set_target_entity convention). Passed to mesh_renderer_set_skeleton to clear.
local NO_SKELETON = 4294967295

local SLOTS = { "body", "hair", "top", "bottom", "shoes", "accessory" }

local state = { camera = nil, character = nil, time = 0.0, next_swap = 2.0, top_on = true }

-- Create a modular character: a skeleton root (owns the Animator3D) plus one child
-- entity per clothing slot, each wired to the root's skeleton and hidden until equipped.
function CharacterOutfit.create_character(skel_path, base_anim_path)
    local root = dse.ecs.create_entity()
    dse.ecs.add_transform(root, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)

    -- Root owns the skeleton/animator and renders the base body from its own Animator3D.
    dse.ecs.add_animator_3d(root, base_anim_path, skel_path)
    dse.ecs.init_animator_3d_fsm(root)
    dse.ecs.add_animator_3d_state(root, "idle", base_anim_path, true, 1.0)
    dse.ecs.set_animator_3d_state(root, "idle", 1.0, true)
    dse.ecs.add_mesh_renderer(root, 0.85, 0.75, 0.65, 1.0)
    dse.ecs.set_mesh_shader_variant(root, "MESH_LIT")
    -- skeleton_entity defaults to null => root reads its own Animator3D. Nothing to wire.

    local character = { root = root, parts = {} }

    for _, slot in ipairs(SLOTS) do
        local part = dse.ecs.create_entity()
        dse.ecs.add_transform(part, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ecs.set_parent(part, root)
        dse.ecs.add_mesh_renderer(part, 1.0, 1.0, 1.0, 1.0)
        dse.ecs.set_mesh_shader_variant(part, "MESH_LIT")
        -- Key wiring: this part is skinned by the ROOT's skeleton, not its own.
        dse.mesh_renderer_set_skeleton(part, root)
        dse.ecs.set_mesh_visible(part, false)
        character.parts[slot] = part
    end

    return character
end

-- Equip a mesh into a slot: swap the mesh and make the part visible.
function CharacterOutfit.equip(character, slot, mesh_path)
    local part = character.parts[slot]
    if not part then return end
    dse.ecs.set_mesh_path(part, mesh_path)
    dse.ecs.set_mesh_visible(part, true)
end

-- Unequip a slot: hide the part (mesh + skeleton wiring are retained for re-equip).
function CharacterOutfit.unequip(character, slot)
    local part = character.parts[slot]
    if not part then return end
    dse.ecs.set_mesh_visible(part, false)
end

-- Detach a slot entirely from the skeleton (e.g. before destroying / reusing it).
function CharacterOutfit.detach(character, slot)
    local part = character.parts[slot]
    if not part then return end
    dse.mesh_renderer_set_skeleton(part, NO_SKELETON)
    dse.ecs.set_mesh_visible(part, false)
end

-- Report which slots are currently visible: { slot = true, ... }.
function CharacterOutfit.get_equipped(character)
    local result = {}
    for slot, part in pairs(character.parts) do
        if dse.ecs.get_mesh_visible(part) then
            result[slot] = true
        end
    end
    return result
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 6.0
    dse.ecs.add_transform(camera, 0.0, 1.6, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -12.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    state.camera = camera
end

function CharacterOutfit.Setup(config)
    config = config or {}
    setup_camera(config)

    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.32, 1.0, 0.94, 0.86, 1.2, 0.18, 0.35)

    local skel = config.skel_path or CharacterOutfit._meta.config.skel_path
    local base = config.base_anim_path or CharacterOutfit._meta.config.base_anim_path
    state.character = CharacterOutfit.create_character(skel, base)

    CharacterOutfit.equip(state.character, "body", config.body_mesh or CharacterOutfit._meta.config.body_mesh)
    CharacterOutfit.equip(state.character, "top",  config.top_mesh  or CharacterOutfit._meta.config.top_mesh)
    CharacterOutfit.equip(state.character, "hair", config.hair_mesh or CharacterOutfit._meta.config.hair_mesh)

    local equipped = {}
    for slot, _ in pairs(CharacterOutfit.get_equipped(state.character)) do
        table.insert(equipped, slot)
    end
    table.sort(equipped)
    print(string.format("[3D][Outfit] character created root=%d equipped=%s", state.character.root, table.concat(equipped, ",")))
end

function CharacterOutfit.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    if state.character ~= nil and state.time >= state.next_swap then
        state.next_swap = state.time + 2.0
        state.top_on = not state.top_on
        if state.top_on then
            CharacterOutfit.equip(state.character, "top", CharacterOutfit._meta.config.top_mesh)
        else
            CharacterOutfit.unequip(state.character, "top")
        end
        print(string.format("[3D][Outfit] top %s", state.top_on and "equipped" or "unequipped"))
    end
end

return CharacterOutfit
