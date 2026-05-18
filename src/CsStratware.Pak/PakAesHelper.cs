using System.Security.Cryptography;

namespace CsStratware.Pak;

internal static class PakAesHelper
{
    public static byte[] DecryptIndex(ReadOnlySpan<byte> cipher, byte[] aesKey)
    {
        if (aesKey.Length != 16 && aesKey.Length != 32)
            throw new ArgumentException("AES key must be 16 or 32 bytes.", nameof(aesKey));

        using var aes = Aes.Create();
        aes.Key = aesKey.Length == 32 ? aesKey : Derive256(aesKey);
        aes.Mode = CipherMode.ECB;
        aes.Padding = PaddingMode.None;

        using var decryptor = aes.CreateDecryptor();
        var plain = new byte[cipher.Length];
        var written = decryptor.TransformBlock(cipher.ToArray(), 0, cipher.Length, plain, 0);
        if (written != plain.Length)
            Array.Resize(ref plain, written);

        return plain;
    }

    private static byte[] Derive256(byte[] key16)
    {
        var k = new byte[32];
        Array.Copy(key16, k, Math.Min(16, key16.Length));
        return k;
    }
}
