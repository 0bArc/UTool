using System.Reflection;
using System.Runtime.Loader;
using CsStratware.Sdk;
using SdkAssetPatch = CsStratware.Sdk.AssetPatch;

namespace CsStratware.ModLoader;

public sealed class CodeAssetPatch
{
    public required string AssetFileName { get; init; }
    public required SdkAssetPatch Instance { get; init; }
    public required Type PatchType { get; init; }
}

public static class ModCodePatchRunner
{
    public static IReadOnlyList<CodeAssetPatch> LoadFromAssembly(string assemblyPath)
    {
        var fullPath = Path.GetFullPath(assemblyPath);
        var loadDir = Path.GetDirectoryName(fullPath)!;
        var context = new AssemblyLoadContext($"csstratware-mod-{Path.GetFileNameWithoutExtension(fullPath)}", isCollectible: true);
        context.Resolving += (_, name) =>
        {
            var candidate = Path.Combine(loadDir, $"{name.Name}.dll");
            return File.Exists(candidate) ? context.LoadFromAssemblyPath(candidate) : null;
        };

        var assembly = context.LoadFromAssemblyPath(fullPath);
        var patches = new List<CodeAssetPatch>();

        foreach (var type in assembly.GetTypes())
        {
            if (type.IsAbstract || !typeof(SdkAssetPatch).IsAssignableFrom(type))
                continue;

            var attr = type.GetCustomAttribute<PatchAssetAttribute>();
            if (attr is null)
                continue;

            var instance = (SdkAssetPatch?)Activator.CreateInstance(type)
                ?? throw new InvalidOperationException($"Could not create patch type {type.FullName}");

            patches.Add(new CodeAssetPatch
            {
                AssetFileName = attr.AssetFileName,
                Instance = instance,
                PatchType = type,
            });
        }

        if (patches.Count == 0)
            throw new InvalidOperationException($"No [PatchAsset] AssetPatch types found in {fullPath}");

        return patches;
    }

    public static string Apply(string sourceJson, SdkAssetPatch patch)
    {
        var editor = new JsonAssetEditor(sourceJson);
        patch.Apply(editor);
        return editor.ToJson();
    }

    public static string ApplyAll(string sourceJson, IEnumerable<SdkAssetPatch> patches)
    {
        var current = sourceJson;
        foreach (var patch in patches)
            current = Apply(current, patch);
        return current;
    }
}
