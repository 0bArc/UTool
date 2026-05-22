using UTool.ModLoader.Merge;

using UTool.Pak.Models;



namespace UTool.Pak;



public sealed class PakMergeBuildOptions

{

    public required IReadOnlyList<string> PakPathsInOrder { get; init; }

    public required string OutputPakPath { get; init; }

    public PakOpenOptions? PakOpenOptions { get; init; }

    public PakBuildOptions? BuildOptions { get; init; }

    public string? ConflictReportDirectory { get; init; }

    public string? FilesDirectory { get; init; }

    public bool JsonMerge { get; init; } = true;

    public bool ClearExtractedDirectory { get; init; } = true;

    public bool PreferUnrealPak { get; init; } = true;

    public UnrealPakOptions? UnrealPakOptions { get; init; }

    public Action<string>? Log { get; init; }

}



public sealed class PakMergeBuildResult

{

    public required PakBuildResult Build { get; init; }

    public required string ExtractedFilesDirectory { get; init; }

    public required IReadOnlyList<PakOverlapConflict> Overlaps { get; init; }

    public IReadOnlyList<MergeConflictReport> JsonReports { get; init; } = [];

    public int JsonMergeCount { get; init; }

}



/// <summary>Detect overlaps, extract to EXTRACTED-MOD/FILES, merge JSON, repack.</summary>

public static class PakMergePipeline

{

    public static PakMergeBuildResult MergeBuild(PakMergeBuildOptions options)

    {

        if (options.PakPathsInOrder.Count == 0)

            throw new ArgumentException("At least one pak path is required.");



        var overlap = PakOverlapChecker.Analyze(options.PakPathsInOrder, options.PakOpenOptions);

        var run = PakMergeExecutor.Run(new PakMergeRunRequest

        {

            PakPathsInOrder = options.PakPathsInOrder,

            OutputPakPath = options.OutputPakPath,

            PakOpenOptions = options.PakOpenOptions,

            BuildOptions = options.BuildOptions,

            FilesDirectory = options.FilesDirectory,

            ConflictReportDirectory = options.ConflictReportDirectory,

            JsonMerge = options.JsonMerge,

            ClearExtractedDirectory = options.ClearExtractedDirectory,

            PreferUnrealPak = options.PreferUnrealPak,

            UnrealPakOptions = options.UnrealPakOptions,

            Log = options.Log,

        });



        return new PakMergeBuildResult

        {

            Build = run.Build,

            ExtractedFilesDirectory = run.ExtractedFilesDirectory,

            Overlaps = overlap.Conflicts,

            JsonReports = run.JsonReports,

            JsonMergeCount = run.JsonMergeCount,

        };

    }

}


