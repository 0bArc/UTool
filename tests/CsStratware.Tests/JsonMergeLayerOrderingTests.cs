using CsStratware.ModLoader.Merge;
using Xunit;

namespace CsStratware.Tests;

public sealed class JsonMergeLayerOrderingTests
{
    [Fact]
    public void OrderForMerge_puts_fullest_table_first_even_when_small_layer_was_staged_first()
    {
        var small = """{"Rows":[{"Name":"A"},{"Name":"B"}]}""";
        var large = string.Join(",", Enumerable.Range(0, 80).Select(i => $$"""{"Name":"R{{i}}"}"""));
        large = $$"""{"Rows":[{{large}}]}""";

        var layers = new[] { ("small.pak", small), ("large.pak", large) };
        var ordered = JsonMergeLayerOrdering.OrderForMerge(layers, l => l.Item2);

        Assert.Equal("large.pak", ordered[0].Item1);
        Assert.Equal(80, UeDataTableMerger.CountDataTableRows(ordered[0].Item2));
    }

    [Fact]
    public void MergeChain_with_reordered_layers_keeps_all_rows_from_largest_table()
    {
        var smallRows = string.Join(",", Enumerable.Range(0, 10).Select(i => $$"""{"Name":"S{{i}}"}"""));
        var largeRows = string.Join(",", Enumerable.Range(0, 60).Select(i => $$"""{"Name":"L{{i}}"}"""));
        var small = $$"""{"Rows":[{{smallRows}}]}""";
        var large = $$"""{"Rows":[{{largeRows}}]}""";

        var ordered = JsonMergeLayerOrdering.OrderForMerge(
            new[] { small, large },
            j => j);
        var merged = UeDataTableMerger.MergeChain(ordered);

        Assert.Equal(70, UeDataTableMerger.CountDataTableRows(merged.Json));
        Assert.Contains("L59", merged.Json);
        Assert.Contains("S9", merged.Json);
    }
}
