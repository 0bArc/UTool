namespace CsStratware.Pak;

public sealed class PakOpenOptions
{
    /// <summary>32-byte AES-256 key for encrypted pak index/entries (hex or raw).</summary>
    public byte[]? AesKey { get; init; }

    public static byte[]? ParseAesKey(string? keyMaterial)
    {
        if (string.IsNullOrWhiteSpace(keyMaterial))
            return null;

        keyMaterial = keyMaterial.Trim();
        if (keyMaterial.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            keyMaterial = keyMaterial[2..];

        if (keyMaterial.Length == 64 && keyMaterial.All(Uri.IsHexDigit))
            return Convert.FromHexString(keyMaterial);

        try
        {
            var fromBase64 = Convert.FromBase64String(keyMaterial);
            if (fromBase64.Length == 32)
                return fromBase64;
        }
        catch (FormatException)
        {
            // not base64
        }

        var bytes = System.Text.Encoding.UTF8.GetBytes(keyMaterial);
        return bytes.Length == 32 ? bytes : null;
    }
}
