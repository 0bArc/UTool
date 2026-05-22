using System.Security.Cryptography;

namespace UTool.Pak;

internal static class PakAesHelper
{
    public static byte[] DecryptIndex(ReadOnlySpan<byte> cipher, byte[] aesKey) =>
        DecryptData(cipher, aesKey);

    public static byte[] DecryptData(ReadOnlySpan<byte> cipher, byte[] aesKey)
    {
        if (aesKey.Length != 16 && aesKey.Length != 32)
            throw new ArgumentException("AES key must be 16 or 32 bytes.", nameof(aesKey));

        if (cipher.Length == 0 || cipher.Length % 16 != 0)
            throw new ArgumentException("Encrypted pak data size must be a multiple of 16.", nameof(cipher));

        using var aes = Aes.Create();
        aes.Key = aesKey.Length == 32 ? aesKey : Derive256(aesKey);
        aes.Mode = CipherMode.ECB;
        aes.Padding = PaddingMode.None;

        using var decryptor = aes.CreateDecryptor();
        var input = cipher.ToArray();
        var plain = new byte[input.Length];
        var written = decryptor.TransformBlock(input, 0, input.Length, plain, 0);
        if (written != plain.Length)
            Array.Resize(ref plain, written);

        return plain;
    }

    public static long Align16(long value) => (value + 15) & ~15L;

    private static byte[] Derive256(byte[] key16)
    {
        var k = new byte[32];
        Array.Copy(key16, k, Math.Min(16, key16.Length));
        return k;
    }
}
