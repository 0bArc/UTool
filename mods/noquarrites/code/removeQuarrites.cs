using CsStratware.Sdk;

/// <summary>Shared gate + helpers for stripping quarrite / rock golem spawn data.</summary>
internal static class QuarriteRemoval
{
    public const string Accolade = "DefeatQuarrite";

    private const string WorldJuvenileStatKey = "(Value=\"WorldJuvenileRockGolemWorldSpawn_?\")";
    private const string DesertJuvenileStatKey = "(Value=\"WorldJuvenileRockGolemDesertSpawn_?\")";

    private static readonly HashSet<string> AutonomousSpawnerRows = new(StringComparer.Ordinal)
    {
        "RockGolem",
        "RockGolem_Ice",
        "RockGolem_Desert",
    };

    private static readonly string[] EpicCreatureRows =
    [
        "RockGolem",
        "RockGolemJr_A",
        "RockGolemJr_D",
        "RockGolemJr_E",
        "RockGolemJr_F",
        "RockGolemJr_Exotic",
    ];

    public static bool ShouldApply(IPlayerSaveContext saves) =>
        saves.AnyProfileHasCompletedAccolade(Accolade);

    public static void RemoveCreatureTypes(JsonAssetEditor editor)
    {
        editor.RemoveArrayElementsWhere("/Rows", "Name", "Juvenile_RockGolem");
        editor.RemoveArrayElementsWhere("/Rows", "Name", "RockGolem");
    }

    public static void RemoveAutonomousSpawns(JsonAssetEditor editor)
    {
        foreach (var name in AutonomousSpawnerRows)
            editor.RemoveArrayElementsWhere("/Rows", "Name", name);
    }

    public static void RemoveSpawnRules(JsonAssetEditor editor)
    {
        editor.RemoveArrayElementsWhere("/Rows", "Name", "Quarrite_Population");
        editor.RemoveArrayElementsWhere("/Rows", "Name", "QuarriteArctic__Population");
    }

    public static void RemoveOlympusSpawnConfig(JsonAssetEditor editor)
    {
        editor.RemovePropertyOnArrayElementsWhere(
            "/Rows",
            "Name",
            "Olympus",
            "AISpawnRules/(Value=\"Juvenile_Rock_Golem\")");
        editor.RemovePropertyOnArrayElementsWhere(
            "/Rows",
            "Name",
            "Olympus",
            "AISpawnRules/(Value=\"Juvenile_Rock_Golem_Arctic\")");
    }

    public static void RemoveEpicCreatures(JsonAssetEditor editor)
    {
        foreach (var name in EpicCreatureRows)
            editor.RemoveArrayElementsWhere("/Rows", "Name", name);
    }

    public static void StripSpawnZones(JsonAssetEditor editor)
    {
        for (var i = 0; ; i++)
        {
            string? zoneName;
            try
            {
                zoneName = editor.TryGetString($"/Rows/{i}/Name");
            }
            catch (InvalidOperationException)
            {
                break;
            }

            if (zoneName is null)
                break;

            TryRemove(editor, $"/Rows/{i}/Creatures/WorldStatInjection/{WorldJuvenileStatKey}");
            TryRemove(editor, $"/Rows/{i}/Creatures/WorldStatInjection/{DesertJuvenileStatKey}");

            for (var j = 0; ; j++)
            {
                string? rowName;
                try
                {
                    rowName = editor.TryGetString($"/Rows/{i}/Creatures/RelevantAutonomousSpawners/{j}/RowName");
                }
                catch (InvalidOperationException)
                {
                    break;
                }

                if (rowName is null)
                    break;

                if (!AutonomousSpawnerRows.Contains(rowName))
                    continue;

                editor.Remove($"/Rows/{i}/Creatures/RelevantAutonomousSpawners/{j}");
                j--;
            }
        }
    }

    private static void TryRemove(JsonAssetEditor editor, string pointer)
    {
        try
        {
            editor.Remove(pointer);
        }
        catch (InvalidOperationException)
        {
        }
    }
}

[PatchAsset("D_AICreatureType.json")]
public sealed class RemoveQuarritesCreatureTypePatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) => QuarriteRemoval.ShouldApply(saves);

    public override void Apply(JsonAssetEditor editor) => QuarriteRemoval.RemoveCreatureTypes(editor);
}

[PatchAsset("D_AutonomousSpawns.json")]
public sealed class RemoveQuarritesAutonomousSpawnsPatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) => QuarriteRemoval.ShouldApply(saves);

    public override void Apply(JsonAssetEditor editor) => QuarriteRemoval.RemoveAutonomousSpawns(editor);
}

[PatchAsset("D_AISpawnRules.json")]
public sealed class RemoveQuarritesSpawnRulesPatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) => QuarriteRemoval.ShouldApply(saves);

    public override void Apply(JsonAssetEditor editor) => QuarriteRemoval.RemoveSpawnRules(editor);
}

[PatchAsset("D_AISpawnConfig.json")]
public sealed class RemoveQuarritesSpawnConfigPatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) => QuarriteRemoval.ShouldApply(saves);

    public override void Apply(JsonAssetEditor editor) => QuarriteRemoval.RemoveOlympusSpawnConfig(editor);
}

[PatchAsset("D_AISpawnZones.json")]
public sealed class RemoveQuarritesSpawnZonesPatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) => QuarriteRemoval.ShouldApply(saves);

    public override void Apply(JsonAssetEditor editor) => QuarriteRemoval.StripSpawnZones(editor);
}

[PatchAsset("D_EpicCreatures.json")]
public sealed class RemoveQuarritesEpicCreaturesPatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) => QuarriteRemoval.ShouldApply(saves);

    public override void Apply(JsonAssetEditor editor) => QuarriteRemoval.RemoveEpicCreatures(editor);
}
