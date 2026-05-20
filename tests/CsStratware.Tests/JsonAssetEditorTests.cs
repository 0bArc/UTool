using CsStratware.ModLoader;
using CsStratware.Sdk;
using Xunit;

namespace CsStratware.Tests;

public sealed class JsonAssetEditorTests
{
    private const string CreatureTableJson = """
        {
          "Rows": [
            { "Name": "Wolf", "Metadata": { "Tier": 1 }, "SkinningXPEvent": { "RowName": "Skin_Wolf" } },
            { "Name": "Juvenile_RockGolem", "Metadata": { "Tier": 9 }, "SkinningXPEvent": { "RowName": "Skin_RockGolem_Juvenile" } },
            { "Name": "RockGolem", "Metadata": { "Tier": 9 }, "AdditionalDamageStat": { "Value": "RockGolemExtraDamage_+%" } }
          ]
        }
        """;

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

    [Fact]
    public void RemoveArrayElementsWhere_drops_matching_row()
    {
        var editor = new JsonAssetEditor(CreatureTableJson);
        var removed = editor.RemoveArrayElementsWhere("/Rows", "Name", "Juvenile_RockGolem");
        var outJson = editor.ToJson();
        Assert.Equal(1, removed);
        Assert.DoesNotContain("Juvenile_RockGolem", outJson);
        Assert.Contains("RockGolem", outJson);
        Assert.Contains("Wolf", outJson);
    }

    [Fact]
    public void RemovePropertyOnArrayElementsWhere_strips_property_keeps_row()
    {
        var editor = new JsonAssetEditor(CreatureTableJson);
        var updated = editor.RemovePropertyOnArrayElementsWhere(
            "/Rows",
            "Name",
            "RockGolem",
            "AdditionalDamageStat");
        var outJson = editor.ToJson();
        Assert.Equal(1, updated);
        Assert.Contains("RockGolem", outJson);
        Assert.DoesNotContain("RockGolemExtraDamage", outJson);
        Assert.Contains("Juvenile_RockGolem", outJson);
    }

    [Fact]
    public void SetOnArrayElementsWhere_updates_nested_property()
    {
        var editor = new JsonAssetEditor(CreatureTableJson);
        var updated = editor.SetOnArrayElementsWhere(
            "/Rows",
            "Name",
            "Wolf",
            "/SkinningXPEvent/RowName",
            "None");
        var outJson = editor.ToJson();
        Assert.Equal(1, updated);
        Assert.Contains("\"RowName\": \"None\"", outJson);
        Assert.Contains("Skin_RockGolem_Juvenile", outJson);
    }

    [Fact]
    public void JsonAssetPatcher_removewhere_and_removepropertywhere()
    {
        var json = JsonAssetPatcher.Apply(CreatureTableJson,
        [
            new PatchOperation
            {
                Op = "removewhere",
                Path = "/Rows",
                MatchProperty = "Name",
                MatchValue = "Juvenile_RockGolem",
            },
            new PatchOperation
            {
                Op = "removepropertywhere",
                Path = "/Rows",
                MatchProperty = "Name",
                MatchValue = "RockGolem",
                TargetPath = "AdditionalDamageStat",
            },
        ]);

        Assert.DoesNotContain("Juvenile_RockGolem", json);
        Assert.Contains("RockGolem", json);
        Assert.DoesNotContain("RockGolemExtraDamage", json);
    }
}
