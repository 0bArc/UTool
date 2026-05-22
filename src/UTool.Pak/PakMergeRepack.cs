using UTool.Pak.Models;

namespace UTool.Pak;

/// <summary>Repack staged merge output (UnrealPak for UE5/Icarus-compatible paks).</summary>
internal static class PakMergeRepack
{
    public static PakBuildResult Build(
        PakMergeStaging.Workset workset,
        string stagedRoot,
        string outputPakPath,
        IReadOnlyList<string> sourcePakPaths,
        PakOpenOptions? openOptions,
        PakMergeOptions options)
    {
        var mount = ResolveMountPoint(workset, sourcePakPaths, openOptions, options);
        var sources = workset.Files
            .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
            .Select(kv => new PakSourceFile(kv.Key, kv.Value))
            .ToList();

        var maxVersion = sourcePakPaths
            .Select(p => PakArchiveCache.Open(p, openOptions).Footer.Version)
            .DefaultIfEmpty(PakFormat.DefaultPakVersion)
            .Max();

        PakMergeStaging.RemoveMergeArtifacts(stagedRoot);

        var needsUnrealPak = maxVersion >= 10;
        if (options.PreferUnrealPak && needsUnrealPak)
        {
            if (TryPackWithUnrealPak(stagedRoot, outputPakPath, mount, options.UnrealPakOptions))
                return ToBuildResult(workset, outputPakPath);

            throw new InvalidOperationException(
                $"Source pak(s) use UE pak version {maxVersion}. Merged output must be built with UnrealPak " +
                "(run 'utool setup unrealpak' or set UTOOL_UNREALPAK).");
        }

        var build = options.BuildOptions ?? new PakBuildOptions();
        if (string.IsNullOrWhiteSpace(build.MountPoint) || build.MountPoint == "../../../YourGame/")
        {
            build = new PakBuildOptions
            {
                MountPoint = mount,
                PakVersion = maxVersion,
                Compression = build.Compression,
            };
        }

        return PakBuilder.Build(sources, outputPakPath, build.MountPoint, build);
    }

    private static string ResolveMountPoint(
        PakMergeStaging.Workset workset,
        IReadOnlyList<string> sourcePakPaths,
        PakOpenOptions? openOptions,
        PakMergeOptions options)
    {
        var build = options.BuildOptions;
        if (build is not null
            && !string.IsNullOrWhiteSpace(build.MountPoint)
            && build.MountPoint != "../../../YourGame/")
        {
            return PakEntryPaths.NormalizeMountPoint(build.MountPoint);
        }

        var mergeMount = PakMergeStaging.ResolveCommonMountPoint(sourcePakPaths, openOptions);
        if (!string.IsNullOrWhiteSpace(mergeMount))
            return PakEntryPaths.NormalizeMountPoint(mergeMount);

        var mountSource = workset.MountSource ?? PakArchiveCache.Open(sourcePakPaths[0], openOptions);
        return PakEntryPaths.NormalizeMountPoint(mountSource.MountPoint);
    }

    private static bool TryPackWithUnrealPak(
        string stagedRoot,
        string outputPakPath,
        string mountPoint,
        UnrealPakOptions? ueOptions)
    {
        try
        {
            UnrealPakRunner.PackDirectory(stagedRoot, outputPakPath, mountPoint, compress: false, ueOptions);
            return File.Exists(outputPakPath) && new FileInfo(outputPakPath).Length > 0;
        }
        catch (FileNotFoundException)
        {
            return false;
        }
    }

    private static PakBuildResult ToBuildResult(PakMergeStaging.Workset workset, string outputPakPath)
    {
        long total = 0;
        foreach (var path in workset.Files.Values)
            total += new FileInfo(path).Length;

        return new PakBuildResult
        {
            OutputPath = Path.GetFullPath(outputPakPath),
            FileCount = workset.Files.Count,
            TotalBytes = total,
        };
    }
}
