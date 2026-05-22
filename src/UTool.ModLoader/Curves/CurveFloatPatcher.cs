using System.Globalization;
using System.Text.Json;
using System.Text.Json.Nodes;
using UAssetAPI;
using UAssetAPI.UnrealTypes;

namespace UTool.ModLoader.Curves;

public static class CurveFloatPatcher
{
    public static void ApplyKeys(string uassetPath, CurveFloatPatchSpec spec)
    {
        if (!File.Exists(uassetPath))
            throw new FileNotFoundException($"Curve uasset not found: {uassetPath}");

        var asset = new UAsset(uassetPath, EngineVersion.VER_UE4_27);
        var json = asset.SerializeJson();
        var patchedJson = PatchRichCurveKeys(json, spec);
        var patched = UAsset.DeserializeJson(patchedJson);
        patched.Write(uassetPath);
    }

    internal static string PatchRichCurveKeys(string uassetJson, CurveFloatPatchSpec spec)
    {
        var root = JsonNode.Parse(uassetJson)
            ?? throw new InvalidOperationException("UAsset JSON root is null.");

        if (TryPatchUAssetApiJson(root, spec))
            return root.ToJsonString(new JsonSerializerOptions { WriteIndented = false });

        var keysArrays = FindSimpleRichCurveKeyArrays(root).ToList();
        if (keysArrays.Count == 0)
            throw new InvalidOperationException("No RichCurve Keys array found in UAsset JSON.");

        PatchSimpleKeyArrays(keysArrays, spec);
        return root.ToJsonString(new JsonSerializerOptions { WriteIndented = false });
    }

    private static bool TryPatchUAssetApiJson(JsonNode root, CurveFloatPatchSpec spec)
    {
        var keyEntries = FindUAssetApiKeyEntries(root).ToList();
        if (keyEntries.Count == 0)
            return false;

        var parentArray = keyEntries[0].ParentArray
            ?? throw new InvalidOperationException("UAssetAPI curve key parent array missing.");

        var template = keyEntries[0].Entry.DeepClone() as JsonObject
            ?? throw new InvalidOperationException("Failed to clone UAssetAPI curve key template.");
        var minTime = spec.MinPatchTime ?? spec.Keys.Min(k => k.Time);
        var rebuilt = new JsonArray();

        if (spec.ExtendFromVanilla)
        {
            foreach (var entry in keyEntries)
            {
                var time = ReadUAssetApiKeyTime(entry.Entry);
                if (time < minTime)
                    rebuilt.Add(entry.Entry.DeepClone());
            }
        }

        foreach (var key in spec.Keys.OrderBy(k => k.Time))
            rebuilt.Add(CreateUAssetApiKeyEntry(template, key.Time, key.Value));

        parentArray.Clear();
        foreach (var item in rebuilt)
            parentArray.Add(item?.DeepClone());

        return true;
    }

    private sealed record UAssetApiKeyEntry(JsonArray ParentArray, JsonObject Entry);

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
                        yield return new UAssetApiKeyEntry(valueArr, entry);
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

    private static float ReadUAssetApiKeyTime(JsonObject entry)
    {
        if (entry["Value"] is JsonArray inner
            && inner[0] is JsonObject prop
            && prop["Value"] is JsonObject val)
            return ParseUAssetFloat(val["Time"]);

        if (entry["Value"] is JsonObject direct)
            return ParseUAssetFloat(direct["Time"]);

        return 0;
    }

    private static JsonObject CreateUAssetApiKeyEntry(JsonObject template, float time, float value)
    {
        var clone = template.DeepClone() as JsonObject
            ?? throw new InvalidOperationException("Failed to clone curve key template.");

        if (clone["Value"] is JsonArray inner && inner[0] is JsonObject prop && prop["Value"] is JsonObject val)
        {
            val["Time"] = FormatUAssetFloat(time);
            val["Value"] = FormatUAssetFloat(value);
        }
        else if (clone["Value"] is JsonObject direct)
        {
            direct["Time"] = FormatUAssetFloat(time);
            direct["Value"] = FormatUAssetFloat(value);
        }

        return clone;
    }

    private static void PatchSimpleKeyArrays(IReadOnlyList<JsonArray> keysArrays, CurveFloatPatchSpec spec)
    {
        var minTime = spec.MinPatchTime ?? spec.Keys.Min(k => k.Time);
        var newKeys = new JsonArray();

        if (spec.ExtendFromVanilla && keysArrays.Count > 0)
        {
            foreach (var item in keysArrays[0])
            {
                if (item is not JsonObject obj)
                    continue;

                var time = obj["Time"]?.GetValue<float>() ?? obj["time"]?.GetValue<float>();
                if (!time.HasValue || time.Value < minTime)
                    newKeys.Add(obj.DeepClone());
            }
        }

        foreach (var key in spec.Keys.OrderBy(k => k.Time))
        {
            newKeys.Add(new JsonObject
            {
                ["Time"] = key.Time,
                ["Value"] = key.Value,
                ["InterpMode"] = "RCIM_Linear",
                ["TangentMode"] = "RCTM_Auto",
            });
        }

        foreach (var array in keysArrays)
        {
            array.Clear();
            foreach (var item in newKeys)
                array.Add(item?.DeepClone());
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
                {
                    yield return keys;
                }

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

    private static float ParseUAssetFloat(JsonNode? node)
    {
        if (node is null)
            return 0;

        if (node is JsonValue val)
        {
            if (val.TryGetValue<float>(out var f))
                return f;

            var text = val.ToString().TrimStart('+');
            return float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed)
                ? parsed
                : 0;
        }

        return 0;
    }

    private static string FormatUAssetFloat(float value) =>
        value.ToString("0.######", CultureInfo.InvariantCulture);
}
