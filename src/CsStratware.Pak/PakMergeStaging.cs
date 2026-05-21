using System.Text;

using CsStratware.ModLoader.Merge;

using CsStratware.Pak.Models;



namespace CsStratware.Pak;



/// <summary>Stage pak layers under EXTRACTED-MOD/FILES: extract, merge JSON overlaps in place.</summary>

internal static class PakMergeStaging

{

    internal sealed class Workset

    {

        public required Dictionary<string, string> Files { get; set; }

        public required Dictionary<string, List<(string SourcePak, string Json)>> JsonLayersByPath { get; init; }

        public PakArchive? MountSource { get; init; }

    }



    internal static Workset Stage(

        IReadOnlyList<string> pakPathsInOrder,

        string filesDirectory,

        PakOpenOptions? openOptions,

        string relativeRootMount,

        Action<string>? log = null)

    {

        var files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        var jsonLayers = new Dictionary<string, List<(string SourcePak, string Json)>>(StringComparer.OrdinalIgnoreCase);

        PakArchive? mountSource = null;

        relativeRootMount = PakEntryPaths.NormalizeMountPoint(relativeRootMount);



        foreach (var pakPath in pakPathsInOrder)

        {

            var archive = PakArchiveCache.Open(pakPath, openOptions);

            mountSource ??= archive;

            using var stream = File.OpenRead(pakPath);



            foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))

            {

                var relative = PakEntryPaths.ToRelativePath(entry.Path, relativeRootMount);

                var bytes = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer, openOptions?.AesKey);

                var target = Path.Combine(filesDirectory, relative.Replace('/', Path.DirectorySeparatorChar));

                Directory.CreateDirectory(Path.GetDirectoryName(target)!);



                if (relative.EndsWith(".json", StringComparison.OrdinalIgnoreCase))

                {

                    if (!jsonLayers.TryGetValue(relative, out var layers))

                    {

                        layers = [];

                        jsonLayers[relative] = layers;

                    }



                    layers.Add((pakPath, Encoding.UTF8.GetString(bytes)));

                }



                if (File.Exists(target))

                {

                    var jsonNote = relative.EndsWith(".json", StringComparison.OrdinalIgnoreCase)

                        && jsonLayers.TryGetValue(relative, out var jl)

                        && jl.Count > 1

                        ? " (json layers merged later)"

                        : string.Empty;

                    log?.Invoke($"OVERWRITE: {relative} from {pakPath}{jsonNote}");

                }



                File.WriteAllBytes(target, bytes);

                files[relative] = target;

            }

        }



        return new Workset

        {

            Files = files,

            JsonLayersByPath = jsonLayers,

            MountSource = mountSource,

        };

    }



    internal static string ResolveCommonMountPoint(IReadOnlyList<string> pakPathsInOrder, PakOpenOptions? openOptions)

    {

        var mounts = pakPathsInOrder

            .Select(path => PakArchiveCache.Open(path, openOptions).MountPoint)

            .ToList();



        return PakEntryPaths.CommonMountPoint(mounts);

    }



    internal static int ApplyJsonOverlaps(

        Workset workset,

        string filesDirectory,

        bool jsonMerge,

        List<MergeConflictReport>? reports = null,

        string? conflictReportDirectory = null,

        Action<string>? log = null)

    {

        if (!jsonMerge)

            return 0;



        var jsonMergeCount = 0;

        foreach (var (relative, layers) in workset.JsonLayersByPath.Where(kv => kv.Value.Count > 1))

        {

            log?.Invoke($"JSON_MERGE: {relative} ({layers.Count} layers)");

            var mergeOpts = new UeDataTableMergeOptions

            {

                AssetLabel = PakEntryPaths.FileNameFromRelative(relative),

            };

            var orderedLayers = JsonMergeLayerOrdering.OrderForMerge(layers, l => l.Json);

            var chain = orderedLayers.Select(l => l.Json).ToList();

            var merged = UeDataTableMerger.MergeChain(chain, mergeOpts);

            reports?.Add(merged.Report);

            jsonMergeCount++;



            var target = Path.Combine(filesDirectory, relative.Replace('/', Path.DirectorySeparatorChar));

            Directory.CreateDirectory(Path.GetDirectoryName(target)!);

            File.WriteAllText(target, merged.Json, Encoding.UTF8);

            workset.Files[relative] = target;



            if (!string.IsNullOrWhiteSpace(conflictReportDirectory))

            {

                Directory.CreateDirectory(conflictReportDirectory);

                var reportName = SanitizeFileName(relative) + ".merge-report.json";

                var reportPath = Path.Combine(conflictReportDirectory, reportName);

                WriteConflictReport(reportPath, merged.Report, layers.Select(l => l.SourcePak).ToList());

            }

        }



        return jsonMergeCount;

    }



    /// <summary>Remove merge sidecars so repack only includes final assets.</summary>

    internal static void RemoveMergeArtifacts(string filesDirectory)

    {

        if (!Directory.Exists(filesDirectory))

            return;



        foreach (var path in Directory.EnumerateFiles(filesDirectory, "*", SearchOption.AllDirectories))

        {

            var name = Path.GetFileName(path);

            if (name.EndsWith(SafeJsonFileWriter.BackupExtension, StringComparison.OrdinalIgnoreCase)

                || name.EndsWith(".csmerge.tmp", StringComparison.OrdinalIgnoreCase)

                || name.Contains(".csmerge", StringComparison.OrdinalIgnoreCase))

            {

                try { File.Delete(path); } catch { /* ignore */ }

            }

        }

    }



    internal static void RefreshWorksetFiles(Workset workset, string filesDirectory)

    {

        var files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        foreach (var path in Directory.EnumerateFiles(filesDirectory, "*", SearchOption.AllDirectories))

        {

            var name = Path.GetFileName(path);

            if (name.Contains(".csmerge", StringComparison.OrdinalIgnoreCase)

                || name.EndsWith(".merge-report.json", StringComparison.OrdinalIgnoreCase))

                continue;



            var relative = Path.GetRelativePath(filesDirectory, path).Replace('\\', '/');

            files[relative] = path;

        }



        workset.Files = files;

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

        File.WriteAllText(path, json, Encoding.UTF8);

    }



    private static string SanitizeFileName(string relative) =>

        relative.Replace('/', '_').Replace('\\', '_');

}


