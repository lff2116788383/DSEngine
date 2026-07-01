namespace DSEngine;

/// <summary>
/// Base class for user scripts — similar to Unity MonoBehaviour.
/// Subclass this and override lifecycle methods.
/// Entity is assigned by the runtime before OnStart is called.
/// </summary>
public abstract class DseScript {
    /// <summary>The entity this script is attached to.</summary>
    public Entity Entity { get; internal set; }

    /// <summary>Called once when the script is first activated.</summary>
    public virtual void OnStart() {}

    /// <summary>Called every frame.</summary>
    public virtual void OnUpdate(float dt) {}

    /// <summary>Called at fixed timestep (physics tick).</summary>
    public virtual void OnFixedUpdate(float dt) {}

    /// <summary>Called when the script or entity is destroyed.</summary>
    public virtual void OnDestroy() {}
}
