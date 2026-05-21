namespace CsStratware.ModLoader.Merge;

/// <summary>Pick fullest UE DataTable JSON as merge base; overlay other layers in pak order.</summary>
public static class JsonMergeLayerOrdering
{
    public static IReadOnlyList<T> OrderForMerge<T>(IReadOnlyList<T> layers, Func<T, string> getJson)
    {
        if (layers.Count <= 1)
            return layers;

        var indexed = layers.Select((layer, index) => new LayerScore<T>(layer, index, getJson(layer))).ToList();
        var baseIndex = indexed
            .OrderByDescending(x => UeDataTableMerger.CountDataTableRows(x.Json))
            .ThenByDescending(x => x.Json.Length)
            .ThenBy(x => x.Index)
            .First()
            .Index;

        var ordered = new List<T>(layers.Count) { layers[baseIndex] };
        for (var i = 0; i < layers.Count; i++)
        {
            if (i != baseIndex)
                ordered.Add(layers[i]);
        }

        return ordered;
    }

    private readonly record struct LayerScore<T>(T Layer, int Index, string Json);
}
