using System.Reflection;
using UTool.Infrastructure.Security;
using UTool.Sdk;
using SdkAssetPatch = UTool.Sdk.AssetPatch;
using SdkPlayerDataPatch = UTool.Sdk.PlayerDataPatch;

namespace UTool.ModLoader;

public sealed class CodeAssetPatch
{
    public required string AssetFileName { get; init; }
    public required SdkAssetPatch Instance { get; init; }
    public required Type PatchType { get; init; }
}

public static class ModCodePatchRunner
{
    private static readonly Dictionary<string, ModAssemblySandbox> ActiveSandboxes = new(StringComparer.OrdinalIgnoreCase);

    public static ModPatchBundle LoadFromAssembly(string assemblyPath, string? modId = null) =>
        new()
        {
            AssetPatches = LoadAssetPatches(assemblyPath, modId),
            PlayerDataPatches = LoadPlayerDataPatches(assemblyPath, modId),
        };

    public static IReadOnlyList<CodeAssetPatch> LoadAssetPatches(string assemblyPath, string? modId = null)
    {
        var assembly = LoadAssembly(assemblyPath, modId);
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

        return patches;
    }

    public static IReadOnlyList<CodePlayerDataPatch> LoadPlayerDataPatches(string assemblyPath, string? modId = null)
    {
        var assembly = LoadAssembly(assemblyPath, modId);
        var patches = new List<CodePlayerDataPatch>();

        foreach (var type in assembly.GetTypes())
        {
            if (type.IsAbstract || !typeof(SdkPlayerDataPatch).IsAssignableFrom(type))
                continue;

            var attr = type.GetCustomAttribute<PatchPlayerDataAttribute>();
            if (attr is null)
                continue;

            var instance = (SdkPlayerDataPatch?)Activator.CreateInstance(type)
                ?? throw new InvalidOperationException($"Could not create patch type {type.FullName}");

            patches.Add(new CodePlayerDataPatch
            {
                RelativePath = attr.RelativePath,
                Instance = instance,
                PatchType = type,
            });
        }

        return patches;
    }

    private static Assembly LoadAssembly(string assemblyPath, string? modId)
    {
        modId ??= Path.GetFileNameWithoutExtension(assemblyPath);
        if (ActiveSandboxes.TryGetValue(modId, out var existing))
        {
            existing.Dispose();
            ActiveSandboxes.Remove(modId);
        }

        var sandbox = new ModAssemblySandbox(modId, typeof(SdkAssetPatch).Assembly.GetName().Version?.ToString());
        ActiveSandboxes[modId] = sandbox;
        return sandbox.Load(assemblyPath);
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

    public static string ApplyAll(string sourceJson, IEnumerable<SdkAssetPatch> patches, IPlayerSaveContext? saves = null)
    {
        var current = sourceJson;
        foreach (var patch in patches)
        {
            if (patch is ConditionalAssetPatch conditional
                && saves is not null
                && !conditional.ShouldApply(saves))
                continue;

            current = Apply(current, patch);
        }

        return current;
    }

    public static string ApplyPlayerData(string sourceJson, SdkPlayerDataPatch patch, PlayerDataApplyContext context)
    {
        var editor = new JsonAssetEditor(sourceJson);
        patch.Apply(editor, context);
        return editor.ToJson();
    }

    public static IReadOnlyList<SdkAssetPatch> FilterActivePatches(
        IEnumerable<CodeAssetPatch> patches,
        IPlayerSaveContext? saves)
    {
        var list = new List<SdkAssetPatch>();
        foreach (var p in patches)
        {
            if (p.Instance is ConditionalAssetPatch conditional
                && saves is not null
                && !conditional.ShouldApply(saves))
                continue;

            list.Add(p.Instance);
        }

        return list;
    }
}
