using CsStratware.Core.Models;
using CsStratware.ModLoader.Merge;
using Xunit;

namespace CsStratware.Tests;

public sealed class UeDataTableMergerTests
{
    [Fact]
    public void MergeChain_numeric_min_wins_on_same_property()
    {
        const string baseJson = """{"Rows":[{"Name":"Wolf","Health":500}]}""";
        const string modA = """{"Rows":[{"Name":"Wolf","Health":100}]}""";
        const string modB = """{"Rows":[{"Name":"Wolf","Health":200}]}""";

        var result = UeDataTableMerger.MergeChain([baseJson, modA, modB]);
        Assert.Contains("\"Health\":100", result.Json.Replace(" ", ""));
        Assert.Contains("numeric-min", result.Report.PropertyConflicts[0].Resolution);
    }

    [Fact]
    public void MergeChain_later_mod_wins_non_numeric()
    {
        const string baseJson = """{"Rows":[{"Name":"Wolf","Tag":"base"}]}""";
        const string modA = """{"Rows":[{"Name":"Wolf","Tag":"modA"}]}""";
        const string modB = """{"Rows":[{"Name":"Wolf","Tag":"modB"}]}""";

        var result = UeDataTableMerger.MergeChain([baseJson, modA, modB]);
        Assert.Contains("modB", result.Json);
        Assert.DoesNotContain("modA", result.Json);
    }

    [Fact]
    public void MergeChain_preserves_base_row_order_and_appends_new_rows()
    {
        const string baseJson = """
            {"Rows":[{"Name":"Alpha"},{"Name":"Beta"}]}
            """;
        const string mod = """{"Rows":[{"Name":"Zeta"}]}""";

        var result = UeDataTableMerger.MergeChain([baseJson, mod]);
        var alpha = result.Json.IndexOf("Alpha", StringComparison.Ordinal);
        var beta = result.Json.IndexOf("Beta", StringComparison.Ordinal);
        var zeta = result.Json.IndexOf("Zeta", StringComparison.Ordinal);
        Assert.True(alpha < beta && beta < zeta);
    }

    [Fact]
    public void MergeChain_deletion_marker_removes_row()
    {
        const string baseJson = """{"Rows":[{"Name":"Wolf"},{"Name":"Bear"}]}""";
        const string mod = """{"Rows":[{"Name":"Wolf","__csDeleted":true}]}""";

        var result = UeDataTableMerger.MergeChain([baseJson, mod]);
        Assert.DoesNotContain("Wolf", result.Json);
        Assert.Contains("Bear", result.Json);
        Assert.Single(result.Report.RowDeletions);
    }

    [Fact]
    public void ModLoadOrderResolver_loadAfter_orders_mods()
    {
        var mods = new[]
        {
            MakeMod("core", loadAfter: []),
            MakeMod("feature", loadAfter: ["core"]),
        };

        var order = ModLoadOrderResolver.Resolve(mods);
        Assert.Equal(["core", "feature"], order.OrderedMods.Select(m => m.Manifest.Id));
    }

    private static ModPackage MakeMod(string id, IReadOnlyList<string>? loadAfter = null) => new()
    {
        RootPath = Path.GetTempPath(),
        ManifestPath = Path.Combine(Path.GetTempPath(), "mod.json"),
        Manifest = new ModManifest
        {
            Id = id,
            Name = id,
            Version = "1.0.0",
            LoadAfter = loadAfter ?? [],
        },
    };
}
