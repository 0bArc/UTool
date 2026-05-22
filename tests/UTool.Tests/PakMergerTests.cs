using System.Text;
using UTool.Pak;
using UTool.Pak.Models;
using Xunit;

namespace UTool.Tests;

public sealed class PakMergerTests
{
    private const string Mount = "../../../TestGame/";

    [Fact]
    public void Merge_unions_unique_paths_from_all_paks()
    {
        var root = NewTempDir();
        var pakA = BuildPak(root, "a.pak", [
            ("Content/OnlyA.txt", "alpha"u8.ToArray()),
            ("Content/Shared.json", """{"Rows":[{"Name":"R1","HP":1}]}"""u8.ToArray()),
        ]);
        var pakB = BuildPak(root, "b.pak", [
            ("Content/OnlyB.txt", "bravo"u8.ToArray()),
            ("Content/Shared.json", """{"Rows":[{"Name":"R1","HP":9},{"Name":"R2","HP":2}]}"""u8.ToArray()),
        ]);
        var output = Path.Combine(root, "merged.pak");

        var result = PakMerger.Merge([pakA, pakB], output, new PakMergeOptions { JsonMerge = true });

        Assert.Equal(3, result.FileCount);
        var merged = PakArchiveCache.Open(output);
        AssertContainsEntry(merged, "Content/OnlyA.txt");
        AssertContainsEntry(merged, "Content/OnlyB.txt");
        AssertContainsEntry(merged, "Content/Shared.json");
    }

    [Fact]
    public void MergeBuild_non_json_overlap_uses_later_pak_bytes()
    {
        var root = NewTempDir();
        var pakA = BuildPak(root, "a.pak", [("Content/Blob.bin", "from-a"u8.ToArray())]);
        var pakB = BuildPak(root, "b.pak", [("Content/Blob.bin", "from-b-longer"u8.ToArray())]);
        var output = Path.Combine(root, "merged.pak");

        PakMergePipeline.MergeBuild(new PakMergeBuildOptions
        {
            PakPathsInOrder = [pakA, pakB],
            OutputPakPath = output,
            JsonMerge = true,
        });

        var bytes = ReadEntryBytes(output, "Content/Blob.bin");
        Assert.Equal("from-b-longer"u8.ToArray(), bytes);
    }

    [Fact]
    public void Merge_json_overlap_unions_rows_and_keeps_extra_properties()
    {
        var root = NewTempDir();
        var baseJson = """{"Rows":[{"Name":"A","HP":10}],"Meta":{"Version":1},"Code":"line1;line2"}""";
        var modJson = """{"Rows":[{"Name":"A","Speed":5},{"Name":"B","HP":2}],"Code":"line1;line2;line3"}""";
        var pakA = BuildPak(root, "a.pak", [("Content/Table.json", Encoding.UTF8.GetBytes(baseJson))]);
        var pakB = BuildPak(root, "b.pak", [("Content/Table.json", Encoding.UTF8.GetBytes(modJson))]);
        var output = Path.Combine(root, "merged.pak");

        PakMerger.Merge([pakA, pakB], output, new PakMergeOptions { JsonMerge = true });

        var mergedJson = Encoding.UTF8.GetString(ReadEntryBytes(output, "Content/Table.json"));
        Assert.Contains("A", mergedJson);
        Assert.Contains("B", mergedJson);
        Assert.Contains("Speed", mergedJson);
        Assert.Contains("Meta", mergedJson);
        Assert.Contains("Version", mergedJson);
        Assert.Contains("Code", mergedJson);
        Assert.Contains("line1;line2", mergedJson);
        Assert.DoesNotContain('\n', mergedJson);
    }

    [Fact]
    public void Merge_total_bytes_at_least_sum_of_unique_assets()
    {
        var root = NewTempDir();
        var big = new string('x', 64 * 1024);
        var pakA = BuildPak(root, "a.pak", [
            ("Content/BigA.dat", Encoding.UTF8.GetBytes(big)),
            ("Content/Shared.json", """{"Rows":[{"Name":"X"}]}"""u8.ToArray()),
        ]);
        var pakB = BuildPak(root, "b.pak", [
            ("Content/BigB.dat", Encoding.UTF8.GetBytes(big)),
            ("Content/Shared.json", """{"Rows":[{"Name":"X"},{"Name":"Y"}]}"""u8.ToArray()),
        ]);
        var output = Path.Combine(root, "merged.pak");

        var result = PakMerger.Merge([pakA, pakB], output, new PakMergeOptions { JsonMerge = true });

        Assert.Equal(3, result.FileCount);
        Assert.True(result.TotalBytes > 120 * 1024, $"merged too small: {result.TotalBytes} bytes");
    }

    [Fact]
    public void Merge_small_pak_before_large_pak_unions_all_rows()
    {
        var root = NewTempDir();
        var smallRows = string.Join(",", Enumerable.Range(0, 5).Select(i => $$"""{"Name":"R{{i}}","HP":1}"""));
        var largeRows = string.Join(",", Enumerable.Range(0, 40).Select(i => $$"""{"Name":"R{{i}}","HP":1}"""));
        var smallJson = $$"""{"Rows":[{{smallRows}}]}""";
        var largeJson = $$"""{"Rows":[{{largeRows}}]}""";
        var pakSmall = BuildPak(root, "small.pak", [("Content/Table.json", Encoding.UTF8.GetBytes(smallJson))]);
        var pakLarge = BuildPak(root, "large.pak", [("Content/Table.json", Encoding.UTF8.GetBytes(largeJson))]);
        var output = Path.Combine(root, "merged.pak");

        PakMerger.Merge([pakSmall, pakLarge], output, new PakMergeOptions { JsonMerge = true });

        var mergedJson = Encoding.UTF8.GetString(ReadEntryBytes(output, "Content/Table.json"));
        Assert.Contains("R39", mergedJson);
        Assert.DoesNotContain('\n', mergedJson);
    }

    [Fact]
    public void FilterMergeInputs_excludes_output_pak()
    {
        var root = NewTempDir();
        var output = Path.Combine(root, "merged.pak");
        var a = BuildPak(root, "a.pak", [("Content/A.txt", "a"u8.ToArray())]);
        File.WriteAllBytes(output, []);

        var filtered = PakPathResolver.FilterMergeInputs([a, output], output);

        Assert.Single(filtered);
        Assert.Equal(a, filtered[0]);
    }

    [Fact]
    public void FilterMergeInputs_excludes_merged_pak_artifact()
    {
        var root = NewTempDir();
        var a = BuildPak(root, "a.pak", [("Content/A.txt", "a"u8.ToArray())]);
        var merged = BuildPak(root, "merged.pak", [("Content/Half.json", """{"Rows":[{"Name":"X"}]}"""u8.ToArray())]);

        var filtered = PakPathResolver.FilterMergeInputs([a, merged]);

        Assert.Single(filtered);
        Assert.Equal(a, filtered[0]);
    }

    [Fact]
    public void ResolveForMerge_finds_nested_mod_dist_paks()
    {
        var root = NewTempDir();
        WriteModManifest(Path.Combine(root, "mod-a"), "mod-a");
        WriteModManifest(Path.Combine(root, "mod-b"), "mod-b");

        var pakA = BuildPak(Path.Combine(root, "mod-a", "dist"), "mod-a_P.pak", [
            ("Content/OnlyA.txt", "alpha"u8.ToArray()),
        ]);
        var pakB = BuildPak(Path.Combine(root, "mod-b", "dist"), "mod-b_P.pak", [
            ("Content/OnlyB.txt", "bravo"u8.ToArray()),
        ]);

        var resolved = PakPathResolver.ResolveForMerge(root);

        Assert.Equal(2, resolved.Count);
        Assert.Contains(pakA, resolved, StringComparer.OrdinalIgnoreCase);
        Assert.Contains(pakB, resolved, StringComparer.OrdinalIgnoreCase);
    }

    [Fact]
    public void Merge_writes_to_extracted_mod_files_without_csmerge_sidecars()
    {
        var root = NewTempDir();
        var pakA = BuildPak(root, "a.pak", [
            ("Content/Shared.json", """{"Rows":[{"Name":"R1","HP":1}]}"""u8.ToArray()),
        ]);
        var pakB = BuildPak(root, "b.pak", [
            ("Content/Shared.json", """{"Rows":[{"Name":"R1","HP":9},{"Name":"R2","HP":2}]}"""u8.ToArray()),
        ]);
        var output = Path.Combine(root, "merged.pak");
        var filesDir = PakMergePaths.ResolveFilesDirectory(output, [pakA, pakB]);

        PakMerger.Merge([pakA, pakB], output, new PakMergeOptions { JsonMerge = true });

        Assert.True(Directory.Exists(filesDir));
        Assert.False(Directory.EnumerateFiles(filesDir, "*", SearchOption.AllDirectories)
            .Any(p => Path.GetFileName(p).Contains(".csmerge", StringComparison.OrdinalIgnoreCase)));
        Assert.Single(Directory.EnumerateFiles(filesDir, "Shared.json", SearchOption.AllDirectories));

        var mergedJson = File.ReadAllText(Directory.EnumerateFiles(filesDir, "Shared.json", SearchOption.AllDirectories).Single());
        Assert.Contains("R2", mergedJson);
    }

    [Fact]
    public void Merge_mods_directory_unions_all_dist_paks()
    {
        var root = NewTempDir();
        WriteModManifest(Path.Combine(root, "mod-a"), "mod-a");
        WriteModManifest(Path.Combine(root, "mod-b"), "mod-b");

        var big = new string('x', 32 * 1024);
        BuildPak(Path.Combine(root, "mod-a", "dist"), "mod-a_P.pak", [
            ("Content/BigA.dat", Encoding.UTF8.GetBytes(big)),
        ]);
        BuildPak(Path.Combine(root, "mod-b", "dist"), "mod-b_P.pak", [
            ("Content/BigB.dat", Encoding.UTF8.GetBytes(big)),
        ]);

        var paks = PakPathResolver.ResolveForMerge(root);
        var output = Path.Combine(root, "merged.pak");
        var result = PakMerger.Merge(paks, output, new PakMergeOptions { JsonMerge = true });

        Assert.Equal(2, paks.Count);
        Assert.Equal(2, result.FileCount);
        Assert.True(result.TotalBytes > 60 * 1024, $"merged too small: {result.TotalBytes} bytes");
    }

    [Fact]
    public void Merge_preserves_subdirectories_when_source_mounts_differ()
    {
        var root = NewTempDir();
        var craftingPak = BuildPak(
            root,
            "crafting.pak",
            "../../../Icarus/Content/data/Crafting/",
            [("D_ProcessorRecipes.json", """{"Rows":[{"Name":"RecipeA"}]}"""u8.ToArray())]);
        var aiPak = BuildPak(
            root,
            "ai.pak",
            "../../../Icarus/Content/data/AI/",
            [("D_AICreatureType.json", """{"Rows":[{"Name":"CreatureA"}]}"""u8.ToArray())]);
        var output = Path.Combine(root, "merged.pak");

        PakMerger.Merge([craftingPak, aiPak], output, new PakMergeOptions { JsonMerge = true });

        var merged = PakArchiveCache.Open(output);
        Assert.Equal("../../../Icarus/Content/data/", merged.MountPoint);
        AssertContainsEntry(merged, "Crafting/D_ProcessorRecipes.json");
        AssertContainsEntry(merged, "AI/D_AICreatureType.json");
    }

    private static void WriteModManifest(string modRoot, string id)
    {
        Directory.CreateDirectory(modRoot);
        File.WriteAllText(
            Path.Combine(modRoot, "mod.json"),
            $$"""{"id":"{{id}}","name":"{{id}}","version":"1.0.0"}""");
    }

    private static string BuildPak(string root, string name, IReadOnlyList<(string Relative, byte[] Bytes)> files)
    {
        return BuildPak(root, name, Mount, files);
    }

    private static string BuildPak(string root, string name, string mount, IReadOnlyList<(string Relative, byte[] Bytes)> files)
    {
        var contentDir = Path.Combine(root, Path.GetFileNameWithoutExtension(name) + "-content");
        Directory.CreateDirectory(contentDir);
        foreach (var (relative, bytes) in files)
        {
            var path = Path.Combine(contentDir, relative.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllBytes(path, bytes);
        }

        var pakPath = Path.Combine(root, name);
        PakBuilder.BuildFromDirectory(contentDir, pakPath, new PakBuildOptions { MountPoint = mount });
        return pakPath;
    }

    private static byte[] ReadEntryBytes(string pakPath, string relativePath)
    {
        var archive = PakArchiveCache.Open(pakPath);
        var entry = FindEntry(archive, relativePath);
        using var stream = File.OpenRead(pakPath);
        return PakEntryExtractor.ReadEntry(stream, entry, archive.Footer);
    }

    private static void AssertContainsEntry(PakArchive archive, string relativePath)
    {
        Assert.NotNull(FindEntry(archive, relativePath));
    }

    private static PakEntryRecord FindEntry(PakArchive archive, string relativePath)
    {
        foreach (var entry in archive.Entries.Values)
        {
            var rel = PakEntryPaths.ToRelativePath(entry.Path, archive.MountPoint);
            if (string.Equals(rel, relativePath, StringComparison.OrdinalIgnoreCase))
                return entry;
        }

        throw new Xunit.Sdk.XunitException($"Entry not found: {relativePath}");
    }

    private static string NewTempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), "utool-pak-test-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return dir;
    }
}
