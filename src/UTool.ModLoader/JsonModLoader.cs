using UTool.Core.Abstractions;
using UTool.Core.Models;
using UTool.ModLoader.Merge;

namespace UTool.ModLoader;

public sealed class JsonModLoader : IModLoader
{
    public async Task<ModLoadResult> LoadAsync(string modsDirectory, CancellationToken cancellationToken = default)
    {
        var issues = new List<ModLoadIssue>();
        var mods = new List<ModPackage>();

        if (!Directory.Exists(modsDirectory))
        {
            issues.Add(new ModLoadIssue
            {
                Severity = ModIssueSeverity.Warning,
                Message = $"Mods directory not found: {modsDirectory}",
            });
            return new ModLoadResult { Mods = mods, Issues = issues };
        }

        foreach (var root in ModDiscovery.FindModRoots(modsDirectory))
        {
            try
            {
                var package = await ModDiscovery.TryLoadPackageAsync(root, cancellationToken);
                if (package is null)
                    continue;

                ValidatePackage(package, issues);
                mods.Add(package);
            }
            catch (Exception ex)
            {
                issues.Add(new ModLoadIssue
                {
                    Severity = ModIssueSeverity.Error,
                    Message = ex.Message,
                    FilePath = Path.Combine(root, ModManifestReader.ManifestFileName),
                });
            }
        }

        ApplyLoadOrder(mods, issues);
        return new ModLoadResult { Mods = mods, Issues = issues };
    }

    private static void ApplyLoadOrder(IList<ModPackage> mods, List<ModLoadIssue> issues)
    {
        var order = ModLoadOrderResolver.Resolve(mods.ToList());
        foreach (var issue in order.Issues)
        {
            issues.Add(new ModLoadIssue
            {
                Severity = issue.Severity,
                ModId = issue.ModId,
                Message = issue.Message,
            });
        }

        mods.Clear();
        foreach (var mod in order.OrderedMods)
            mods.Add(mod);
    }

    private static void ValidatePackage(ModPackage package, List<ModLoadIssue> issues)
    {
        foreach (var contentRoot in package.Manifest.ContentRoots)
        {
            var path = Path.Combine(package.RootPath, contentRoot);
            if (!Directory.Exists(path))
            {
                issues.Add(new ModLoadIssue
                {
                    Severity = ModIssueSeverity.Warning,
                    ModId = package.Manifest.Id,
                    Message = $"Content root missing: {contentRoot}",
                    FilePath = path,
                });
            }
        }

        foreach (var patchFile in package.Manifest.PatchFiles)
        {
            var path = Path.Combine(package.RootPath, patchFile);
            if (!File.Exists(path))
            {
                issues.Add(new ModLoadIssue
                {
                    Severity = ModIssueSeverity.Warning,
                    ModId = package.Manifest.Id,
                    Message = $"Patch file missing: {patchFile}",
                    FilePath = path,
                });
            }
        }
    }

}
