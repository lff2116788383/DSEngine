using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;

namespace DSEngine.ScriptMeta;

/// <summary>
/// Build-time producer for the shared <c>.dscriptmeta</c> contract
/// (see engine/scripting/script_metadata.{h,cpp}). Reflects over a built
/// managed assembly, finds every <c>DseScript</c> subclass, and emits one
/// versioned JSON file per class describing its Inspector-visible fields.
///
/// Pure metadata read: types/fields/attributes only — no instance is
/// constructed, so no user runtime code executes.
///
/// Usage: DSEngine.ScriptMeta &lt;assembly.dll&gt; &lt;outputDir&gt; [more.dll ...]
/// </summary>
internal static class Program
{
    private const int SchemaVersion = 1;

    private static int Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.Error.WriteLine(
                "usage: DSEngine.ScriptMeta <assembly.dll> <outputDir> [extra.dll ...]");
            return 2;
        }

        var outputDir = args[1];
        var assemblyPaths = new List<string> { args[0] };
        assemblyPaths.AddRange(args.Skip(2));

        try
        {
            Directory.CreateDirectory(outputDir);
            var probeDir = Path.GetDirectoryName(Path.GetFullPath(args[0])) ?? ".";
            var alc = new ProbingLoadContext(probeDir);

            int written = 0;
            foreach (var asmPath in assemblyPaths)
            {
                var asm = alc.LoadFromAssemblyPath(Path.GetFullPath(asmPath));
                foreach (var type in SafeGetTypes(asm))
                {
                    if (type is null || !InheritsDseScript(type)) continue;
                    var json = BuildJson(type);
                    var fileName = (type.FullName ?? type.Name) + ".dscriptmeta";
                    File.WriteAllText(Path.Combine(outputDir, fileName), json, new UTF8Encoding(false));
                    written++;
                    Console.WriteLine($"[ScriptMeta] {fileName}");
                }
            }
            Console.WriteLine($"[ScriptMeta] wrote {written} file(s) to {outputDir}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("[ScriptMeta] failed: " + ex.Message);
            return 1;
        }
    }

    private static IEnumerable<Type?> SafeGetTypes(Assembly asm)
    {
        try { return asm.GetTypes(); }
        catch (ReflectionTypeLoadException ex) { return ex.Types; }
    }

    private static bool InheritsDseScript(Type type)
    {
        if (type.IsAbstract) return false;
        for (var t = type.BaseType; t is not null; t = t.BaseType)
        {
            if (t.Name == "DseScript") return true;
        }
        return false;
    }

    private static string BuildJson(Type type)
    {
        var fields = new StringBuilder();
        int count = 0;
        foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.Instance))
        {
            if (field.IsInitOnly || field.IsLiteral) continue;
            if (HasAttribute(field, "HideInInspectorAttribute")) continue;
            var mapped = MapFieldType(field.FieldType);
            if (mapped is null) continue;

            if (count > 0) fields.Append(',');
            fields.Append("{\"name\":").Append(Quote(field.Name))
                  .Append(",\"type\":").Append(Quote(mapped))
                  .Append(",\"tooltip\":\"\"")
                  .Append(",\"number_default\":[0.0,0.0,0.0,0.0]")
                  .Append(",\"string_default\":\"\"}");
            count++;
        }

        var sb = new StringBuilder();
        sb.Append("{\"version\":").Append(SchemaVersion).Append(",\"script\":{")
          .Append("\"class_name\":").Append(Quote(type.Name))
          .Append(",\"full_name\":").Append(Quote(type.FullName ?? type.Name))
          .Append(",\"base_type\":").Append(Quote(type.BaseType?.Name ?? ""))
          .Append(",\"assembly\":").Append(Quote(type.Assembly.GetName().Name ?? ""))
          .Append(",\"source_path\":\"\"")
          .Append(",\"fields\":[").Append(fields).Append("]}}");
        return sb.ToString();
    }

    private static bool HasAttribute(FieldInfo field, string attrName)
    {
        foreach (var data in field.GetCustomAttributesData())
        {
            if (data.AttributeType.Name == attrName) return true;
        }
        return false;
    }

    private static string? MapFieldType(Type type)
    {
        switch (Type.GetTypeCode(type))
        {
            case TypeCode.Int32: return "Int";
            case TypeCode.Single: return "Float";
            case TypeCode.Double: return "Float";
            case TypeCode.Boolean: return "Bool";
            case TypeCode.String: return "String";
        }
        return type.FullName switch
        {
            "System.Numerics.Vector2" => "Vec2",
            "System.Numerics.Vector3" => "Vec3",
            "System.Numerics.Vector4" => "Vec4",
            "DSEngine.Vector2" => "Vec2",
            "DSEngine.Vector3" => "Vec3",
            "DSEngine.Vector4" => "Vec4",
            "DSEngine.Color" or "UnityEngine.Color" => "Color",
            _ => null,
        };
    }

    private static string Quote(string s)
    {
        var sb = new StringBuilder(s.Length + 2);
        sb.Append('"');
        foreach (var c in s)
        {
            switch (c)
            {
                case '"': sb.Append("\\\""); break;
                case '\\': sb.Append("\\\\"); break;
                case '\n': sb.Append("\\n"); break;
                case '\r': sb.Append("\\r"); break;
                case '\t': sb.Append("\\t"); break;
                default:
                    if (c < 0x20)
                        sb.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
                    else
                        sb.Append(c);
                    break;
            }
        }
        sb.Append('"');
        return sb.ToString();
    }

    private sealed class ProbingLoadContext : AssemblyLoadContext
    {
        private readonly string _dir;
        public ProbingLoadContext(string dir) : base(isCollectible: false) => _dir = dir;

        protected override Assembly? Load(AssemblyName name)
        {
            var candidate = Path.Combine(_dir, name.Name + ".dll");
            return File.Exists(candidate) ? LoadFromAssemblyPath(candidate) : null;
        }
    }
}
