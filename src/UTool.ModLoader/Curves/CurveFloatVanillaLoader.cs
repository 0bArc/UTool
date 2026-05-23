using System.Text.Json;
using System.Text.Json.Nodes;
using UTool.Sdk;

namespace UTool.ModLoader.Curves;

internal static class CurveFloatVanillaLoader
{
    public static IReadOnlyList<CurveKey> ReadKeys(string uassetPath)
    {
        var json = new UAssetAPI.UAsset(uassetPath, UAssetAPI.UnrealTypes.EngineVersion.VER_UE4_27).SerializeJson();
        var root = JsonNode.Parse(json)
            ?? throw new InvalidOperationException("UAsset JSON root is null.");

        var apiKeys = FindUAssetApiKeyEntries(root).ToList();
        if (apiKeys.Count > 0)
        {
            return apiKeys
                .Select(e => new CurveKey(ReadUAssetApiKeyTime(e.Entry), ReadUAssetApiKeyValue(e.Entry)))
                .OrderBy(k => k.Time)
                .ToList();
        }

        var simple = FindSimpleRichCurveKeyArrays(root).FirstOrDefault();
        if (simple is null)
            return [];

        return simple
            .OfType<JsonObject>()
            .Select(obj => new CurveKey(
                obj["Time"]?.GetValue<float>() ?? obj["time"]?.GetValue<float>() ?? 0,
                obj["Value"]?.GetValue<float>() ?? obj["value"]?.GetValue<float>() ?? 0))
            .OrderBy(k => k.Time)
            .ToList();
    }

    private sealed record UAssetApiKeyEntry(JsonObject Entry);

    private static IEnumerable<UAssetApiKeyEntry> FindUAssetApiKeyEntries(JsonNode node)
    {
        if (node is JsonObject obj)
        {
            var isKeysArray = obj["Name"]?.GetValue<string>()?.Equals("Keys", StringComparison.OrdinalIgnoreCase) == true
                && obj["$type"]?.GetValue<string>()?.Contains("ArrayPropertyData", StringComparison.Ordinal) == true;

            if (isKeysArray
                && obj.TryGetPropertyValue("Value", out var valueNode)
                && valueNode is JsonArray valueArr
                && valueArr.Count > 0
                && valueArr[0] is JsonObject first
                && first["$type"]?.GetValue<string>()?.Contains("StructPropertyData", StringComparison.Ordinal) == true
                && first["StructType"]?.GetValue<string>()?.Equals("RichCurveKey", StringComparison.OrdinalIgnoreCase) == true)
            {
                for (var i = 0; i < valueArr.Count; i++)
                {
                    if (valueArr[i] is JsonObject entry)
                        yield return new UAssetApiKeyEntry(entry);
                }

                yield break;
            }

            foreach (var prop in obj)
            {
                if (prop.Value is not null)
                {
                    foreach (var hit in FindUAssetApiKeyEntries(prop.Value))
                        yield return hit;
                }
            }
        }
        else if (node is JsonArray arr)
        {
            foreach (var item in arr)
            {
                if (item is not null)
                {
                    foreach (var hit in FindUAssetApiKeyEntries(item))
                        yield return hit;
                }
            }
        }
    }

    private static IEnumerable<JsonArray> FindSimpleRichCurveKeyArrays(JsonNode node)
    {
        if (node is JsonObject obj)
        {
            foreach (var prop in obj)
            {
                if (prop.Key.Equals("Keys", StringComparison.OrdinalIgnoreCase)
                    && prop.Value is JsonArray keys
                    && keys.Count > 0
                    && keys[0] is JsonObject first
                    && (first.ContainsKey("Time") || first.ContainsKey("time")))
                    yield return keys;

                if (prop.Value is not null)
                {
                    foreach (var hit in FindSimpleRichCurveKeyArrays(prop.Value))
                        yield return hit;
                }
            }
        }
        else if (node is JsonArray arr)
        {
            foreach (var item in arr)
            {
                if (item is not null)
                {
                    foreach (var hit in FindSimpleRichCurveKeyArrays(item))
                        yield return hit;
                }
            }
        }
    }

    private static float ReadUAssetApiKeyTime(JsonObject entry)
    {
        if (entry["Value"] is JsonArray inner
            && inner[0] is JsonObject prop
            && prop["Value"] is JsonObject val)
            return ParseFloat(val["Time"]);

        if (entry["Value"] is JsonObject direct)
            return ParseFloat(direct["Time"]);

        return 0;
    }

    private static float ReadUAssetApiKeyValue(JsonObject entry)
    {
        if (entry["Value"] is JsonArray inner
            && inner[0] is JsonObject prop
            && prop["Value"] is JsonObject val)
            return ParseFloat(val["Value"]);

        if (entry["Value"] is JsonObject direct)
            return ParseFloat(direct["Value"]);

        return 0;
    }

    private static float ParseFloat(JsonNode? node)
    {
        if (node is null)
            return 0;

        if (node is JsonValue val)
        {
            if (val.TryGetValue<float>(out var f))
                return f;

            var text = val.ToString().TrimStart('+');
            return float.TryParse(text, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var parsed)
                ? parsed
                : 0;
        }

        return 0;
    }
}
