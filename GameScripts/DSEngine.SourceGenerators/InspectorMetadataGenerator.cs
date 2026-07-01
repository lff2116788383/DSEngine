using System.Collections.Immutable;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;

namespace DSEngine.SourceGenerators;

/// <summary>
/// Roslyn incremental source generator that emits compile-time Inspector metadata
/// for every DseScript subclass — eliminates runtime reflection for the editor.
///
/// For each class Foo : DseScript with public fields, generates:
///   partial class Foo {
///       public static readonly InspectorFieldInfo[] __InspectorFields = [ ... ];
///   }
///
/// The engine C++ side reads this array via interop to populate the Inspector panel.
/// </summary>
[Generator(LanguageNames.CSharp)]
public sealed class InspectorMetadataGenerator : IIncrementalGenerator
{
    public void Initialize(IncrementalGeneratorInitializationContext context)
    {
        // Register the attribute source (marker attribute for opt-out)
        context.RegisterPostInitializationOutput(static ctx =>
        {
            ctx.AddSource("InspectorFieldInfo.g.cs", SourceText.From(InspectorFieldInfoSource, Encoding.UTF8));
            ctx.AddSource("HideInInspectorAttribute.g.cs", SourceText.From(HideInInspectorSource, Encoding.UTF8));
            ctx.AddSource("RangeAttribute.g.cs", SourceText.From(RangeAttributeSource, Encoding.UTF8));
        });

        // Pipeline: find all class declarations that inherit DseScript
        var scriptClasses = context.SyntaxProvider
            .CreateSyntaxProvider(
                predicate: static (node, _) => node is ClassDeclarationSyntax cds
                    && cds.BaseList is not null
                    && !cds.Modifiers.Any(SyntaxKind.AbstractKeyword),
                transform: static (ctx, ct) =>
                {
                    var cds = (ClassDeclarationSyntax)ctx.Node;
                    var symbol = ctx.SemanticModel.GetDeclaredSymbol(cds, ct);
                    if (symbol is null) return null;
                    if (!InheritsDseScript(symbol)) return null;
                    return symbol;
                })
            .Where(static s => s is not null)
            .Select(static (s, _) => s!);

        // Generate
        context.RegisterSourceOutput(scriptClasses, static (spc, classSymbol) =>
        {
            var source = GenerateForClass(classSymbol);
            if (source is not null)
            {
                var hint = $"{classSymbol.ContainingNamespace}.{classSymbol.Name}.InspectorMeta.g.cs"
                    .Replace("<", "_").Replace(">", "_");
                spc.AddSource(hint, SourceText.From(source, Encoding.UTF8));
            }
        });
    }

    private static bool InheritsDseScript(INamedTypeSymbol symbol)
    {
        var current = symbol.BaseType;
        while (current is not null)
        {
            if (current.Name == "DseScript" &&
                (current.ContainingNamespace?.ToString() == "DSEngine" ||
                 current.ContainingNamespace?.ToString() == ""))
                return true;
            current = current.BaseType;
        }
        return false;
    }

    private static string? GenerateForClass(INamedTypeSymbol classSymbol)
    {
        var fields = new StringBuilder();
        int count = 0;

        foreach (var member in classSymbol.GetMembers())
        {
            if (member is not IFieldSymbol field) continue;
            if (field.DeclaredAccessibility != Accessibility.Public) continue;
            if (field.IsStatic || field.IsConst) continue;
            if (field.IsImplicitlyDeclared) continue;

            // Check [HideInInspector]
            bool hidden = false;
            float rangeMin = 0, rangeMax = 0;
            bool hasRange = false;

            foreach (var attr in field.GetAttributes())
            {
                var attrName = attr.AttributeClass?.Name ?? "";
                if (attrName == "HideInInspectorAttribute" || attrName == "HideInInspector")
                    hidden = true;
                if (attrName == "RangeAttribute" || attrName == "Range")
                {
                    if (attr.ConstructorArguments.Length >= 2)
                    {
                        rangeMin = Convert.ToSingle(attr.ConstructorArguments[0].Value);
                        rangeMax = Convert.ToSingle(attr.ConstructorArguments[1].Value);
                        hasRange = true;
                    }
                }
            }

            if (hidden) continue;

            var fieldType = MapFieldType(field.Type);
            if (fieldType == "Unknown") continue;

            if (count > 0) fields.Append(",\n            ");
            fields.Append($"new InspectorFieldInfo(\"{field.Name}\", InspectorFieldType.{fieldType}");
            if (hasRange)
                fields.Append($", {rangeMin}f, {rangeMax}f");
            fields.Append(')');
            count++;
        }

        if (count == 0) return null;

        var ns = classSymbol.ContainingNamespace?.IsGlobalNamespace == true
            ? null
            : classSymbol.ContainingNamespace?.ToString();

        var sb = new StringBuilder();
        sb.AppendLine("// <auto-generated/>");
        sb.AppendLine("using DSEngine;");
        sb.AppendLine();
        if (ns is not null)
        {
            sb.AppendLine($"namespace {ns};");
            sb.AppendLine();
        }
        sb.AppendLine($"partial class {classSymbol.Name}");
        sb.AppendLine("{");
        sb.AppendLine($"    public static readonly InspectorFieldInfo[] __InspectorFields = new InspectorFieldInfo[]");
        sb.AppendLine("    {");
        sb.AppendLine($"            {fields}");
        sb.AppendLine("    };");
        sb.AppendLine("}");

        return sb.ToString();
    }

    private static string MapFieldType(ITypeSymbol type)
    {
        var name = type.SpecialType switch
        {
            SpecialType.System_Int32 => "Int",
            SpecialType.System_Single => "Float",
            SpecialType.System_Double => "Double",
            SpecialType.System_Boolean => "Bool",
            SpecialType.System_String => "String",
            _ => null
        };
        if (name is not null) return name;

        var fullName = type.ToDisplayString();
        return fullName switch
        {
            "DSEngine.Vector3" => "Vector3",
            "DSEngine.Vector4" => "Vector4",
            "System.Numerics.Vector3" => "Vector3",
            "System.Numerics.Vector4" => "Vector4",
            "UnityEngine.Color" or "DSEngine.Color" => "Color",
            _ => "Unknown"
        };
    }

    // ── Injected source (always added) ───────────────────────────────────────

    private const string InspectorFieldInfoSource = @"
namespace DSEngine;

/// <summary>Field type as seen by the Inspector panel.</summary>
public enum InspectorFieldType
{
    Int,
    Float,
    Double,
    Bool,
    String,
    Vector3,
    Vector4,
    Color,
}

/// <summary>
/// Compile-time metadata for a single inspectable field.
/// Generated by DSEngine.SourceGenerators — do not write manually.
/// </summary>
public readonly struct InspectorFieldInfo
{
    public readonly string Name;
    public readonly InspectorFieldType FieldType;
    public readonly float RangeMin;
    public readonly float RangeMax;
    public readonly bool HasRange;

    public InspectorFieldInfo(string name, InspectorFieldType fieldType,
                              float rangeMin = 0f, float rangeMax = 0f)
    {
        Name = name;
        FieldType = fieldType;
        RangeMin = rangeMin;
        RangeMax = rangeMax;
        HasRange = rangeMin != 0f || rangeMax != 0f;
    }
}
";

    private const string HideInInspectorSource = @"
namespace DSEngine;

/// <summary>Prevents a public field from appearing in the Inspector.</summary>
[System.AttributeUsage(System.AttributeTargets.Field)]
public sealed class HideInInspectorAttribute : System.Attribute { }
";

    private const string RangeAttributeSource = @"
namespace DSEngine;

/// <summary>Constrains a numeric field to [min, max] in the Inspector slider.</summary>
[System.AttributeUsage(System.AttributeTargets.Field)]
public sealed class RangeAttribute : System.Attribute
{
    public float Min { get; }
    public float Max { get; }

    public RangeAttribute(float min, float max)
    {
        Min = min;
        Max = max;
    }
}
";
}
