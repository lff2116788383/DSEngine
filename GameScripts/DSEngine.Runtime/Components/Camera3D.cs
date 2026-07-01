namespace DSEngine;

/// <summary>
/// High-level wrapper over Camera3DComponent C ABI.
/// </summary>
public readonly struct Camera3D {
    private readonly uint _entity;
    public Camera3D(uint entity) => _entity = entity;

    public float Fov {
        get => Native.dse_camera3d_get_fov(_entity);
        set => Native.dse_camera3d_set_fov(_entity, value);
    }

    public float NearClip {
        get => Native.dse_camera3d_get_near_clip(_entity);
        set => Native.dse_camera3d_set_near_clip(_entity, value);
    }

    public float FarClip {
        get => Native.dse_camera3d_get_far_clip(_entity);
        set => Native.dse_camera3d_set_far_clip(_entity, value);
    }
}
