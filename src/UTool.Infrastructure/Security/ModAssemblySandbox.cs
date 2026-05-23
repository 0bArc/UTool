using System.Reflection;
using System.Runtime.Loader;

namespace UTool.Infrastructure.Security;

public sealed class ModAssemblySandbox : IDisposable
{
    private readonly AssemblyLoadContext _context;
    private bool _disposed;

    private readonly IReadOnlyDictionary<string, string> _hostAssemblyPaths;

    public ModAssemblySandbox(
        string modId,
        string? allowedSdkVersion = null,
        IReadOnlyDictionary<string, string>? hostAssemblyPaths = null)
    {
        ModId = modId;
        AllowedSdkVersion = allowedSdkVersion;
        _hostAssemblyPaths = hostAssemblyPaths ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        _context = new AssemblyLoadContext($"utool-sandbox-{modId}", isCollectible: true);
        _context.Resolving += OnResolving;
    }

    public string ModId { get; }
    public string? AllowedSdkVersion { get; }
    public Assembly? LoadedAssembly { get; private set; }

    public Assembly Load(string assemblyPath)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var fullPath = Path.GetFullPath(assemblyPath);
        LoadedAssembly = _context.LoadFromAssemblyPath(fullPath);
        EnforceSdkCompatibility(LoadedAssembly);
        return LoadedAssembly;
    }

    public void Unload()
    {
        if (_disposed)
            return;

        LoadedAssembly = null;
        _disposed = true;
        _context.Unload();
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
    }

    public void Dispose() => Unload();

    private Assembly? OnResolving(AssemblyLoadContext context, AssemblyName name)
    {
        if (IsBlockedAssembly(name.Name))
            throw new InvalidOperationException($"Blocked assembly load from mod '{ModId}': {name.Name}");

        if (name.Name is not null
            && _hostAssemblyPaths.TryGetValue(name.Name, out var hostPath)
            && File.Exists(hostPath))
            return context.LoadFromAssemblyPath(hostPath);

        var loadDir = LoadedAssembly is null
            ? null
            : Path.GetDirectoryName(LoadedAssembly.Location);
        if (loadDir is null)
            return null;

        var candidate = Path.Combine(loadDir, $"{name.Name}.dll");
        return File.Exists(candidate) ? context.LoadFromAssemblyPath(candidate) : null;
    }

    private void EnforceSdkCompatibility(Assembly assembly)
    {
        if (string.IsNullOrWhiteSpace(AllowedSdkVersion))
            return;

        var sdk = assembly.GetReferencedAssemblies()
            .FirstOrDefault(a => string.Equals(a.Name, "UTool.Sdk", StringComparison.OrdinalIgnoreCase));
        if (sdk?.Version?.ToString() is { } ver && ver != AllowedSdkVersion)
        {
            throw new InvalidOperationException(
                $"Mod '{ModId}' targets Sdk {ver}; host requires {AllowedSdkVersion}.");
        }
    }

    private static bool IsBlockedAssembly(string? name) => name switch
    {
        "System.Diagnostics.Process" => false,
        "System.Net.Http" => true,
        "System.Net.Sockets" => true,
        _ when name?.StartsWith("System.Net.", StringComparison.Ordinal) == true => true,
        _ => false,
    };
}
