using CsStratware.ModLoader.Merge;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

/// <summary>Extract all mod paks to EXTRACTED-MOD/FILES, merge JSON in place, repack.</summary>
public static class PakMergeExecutor
{
    public static PakMergeRunResult Run(PakMergeRunRequest request)
    {
        if (request.PakPathsInOrder.Count == 0)
            throw new ArgumentException("At least one pak path is required.", nameof(request));

        var openOptions = request.PakOpenOptions;
        var filesDir = request.FilesDirectory
            ?? PakMergePaths.ResolveFilesDirectory(request.OutputPakPath, request.PakPathsInOrder);

        PakMergePaths.PrepareFilesDirectory(filesDir, clearExisting: request.ClearExtractedDirectory);

        var mergeMount = PakMergeStaging.ResolveCommonMountPoint(request.PakPathsInOrder, openOptions);
        var workset = PakMergeStaging.Stage(
            request.PakPathsInOrder,
            filesDir,
            openOptions,
            mergeMount,
            request.Log);

        var jsonReports = new List<MergeConflictReport>();
        var jsonMergeCount = PakMergeStaging.ApplyJsonOverlaps(
            workset,
            filesDir,
            request.JsonMerge,
            jsonReports,
            request.ConflictReportDirectory,
            request.Log);

        PakMergeStaging.RemoveMergeArtifacts(filesDir);
        PakMergeStaging.RefreshWorksetFiles(workset, filesDir);

        var build = PakMergeRepack.Build(
            workset,
            filesDir,
            request.OutputPakPath,
            request.PakPathsInOrder,
            openOptions,
            new PakMergeOptions
            {
                PakOpenOptions = openOptions,
                JsonMerge = request.JsonMerge,
                BuildOptions = request.BuildOptions,
                PreferUnrealPak = request.PreferUnrealPak,
                UnrealPakOptions = request.UnrealPakOptions,
            });

        return new PakMergeRunResult
        {
            Build = build,
            ExtractedFilesDirectory = filesDir,
            JsonMergeCount = jsonMergeCount,
            JsonReports = jsonReports,
        };
    }
}

public sealed class PakMergeRunRequest
{
    public required IReadOnlyList<string> PakPathsInOrder { get; init; }
    public required string OutputPakPath { get; init; }
    public PakOpenOptions? PakOpenOptions { get; init; }
    public PakBuildOptions? BuildOptions { get; init; }
    public string? FilesDirectory { get; init; }
    public string? ConflictReportDirectory { get; init; }
    public bool JsonMerge { get; init; } = true;
    public bool ClearExtractedDirectory { get; init; } = true;
    public bool PreferUnrealPak { get; init; } = true;
    public UnrealPakOptions? UnrealPakOptions { get; init; }
    public Action<string>? Log { get; init; }
}

public sealed class PakMergeRunResult
{
    public required PakBuildResult Build { get; init; }
    public required string ExtractedFilesDirectory { get; init; }
    public int JsonMergeCount { get; init; }
    public IReadOnlyList<MergeConflictReport> JsonReports { get; init; } = [];
}
