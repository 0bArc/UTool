using System.Reflection;
using CsStratware.Infrastructure.Security;
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
    private static readonly Dictionary<string, ModAssemblySandbox> ActiveSandboxes = new(StringComparer.OrdinalIgnoreCase);

    public static IReadOnlyList<CodeAssetPatch> LoadFromAssembly(string assemblyPath, string? modId = null)
    {
        modId ??= Path.GetFileNameWithoutExtension(assemblyPath);
        if (ActiveSandboxes.TryGetValue(modId, out var existing))
        {
            existing.Dispose();
            ActiveSandboxes.Remove(modId);
        }

        var sandbox = new ModAssemblySandbox(modId, typeof(SdkAssetPatch).Assembly.GetName().Version?.ToString());
        ActiveSandboxes[modId] = sandbox;
        var assembly = sandbox.Load(assemblyPath);
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
            throw new InvalidOperationException($"No [PatchAsset] AssetPatch types found in {assemblyPath}");

        return patches;
    }

    public static void UnloadMod(string modId)
    {
        if (ActiveSandboxes.Remove(modId, out var sandbox))
            sandbox.Dispose();
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
