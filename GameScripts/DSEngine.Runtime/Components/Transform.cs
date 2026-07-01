namespace DSEngine;

/// <summary>
/// High-level wrapper over TransformComponent C ABI getters/setters.
/// Accesses the ECS component through entity ID — safe across hot-reload.
/// </summary>
public readonly struct Transform {
    private readonly uint _entity;
    public Transform(uint entity) => _entity = entity;

    public Vector3 Position {
        get {
            Native.dse_transform_get_position(_entity, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }
        set => Native.dse_transform_set_position(_entity, value.X, value.Y, value.Z);
    }

    public Vector3 Rotation {
        get {
            Native.dse_transform_get_rotation(_entity, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }
        set => Native.dse_transform_set_rotation(_entity, value.X, value.Y, value.Z);
    }

    public Vector3 Scale {
        get {
            Native.dse_transform_get_scale(_entity, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }
        set => Native.dse_transform_set_scale(_entity, value.X, value.Y, value.Z);
    }
}
