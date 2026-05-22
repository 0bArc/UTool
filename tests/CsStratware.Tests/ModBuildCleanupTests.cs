using CsStratware.Infrastructure.Caching;
using CsStratware.Pak;
using Xunit;

namespace CsStratware.Tests;

public sealed class ModBuildCleanupTests
{
    [Fact]
    public void FileIdentity_FromPath_missing_directory_does_not_throw()
    {
        var path = Path.Combine(Path.GetTempPath(), "csstratware-missing-" + Guid.NewGuid().ToString("N"));
        var identity = FileIdentity.FromPath(path);
        Assert.Equal(Path.GetFullPath(path), identity.FullPath);
    }

    [Fact]
    public void AssetIndexCache_GetOrBuild_missing_directory_returns_empty_manifest()
    {
        var path = Path.Combine(Path.GetTempPath(), "csstratware-missing-" + Guid.NewGuid().ToString("N"));
        var index = AssetIndexCache.ForDirectory(path);
        var manifest = index.GetOrBuild();
        Assert.Empty(manifest.Entries);
    }

    [Fact]
    public void AfterPack_removes_bin_obj_and_pack_staging()
    {
        var modDir = Path.Combine(Path.GetTempPath(), "csstratware-mod-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(modDir);
        Directory.CreateDirectory(Path.Combine(modDir, "dist"));
        Directory.CreateDirectory(Path.Combine(modDir, "bin"));
        Directory.CreateDirectory(Path.Combine(modDir, "obj"));
        Directory.CreateDirectory(Path.Combine(modDir, ".cache", "compiled"));

        try
        {
            ModBuildCleanup.AfterPack(modDir);
            Assert.False(Directory.Exists(Path.Combine(modDir, "bin")));
            Assert.False(Directory.Exists(Path.Combine(modDir, "obj")));
            Assert.False(Directory.Exists(Path.Combine(modDir, ".cache")));
            Assert.True(Directory.Exists(Path.Combine(modDir, "dist")));
        }
        finally
        {
            try { Directory.Delete(modDir, recursive: true); } catch { /* ignore */ }
        }
    }

    [Fact]
    public void AfterPack_keepCache_retains_cache_but_removes_bin()
    {
        var modDir = Path.Combine(Path.GetTempPath(), "csstratware-mod-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(modDir, "bin"));
        Directory.CreateDirectory(Path.Combine(modDir, ".cache", "compiled"));

        try
        {
            ModBuildCleanup.AfterPack(modDir, keepCache: true);
            Assert.False(Directory.Exists(Path.Combine(modDir, "bin")));
            Assert.True(Directory.Exists(Path.Combine(modDir, ".cache", "compiled")));
        }
        finally
        {
            try { Directory.Delete(modDir, recursive: true); } catch { /* ignore */ }
        }
    }
}
