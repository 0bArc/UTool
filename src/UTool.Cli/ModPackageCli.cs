using UTool.Core.Models;
using UTool.ModLoader;

namespace UTool.Cli;

internal static class ModPackageCli
{
    public static ModPackage? TryLoad(string modDir) =>
        ModDiscovery.TryLoadPackageAsync(modDir).GetAwaiter().GetResult();
}
