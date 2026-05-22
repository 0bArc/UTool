using UTool.Infrastructure.PlayerData;
using UTool.ModLoader;
using UTool.Sdk;
using Xunit;

namespace UTool.Tests;

public sealed class PlayerSaveContextTests
{
    private const string AccoladesWithQuarrite = """
        {
          "CompletedAccolades": [
            {
              "Accolade": { "RowName": "DefeatQuarrite", "DataTableName": "D_Accolades" },
              "TimeCompleted": "2026.05.19-00.03.58",
              "ProspectID": "X"
            }
          ]
        }
        """;

    [Fact]
    public void AccoladeQuery_detects_DefeatQuarrite()
    {
        Assert.True(AccoladeQuery.HasCompletedAccolade(AccoladesWithQuarrite, "DefeatQuarrite"));
        Assert.False(AccoladeQuery.HasCompletedAccolade(AccoladesWithQuarrite, "WolfBossKilled"));
    }

    [Fact]
    public void ConditionalAssetPatch_skipped_when_no_save_match()
    {
        var patch = new TestConditionalPatch();
        var emptySaves = new PlayerSaveReader(new PlayerDataStore(CreateTempPlayerDataRoot()));
        Assert.False(patch.ShouldApply(emptySaves));
    }

    [Fact]
    public void ModCodePatchRunner_skips_conditional_patch_in_apply_all()
    {
        const string json = """{ "Rows": [ { "Name": "RockGolem" } ] }""";
        var patch = new TestConditionalPatch();
        var saves = new PlayerSaveReader(new PlayerDataStore(CreateTempPlayerDataRoot()));
        var outJson = ModCodePatchRunner.ApplyAll(json, [patch], saves);
        Assert.Contains("RockGolem", outJson);
    }

    private sealed class TestConditionalPatch : ConditionalAssetPatch
    {
        public override bool ShouldApply(IPlayerSaveContext saves) =>
            saves.AnyProfileHasCompletedAccolade("DefeatQuarrite");

        public override void Apply(JsonAssetEditor editor) =>
            editor.RemoveArrayElementsWhere("/Rows", "Name", "RockGolem");
    }

    private static string CreateTempPlayerDataRoot()
    {
        var root = Path.Combine(Path.GetTempPath(), "utool-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        return root;
    }
}
