namespace DSEngine;

/// <summary>
/// Lightweight handle wrapping an ECS entity ID.
/// All component access goes through the C ABI via entity ID — no native pointers held.
/// </summary>
public readonly struct Entity : System.IEquatable<Entity> {
    public readonly uint Id;

    public Entity(uint id) => Id = id;

    public static Entity Create() => new(Native.dse_entity_create());
    public void Destroy() => Native.dse_entity_destroy(Id);
    public bool IsValid => Native.dse_entity_valid(Id) != 0;

    public static Entity Null => new(0);

    // Component accessors
    public Transform Transform => new(Id);
    public Camera3D Camera3D => new(Id);
    public MeshRenderer MeshRenderer => new(Id);

    public bool Equals(Entity other) => Id == other.Id;
    public override bool Equals(object? obj) => obj is Entity e && Equals(e);
    public override int GetHashCode() => (int)Id;
    public static bool operator ==(Entity a, Entity b) => a.Id == b.Id;
    public static bool operator !=(Entity a, Entity b) => a.Id != b.Id;
    public override string ToString() => $"Entity({Id})";
}
