using CsStratware.Core.Abstractions;
using CsStratware.Core.Models;

namespace CsStratware.ModLoader;

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

        ResolveLoadOrder(mods, issues);
        return new ModLoadResult { Mods = mods, Issues = issues };
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

    private static void ResolveLoadOrder(IList<ModPackage> mods, List<ModLoadIssue> issues)
    {
        var byId = mods.ToDictionary(m => m.Manifest.Id, StringComparer.OrdinalIgnoreCase);
        var visiting = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var ordered = new List<ModPackage>();

        void Visit(ModPackage mod)
        {
            var id = mod.Manifest.Id;
            if (visited.Contains(id))
                return;

            if (!visiting.Add(id))
            {
                issues.Add(new ModLoadIssue
                {
                    Severity = ModIssueSeverity.Error,
                    ModId = id,
                    Message = "Circular mod dependency detected.",
                });
                return;
            }

            foreach (var dep in mod.Manifest.Dependencies.Where(d => !d.Optional))
            {
                if (!byId.TryGetValue(dep.Id, out var depMod))
                {
                    issues.Add(new ModLoadIssue
                    {
                        Severity = ModIssueSeverity.Error,
                        ModId = id,
                        Message = $"Missing required dependency: {dep.Id}",
                    });
                    continue;
                }

                Visit(depMod);
            }

            visiting.Remove(id);
            visited.Add(id);
            ordered.Add(mod);
        }

        foreach (var mod in mods.OrderBy(m => m.Manifest.Id, StringComparer.OrdinalIgnoreCase))
            Visit(mod);

        mods.Clear();
        foreach (var mod in ordered)
            mods.Add(mod);
    }
}
