using CsStratware.ModLoader.Merge;

namespace CsStratware.ModLoader;

/// <summary>Backward-compatible entry point for UE DataTable JSON merge.</summary>
public static class UeJsonMerger
{
    public static string Merge(string baseJson, string overlayJson) =>
        UeDataTableMerger.MergeToJson(baseJson, overlayJson);

    public static UeDataTableMergeResult MergeWithReport(
        string baseJson,
        string overlayJson,
        UeDataTableMergeOptions? options = null) =>
        UeDataTableMerger.Merge(baseJson, overlayJson, options);
}
