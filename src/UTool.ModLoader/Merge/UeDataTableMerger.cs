using System.Globalization;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace UTool.ModLoader.Merge;

/// <summary>Structured UE DataTable JSON merge (RowName/Name keys, numeric-min, later-wins).</summary>
public static class UeDataTableMerger
{
    public const string DeletionMarkerProperty = "__csDeleted";

    public static readonly string[] RowKeyCandidates = ["RowName", "Name", "ID", "Id"];

    private static readonly JsonSerializerOptions SerializeOptions = new() { WriteIndented = false };

    /// <summary>Count <c>Rows</c>/<c>rows</c> array entries when present; else 0.</summary>
    public static int CountDataTableRows(string json)
    {
        if (string.IsNullOrWhiteSpace(json))
            return 0;

        try
        {
            if (JsonNode.Parse(json) is not JsonObject root)
                return 0;

            foreach (var rowsKey in new[] { "Rows", "rows" })
            {
                if (root[rowsKey] is JsonArray rows)
                    return rows.Count;
            }
        }
        catch (JsonException)
        {
            return 0;
        }

        return 0;
    }

    public static UeDataTableMergeResult MergeChain(
        IReadOnlyList<string> jsonLayers,
        UeDataTableMergeOptions? options = null)
    {
        if (jsonLayers.Count == 0)
            throw new ArgumentException("At least one JSON layer is required.", nameof(jsonLayers));

        options ??= new UeDataTableMergeOptions();
        var report = new MergeConflictCollector(options.AssetLabel);

        var current = JsonNode.Parse(jsonLayers[0])
            ?? throw new InvalidOperationException("Base JSON root is null.");

        for (var i = 1; i < jsonLayers.Count; i++)
        {
            var overlay = JsonNode.Parse(jsonLayers[i])
                ?? throw new InvalidOperationException($"Overlay layer {i} JSON root is null.");
            current = MergeNodes(current, overlay, $"layer:{i}", report, options);
        }

        return new UeDataTableMergeResult
        {
            Json = current.ToJsonString(SerializeOptions),
            Report = report.Build(),
        };
    }

    public static UeDataTableMergeResult Merge(
        string baseJson,
        string overlayJson,
        UeDataTableMergeOptions? options = null) =>
        MergeChain([baseJson, overlayJson], options);

    public static string MergeToJson(
        string baseJson,
        string overlayJson,
        UeDataTableMergeOptions? options = null) =>
        Merge(baseJson, overlayJson, options).Json;

    private static JsonNode MergeNodes(
        JsonNode baseNode,
        JsonNode overlayNode,
        string overlaySource,
        MergeConflictCollector report,
        UeDataTableMergeOptions options)
    {
        if (TryMergeRowsContainer(baseNode, overlayNode, overlaySource, report, options, out var merged))
            return merged;

        if (baseNode is JsonArray baseArr && overlayNode is JsonArray overlayArr)
            return MergeArrays(baseArr, overlayArr, overlaySource, report, options);

        if (baseNode is JsonObject baseObj && overlayNode is JsonObject overlayObj)
        {
            var clone = baseObj.DeepClone() as JsonObject ?? new JsonObject();
            MergeObjects(clone, overlayObj, overlaySource, report, options, rowKey: "", propertyPath: "");
            return clone;
        }

        // Mismatched root types: keep base tree (do not replace entire asset with overlay-only payload).
        return baseNode.DeepClone();
    }

    private static bool TryMergeRowsContainer(
        JsonNode baseNode,
        JsonNode overlayNode,
        string overlaySource,
        MergeConflictCollector report,
        UeDataTableMergeOptions options,
        out JsonNode merged)
    {
        merged = baseNode;
        if (baseNode is not JsonObject baseObj || overlayNode is not JsonObject overlayObj)
            return false;

        foreach (var rowsKey in new[] { "Rows", "rows" })
        {
            if (baseObj[rowsKey] is not JsonArray baseRows)
                continue;
            if (overlayObj[rowsKey] is not JsonArray overlayRows)
                continue;

            var clone = baseObj.DeepClone() as JsonObject ?? new JsonObject();
            clone[rowsKey] = MergeRowArrays(baseRows, overlayRows, overlaySource, report, options);

            foreach (var prop in overlayObj)
            {
                if (string.Equals(prop.Key, rowsKey, StringComparison.OrdinalIgnoreCase))
                    continue;

                if (prop.Value is null)
                {
                    clone[prop.Key] = null;
                    continue;
                }

                clone[prop.Key] = clone[prop.Key] is null
                    ? prop.Value.DeepClone()
                    : MergeNodes(clone[prop.Key]!, prop.Value, overlaySource, report, options);
            }

            merged = clone;
            return true;
        }

        return false;
    }

    private static JsonArray MergeRowArrays(
        JsonArray baseRows,
        JsonArray overlayRows,
        string overlaySource,
        MergeConflictCollector report,
        UeDataTableMergeOptions options)
    {
        var map = new Dictionary<string, JsonObject>(StringComparer.OrdinalIgnoreCase);
        var order = new List<string>();
        var anonFingerprints = new HashSet<string>(StringComparer.Ordinal);

        void Ingest(JsonObject row, string source, bool isOverlay)
        {
            if (IsDeletionRow(row, options.DeletionMarkerProperty))
            {
                if (TryGetRowKey(row, out var delKey))
                {
                    map.Remove(delKey);
                    order.Remove(delKey);
                    report.RecordRowDeletion(delKey, source);
                }

                return;
            }

            if (!TryGetRowKey(row, out var key))
            {
                var fp = ContentFingerprint(row);
                if (!anonFingerprints.Add(fp))
                    return;

                var synthetic = $"__anon_{order.Count}_{fp[..Math.Min(12, fp.Length)]}";
                map[synthetic] = row.DeepClone() as JsonObject ?? new JsonObject();
                order.Add(synthetic);
                return;
            }

            if (!map.TryGetValue(key, out var existing))
            {
                map[key] = row.DeepClone() as JsonObject ?? new JsonObject();
                order.Add(key);
                return;
            }

            MergeObjects(existing, row, source, report, options, key, propertyPath: "");
        }

        foreach (var item in baseRows)
        {
            if (item is JsonObject obj)
                Ingest(obj, "base", isOverlay: false);
        }

        foreach (var item in overlayRows)
        {
            if (item is JsonObject obj)
                Ingest(obj, overlaySource, isOverlay: true);
        }

        var result = new JsonArray();
        foreach (var key in order)
        {
            if (map.TryGetValue(key, out var row))
                result.Add(row.DeepClone());
        }

        return result;
    }

    private static JsonArray MergeArrays(
        JsonArray baseArr,
        JsonArray overlayArr,
        string overlaySource,
        MergeConflictCollector report,
        UeDataTableMergeOptions options)
    {
        if (LooksLikeKeyedRowArray(baseArr) || LooksLikeKeyedRowArray(overlayArr))
            return MergeRowArrays(baseArr, overlayArr, overlaySource, report, options);

        var result = new JsonArray();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var item in baseArr)
        {
            var fp = item?.ToJsonString(SerializeOptions) ?? "null";
            if (seen.Add(fp))
                result.Add(item?.DeepClone());
        }

        foreach (var item in overlayArr)
        {
            var fp = item?.ToJsonString(SerializeOptions) ?? "null";
            if (seen.Add(fp))
                result.Add(item?.DeepClone());
        }

        return result;
    }

    private static void MergeObjects(
        JsonObject target,
        JsonObject overlay,
        string overlaySource,
        MergeConflictCollector report,
        UeDataTableMergeOptions options,
        string rowKey,
        string propertyPath)
    {
        foreach (var prop in overlay)
        {
            if (string.Equals(prop.Key, options.DeletionMarkerProperty, StringComparison.OrdinalIgnoreCase))
                continue;

            var childPath = string.IsNullOrEmpty(propertyPath) ? prop.Key : $"{propertyPath}/{prop.Key}";

            if (prop.Value is null)
            {
                target[prop.Key] = null;
                continue;
            }

            if (target[prop.Key] is JsonObject targetChild && prop.Value is JsonObject overlayChild)
            {
                MergeObjects(targetChild, overlayChild, overlaySource, report, options, rowKey, childPath);
                continue;
            }

            if (target[prop.Key] is JsonArray targetArr && prop.Value is JsonArray overlayArr)
            {
                target[prop.Key] = MergeArrays(targetArr, overlayArr, overlaySource, report, options);
                continue;
            }

            if (target.TryGetPropertyValue(prop.Key, out var existing) && existing is not null)
            {
                var resolved = ResolvePropertyConflict(
                    existing,
                    prop.Value,
                    overlaySource,
                    report,
                    options,
                    rowKey,
                    childPath);
                target[prop.Key] = resolved;
                continue;
            }

            target[prop.Key] = prop.Value.DeepClone();
        }
    }

    private static JsonNode ResolvePropertyConflict(
        JsonNode existing,
        JsonNode incoming,
        string laterSource,
        MergeConflictCollector report,
        UeDataTableMergeOptions options,
        string rowKey,
        string propertyPath)
    {
        if (options.NumericMinWins
            && TryGetNumeric(existing, out var a)
            && TryGetNumeric(incoming, out var b)
            && a != b)
        {
            var min = Math.Min(a, b);
            report.RecordPropertyConflict(
                rowKey,
                propertyPath,
                earlier: FormatNumericConflicting(existing, a, b),
                later: FormatNumericConflicting(incoming, a, b),
                resolved: min.ToString(CultureInfo.InvariantCulture),
                resolution: "numeric-min",
                laterSource);

            return SelectNumericJsonValue(existing, incoming, min);
        }

        if (options.LaterModWinsNonNumeric)
        {
            if (!JsonDeepEquals(existing, incoming))
            {
                report.RecordPropertyConflict(
                    rowKey,
                    propertyPath,
                    existing.ToJsonString(SerializeOptions),
                    incoming.ToJsonString(SerializeOptions),
                    incoming.ToJsonString(SerializeOptions),
                    "later-wins",
                    laterSource);
            }

            return incoming.DeepClone();
        }

        return existing.DeepClone();
    }

    private static bool LooksLikeKeyedRowArray(JsonArray arr)
    {
        foreach (var item in arr)
        {
            if (item is JsonObject obj && TryGetRowKey(obj, out _))
                return true;
        }

        return false;
    }

    private static bool IsDeletionRow(JsonObject row, string marker) =>
        row[marker] is JsonValue v && v.TryGetValue<bool>(out var b) && b;

    public static bool TryGetRowKey(JsonObject row, out string key)
    {
        foreach (var candidate in RowKeyCandidates)
        {
            if (row[candidate] is JsonValue val && val.TryGetValue<string>(out var s) && !string.IsNullOrWhiteSpace(s))
            {
                key = s;
                return true;
            }
        }

        key = "";
        return false;
    }

    private static bool TryGetNumeric(JsonNode node, out double value)
    {
        if (node is JsonValue val)
        {
            if (val.TryGetValue<double>(out value))
                return true;
            if (val.TryGetValue<long>(out var l))
            {
                value = l;
                return true;
            }
            if (val.TryGetValue<int>(out var i))
            {
                value = i;
                return true;
            }
        }

        value = 0;
        return false;
    }

    private static JsonNode SelectNumericJsonValue(JsonNode a, JsonNode b, double min)
    {
        if (TryGetNumeric(a, out var va) && Math.Abs(va - min) < 0.0000001)
            return a.DeepClone();
        if (TryGetNumeric(b, out var vb) && Math.Abs(vb - min) < 0.0000001)
            return b.DeepClone();
        return JsonValue.Create(min)!;
    }

    private static string FormatNumericConflicting(JsonNode node, double a, double b) =>
        node.ToJsonString(SerializeOptions);

    private static bool JsonDeepEquals(JsonNode a, JsonNode b) =>
        string.Equals(
            a.ToJsonString(SerializeOptions),
            b.ToJsonString(SerializeOptions),
            StringComparison.Ordinal);

    private static string ContentFingerprint(JsonObject row) =>
        row.ToJsonString(SerializeOptions);

    private sealed class MergeConflictCollector(string assetLabel)
    {
        private readonly List<PropertyMergeConflict> _property = [];
        private readonly List<RowDeletionRecord> _deletions = [];

        public void RecordPropertyConflict(
            string rowKey,
            string propertyPath,
            string? earlier,
            string? later,
            string? resolved,
            string resolution,
            string laterSource)
        {
            _property.Add(new PropertyMergeConflict
            {
                AssetLabel = assetLabel,
                RowKey = rowKey,
                PropertyPath = propertyPath,
                EarlierValue = earlier,
                LaterValue = later,
                ResolvedValue = resolved,
                Resolution = resolution,
                LaterSource = laterSource,
            });
        }

        public void RecordRowDeletion(string rowKey, string source)
        {
            _deletions.Add(new RowDeletionRecord
            {
                AssetLabel = assetLabel,
                RowKey = rowKey,
                Source = source,
            });
        }

        public MergeConflictReport Build() => new()
        {
            AssetLabel = assetLabel,
            PropertyConflicts = _property,
            RowDeletions = _deletions,
        };
    }
}
