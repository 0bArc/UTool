namespace UTool.Pak.IoStore;

/// <summary>Placeholder for UE5 IoStore (.utoc/.ucas) — not yet implemented.</summary>
public static class IoStoreSupport
{
    public static bool IsIoStoreContainer(string path) =>
        path.EndsWith(".utoc", StringComparison.OrdinalIgnoreCase)
        || path.EndsWith(".ucas", StringComparison.OrdinalIgnoreCase);

    public static void ThrowNotSupported(string path) =>
        throw new NotSupportedException(
            $"IoStore path '{path}' is not supported yet. Use UnrealPak or cooked .pak workflows.");
}
