using System.Text;

namespace DSEngine;

/// <summary>
/// High-level wrapper over MeshRendererComponent C ABI.
/// </summary>
public readonly struct MeshRenderer {
    private readonly uint _entity;
    public MeshRenderer(uint entity) => _entity = entity;

    public int Visible {
        get => Native.dse_mesh_renderer_get_visible(_entity);
        set => Native.dse_mesh_renderer_set_visible(_entity, value);
    }

    public int ReceiveShadow {
        get => Native.dse_mesh_renderer_get_receive_shadow(_entity);
        set => Native.dse_mesh_renderer_set_receive_shadow(_entity, value);
    }

    public float Metallic {
        get => Native.dse_mesh_renderer_get_metallic(_entity);
        set => Native.dse_mesh_renderer_set_metallic(_entity, value);
    }

    public float Roughness {
        get => Native.dse_mesh_renderer_get_roughness(_entity);
        set => Native.dse_mesh_renderer_set_roughness(_entity, value);
    }

    public Vector4 Color {
        get {
            Native.dse_mesh_renderer_get_color(_entity, out float x, out float y, out float z, out float w);
            return new Vector4(x, y, z, w);
        }
        set => Native.dse_mesh_renderer_set_color(_entity, value.X, value.Y, value.Z, value.W);
    }

    public Vector3 Emissive {
        get {
            Native.dse_mesh_renderer_get_emissive(_entity, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }
        set => Native.dse_mesh_renderer_set_emissive(_entity, value.X, value.Y, value.Z);
    }

    public string MeshPath {
        get {
            byte[] buf = new byte[512];
            int len = Native.dse_mesh_renderer_get_mesh_path(_entity, buf, buf.Length);
            return len > 0 ? Encoding.UTF8.GetString(buf, 0, len) : string.Empty;
        }
        set => Native.dse_mesh_renderer_set_mesh_path(_entity, value);
    }

    public string ShaderVariant {
        get {
            byte[] buf = new byte[256];
            int len = Native.dse_mesh_renderer_get_shader_variant(_entity, buf, buf.Length);
            return len > 0 ? Encoding.UTF8.GetString(buf, 0, len) : string.Empty;
        }
        set => Native.dse_mesh_renderer_set_shader_variant(_entity, value);
    }
}
