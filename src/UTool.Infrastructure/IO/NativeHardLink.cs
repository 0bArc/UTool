using System.Runtime.InteropServices;

namespace UTool.Infrastructure.IO;

internal static class NativeHardLink
{
    public static bool TryCreate(string linkPath, string existingPath)
    {
        if (!OperatingSystem.IsWindows())
            return false;

        try
        {
            return CreateHardLink(linkPath, existingPath, IntPtr.Zero);
        }
        catch
        {
            return false;
        }
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool CreateHardLink(string lpFileName, string lpExistingFileName, IntPtr lpSecurityAttributes);
}
