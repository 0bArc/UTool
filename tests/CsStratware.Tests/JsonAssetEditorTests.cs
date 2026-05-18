using CsStratware.Sdk;
using Xunit;

namespace CsStratware.Tests;

public sealed class JsonAssetEditorTests
{
    [Fact]
    public void ReplaceAll_scoped_does_not_touch_outside_subtree()
    {
        const string json = """
            [{"Type":"A","Properties":{"X":1}},{"Type":"B","Properties":{"X":2}}]
            """;
        var editor = new JsonAssetEditor(json);
        editor.ReplaceAll("X", 99, "/1/Properties");
        var outJson = editor.ToJson();
        Assert.Contains("\"X\": 1", outJson);
        Assert.Contains("\"X\": 99", outJson);
    }
}
