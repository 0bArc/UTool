using CsStratware.Pak;
using Xunit;

namespace CsStratware.Tests;

public sealed class PakCryptoTests
{
    [Fact]
    public void ParseAesKey_accepts_64_char_hex()
    {
        var hex = new string('A', 64);
        var key = PakOpenOptions.ParseAesKey(hex);
        Assert.NotNull(key);
        Assert.Equal(32, key!.Length);
    }

    [Fact]
    public void ParseAesKey_accepts_base64_32_bytes()
    {
        var raw = Enumerable.Range(0, 32).Select(i => (byte)i).ToArray();
        var b64 = Convert.ToBase64String(raw);
        var key = PakOpenOptions.ParseAesKey(b64);
        Assert.Equal(raw, key);
    }

    [Fact]
    public void TryZlibDecompress_finds_payload_after_header()
    {
        var json = "{ \"ok\": true }"u8.ToArray();
        using var output = new MemoryStream();
        using (var zlib = new System.IO.Compression.ZLibStream(
                   output,
                   System.IO.Compression.CompressionMode.Compress,
                   leaveOpen: true))
            zlib.Write(json);

        var prefix = new byte[] { 0, 0, 0, 0, 0, 0, 0, 0x8A, 0x03 };
        var wrapped = prefix.Concat(output.ToArray()).ToArray();
        Assert.True(PakPayloadDecoder.TryZlibDecompress(wrapped, out var decoded));
        Assert.Equal(json, decoded);
    }
}
