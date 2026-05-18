using System.Security.Cryptography;

namespace CsStratware.Infrastructure.Caching;

public static class ContentHasher
{
    public static string HashFile(string path)
    {
        using var stream = File.OpenRead(path);
        return HashStream(stream);
    }

    public static string HashStream(Stream stream)
    {
        var hash = SHA256.HashData(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    public static string HashText(string text) =>
        Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(text))).ToLowerInvariant();

    public static string HashBytes(ReadOnlySpan<byte> data) =>
        Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
}
