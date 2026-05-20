using CsStratware.ModLoader.Merge;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public sealed class PakMergeBuildOptions
{
    public required IReadOnlyList<string> PakPathsInOrder { get; init; }
    public required string OutputPakPath { get; init; }
    public PakOpenOptions? PakOpenOptions { get; init; }
    public PakBuildOptions? BuildOptions { get; init; }
    public string? ConflictReportDirectory { get; init; }
    public bool JsonMerge { get; init; } = true;
}

public sealed class PakMergeBuildResult
{
    public required PakBuildResult Build { get; init; }
    public required IReadOnlyList<PakOverlapConflict> Overlaps { get; init; }
    public IReadOnlyList<MergeConflictReport> JsonReports { get; init; } = [];
    public int JsonMergeCount { get; init; }
}

/// <summary>Detect overlaps, merge JSON DataTables, build unified pak.</summary>
public static class PakMergePipeline
{
    public static PakMergeBuildResult MergeBuild(PakMergeBuildOptions options)
    {
        if (options.PakPathsInOrder.Count == 0)
            throw new ArgumentException("At least one pak path is required.");

        var overlap = PakOverlapChecker.Analyze(options.PakPathsInOrder, options.PakOpenOptions);
        var jsonReports = new List<MergeConflictReport>();
        var jsonMergeCount = 0;

        if (!options.JsonMerge)
        {
            var buildOnly = PakMerger.Merge(options.PakPathsInOrder, options.OutputPakPath, new PakMergeOptions
            {
                PakOpenOptions = options.PakOpenOptions,
                JsonMerge = false,
                BuildOptions = options.BuildOptions,
            });
            return new PakMergeBuildResult
            {
                Build = buildOnly,
                Overlaps = overlap.Conflicts,
                JsonReports = jsonReports,
            };
        }

        var openOptions = options.PakOpenOptions;
        var tempDir = Path.Combine(Path.GetTempPath(), "csstratware-merge-build-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);

        try
        {
            var files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var layerJsonByPath = new Dictionary<string, List<(string Pak, string Json)>>(StringComparer.OrdinalIgnoreCase);
            PakArchive? mountSource = null;

            foreach (var pakPath in options.PakPathsInOrder)
            {
                var archive = PakArchiveCache.Open(pakPath, openOptions);
                mountSource ??= archive;
                using var stream = File.OpenRead(pakPath);

                foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
                {
                    var relative = PakEntryPaths.ToRelativePath(entry.Path, archive.MountPoint);
                    var bytes = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer, openOptions?.AesKey);
                    var target = Path.Combine(tempDir, relative.Replace('/', Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.GetDirectoryName(target)!);

                    if (relative.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                    {
                        if (!layerJsonByPath.TryGetValue(relative, out var layers))
                        {
                            layers = [];
                            layerJsonByPath[relative] = layers;
                        }

                        layers.Add((pakPath, System.Text.Encoding.UTF8.GetString(bytes)));
                    }

                    if (!files.ContainsKey(relative))
                    {
                        File.WriteAllBytes(target, bytes);
                        files[relative] = target;
                    }
                }
            }

            foreach (var (relative, layers) in layerJsonByPath.Where(kv => kv.Value.Count > 1))
            {
                var mergeOpts = new UeDataTableMergeOptions { AssetLabel = PakEntryPaths.FileNameFromRelative(relative) };
                var chain = layers.Select(l => l.Json).ToList();
                var merged = UeDataTableMerger.MergeChain(chain, mergeOpts);
                jsonReports.Add(merged.Report);
                jsonMergeCount++;

                var target = Path.Combine(tempDir, relative.Replace('/', Path.DirectorySeparatorChar));
                SafeJsonFileWriter.Write(target, merged.Json, keepBackup: false);
                files[relative] = target;

                if (!string.IsNullOrWhiteSpace(options.ConflictReportDirectory))
                {
                    Directory.CreateDirectory(options.ConflictReportDirectory);
                    var reportName = SanitizeFileName(relative) + ".merge-report.json";
                    var reportPath = Path.Combine(options.ConflictReportDirectory, reportName);
                    WriteConflictReport(reportPath, merged.Report, layers.Select(l => l.Pak).ToList());
                }
            }

            mountSource ??= PakArchiveCache.Open(options.PakPathsInOrder[0], openOptions);
            var sources = files
                .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                .Select(kv => new PakSourceFile(kv.Key, kv.Value));

            var buildOpts = options.BuildOptions ?? new PakBuildOptions();
            if (string.IsNullOrWhiteSpace(buildOpts.MountPoint) || buildOpts.MountPoint == "../../../YourGame/")
            {
                buildOpts = new PakBuildOptions
                {
                    MountPoint = mountSource.MountPoint,
                    PakVersion = buildOpts.PakVersion,
                    Compression = buildOpts.Compression,
                };
            }

            var built = PakBuilder.Build(sources, options.OutputPakPath, buildOpts.MountPoint, buildOpts);
            return new PakMergeBuildResult
            {
                Build = built,
                Overlaps = overlap.Conflicts,
                JsonReports = jsonReports,
                JsonMergeCount = jsonMergeCount,
            };
        }
        finally
        {
            try { Directory.Delete(tempDir, recursive: true); } catch { /* best effort */ }
        }
    }

    private static void WriteConflictReport(string path, MergeConflictReport report, IReadOnlyList<string> sources)
    {
        var payload = new
        {
            report.AssetLabel,
            sources,
            propertyConflicts = report.PropertyConflicts,
            rowDeletions = report.RowDeletions,
        };
        var json = System.Text.Json.JsonSerializer.Serialize(payload, new System.Text.Json.JsonSerializerOptions { WriteIndented = true });
        SafeJsonFileWriter.Write(path, json);
    }

    private static string SanitizeFileName(string relative) =>
        relative.Replace('/', '_').Replace('\\', '_');
}
