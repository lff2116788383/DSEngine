using System;

namespace DSEngine.Game;

/// <summary>
/// Sample script demonstrating C# scripting in DSEngine.
/// Must be declared partial for the source generator to emit Inspector metadata.
/// </summary>
public partial class SampleScript : DseScript {
    // ── Inspector-visible fields (auto-discovered by source generator) ────────
    public float RotationSpeed = 45.0f;

    [Range(0.1f, 10.0f)]
    public float SpeedMultiplier = 1.0f;

    public bool EnableRotation = true;

    public Vector3 RotationAxis = new(0, 1, 0);

    [HideInInspector]
    public int InternalCounter;

    // ── Private state ─────────────────────────────────────────────────────────
    private float _elapsed;

    public override void OnStart() {
        Console.WriteLine($"[SampleScript] OnStart — Entity {Entity.Id}");
        var pos = Entity.Transform.Position;
        Console.WriteLine($"[SampleScript] Initial position: {pos}");
    }

    public override void OnUpdate(float dt) {
        _elapsed += dt;

        if (!EnableRotation) return;

        var transform = Entity.Transform;
        var rotation = transform.Rotation;
        rotation.Y += RotationSpeed * SpeedMultiplier * dt;
        transform.Rotation = rotation;
    }

    public override void OnDestroy() {
        Console.WriteLine($"[SampleScript] OnDestroy — Entity {Entity.Id}");
    }
}
