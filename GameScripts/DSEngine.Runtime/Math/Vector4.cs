using System;
using System.Runtime.InteropServices;

namespace DSEngine;

[StructLayout(LayoutKind.Sequential)]
public struct Vector4 : IEquatable<Vector4> {
    public float X;
    public float Y;
    public float Z;
    public float W;

    public Vector4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }

    public static Vector4 Zero => new(0, 0, 0, 0);
    public static Vector4 One => new(1, 1, 1, 1);

    public static Vector4 operator +(Vector4 a, Vector4 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
    public static Vector4 operator -(Vector4 a, Vector4 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W);
    public static Vector4 operator *(Vector4 v, float s) => new(v.X * s, v.Y * s, v.Z * s, v.W * s);

    public bool Equals(Vector4 other) => X == other.X && Y == other.Y && Z == other.Z && W == other.W;
    public override bool Equals(object? obj) => obj is Vector4 v && Equals(v);
    public override int GetHashCode() => HashCode.Combine(X, Y, Z, W);
    public static bool operator ==(Vector4 a, Vector4 b) => a.Equals(b);
    public static bool operator !=(Vector4 a, Vector4 b) => !a.Equals(b);
    public override string ToString() => $"({X:F3}, {Y:F3}, {Z:F3}, {W:F3})";
}
