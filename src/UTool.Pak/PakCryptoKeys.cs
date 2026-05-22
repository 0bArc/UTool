using System.Text;
using System.Text.Json;

namespace UTool.Pak;

public static class PakCryptoKeys
{
    public static string WriteCryptoJson(byte[] aesKey, string? directory = null)
    {
        if (aesKey.Length != 32)
            throw new ArgumentException("UnrealPak crypto key must be 32 bytes (AES-256).", nameof(aesKey));

        directory = directory ?? Path.Combine(Path.GetTempPath(), "utool-crypto");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, "Crypto.json");
        var keyBase64 = Convert.ToBase64String(aesKey);

        var json = $$"""
            {
              "$types": {
                "UnrealBuildTool.EncryptionAndSigning+CryptoSettings, UnrealBuildTool, Version=4.0.0.0, Culture=neutral, PublicKeyToken=null": "1",
                "UnrealBuildTool.EncryptionAndSigning+EncryptionKey, UnrealBuildTool, Version=4.0.0.0, Culture=neutral, PublicKeyToken=null": "2"
              },
              "$type": "1",
              "EncryptionKey": {
                "$type": "2",
                "Name": "utool",
                "Guid": "00000000-0000-0000-0000-000000000000",
                "Key": "{{keyBase64}}"
              },
              "SigningKey": null,
              "bEnablePakSigning": false,
              "bEnablePakIndexEncryption": true,
              "bEnablePakIniEncryption": true,
              "bEnablePakUAssetEncryption": true,
              "bEnablePakFullAssetEncryption": false,
              "bDataCryptoRequired": true,
              "PakEncryptionRequired": true,
              "PakSigningRequired": false,
              "SecondaryEncryptionKeys": []
            }
            """;

        File.WriteAllText(path, json, new UTF8Encoding(false));
        return path;
    }
}
