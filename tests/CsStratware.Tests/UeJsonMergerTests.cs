using CsStratware.ModLoader;
using Xunit;

namespace CsStratware.Tests;

public sealed class UeJsonMergerTests
{
    [Fact]
    public void Merge_rows_unions_by_name_and_keeps_both_mod_changes()
    {
        const string baseJson = """
            {
              "Rows": [
                { "Name": "Wolf", "HP": 10 },
                { "Name": "Bear", "HP": 50 }
              ]
            }
            """;

        const string overlayJson = """
            {
              "Rows": [
                { "Name": "Wolf", "Speed": 5 },
                { "Name": "Dragon", "HP": 999 }
              ]
            }
            """;

        var merged = UeJsonMerger.Merge(baseJson, overlayJson);
        Assert.Contains("\"Name\":\"Wolf\"", merged.Replace(" ", ""));
        Assert.Contains("\"HP\":10", merged.Replace(" ", ""));
        Assert.Contains("\"Speed\":5", merged.Replace(" ", ""));
        Assert.Contains("Bear", merged);
        Assert.Contains("Dragon", merged);
    }
}
