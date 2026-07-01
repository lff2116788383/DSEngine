using System;

namespace DSEngine.Game;

/// <summary>
/// Sample script demonstrating C# scripting in DSEngine.
/// </summary>
public class SampleScript : DseScript {
    private float _elapsed;

    public override void OnStart() {
        Console.WriteLine($"[SampleScript] OnStart — Entity {Entity.Id}");
        var pos = Entity.Transform.Position;
        Console.WriteLine($"[SampleScript] Initial position: {pos}");
    }

    public override void OnUpdate(float dt) {
        _elapsed += dt;

        // Rotate entity slowly around Y axis
        var transform = Entity.Transform;
        var rotation = transform.Rotation;
        rotation.Y += 45.0f * dt;
        transform.Rotation = rotation;
    }

    public override void OnDestroy() {
        Console.WriteLine($"[SampleScript] OnDestroy — Entity {Entity.Id}");
    }
}
