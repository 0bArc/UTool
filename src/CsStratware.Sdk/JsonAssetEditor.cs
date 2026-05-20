using System.Text.Json.Nodes;
using CsStratware.Core.Json;

namespace CsStratware.Sdk;

/// <summary>Mutable JSON document for mod code. Same ops as declarative patch JSON.</summary>
public sealed class JsonAssetEditor
{
    private readonly JsonNode _root;

    public JsonAssetEditor(string json)
    {
        _root = JsonNode.Parse(json)
            ?? throw new InvalidOperationException("JSON root is null.");
    }

    public void Replace(string jsonPointer, object? value) =>
        SetAtPointer(_root, jsonPointer, JsonValue.Create(value), replace: true);

    public void Add(string jsonPointer, object? value) =>
        SetAtPointer(_root, jsonPointer, JsonValue.Create(value), replace: false);

    public void Remove(string jsonPointer) => RemoveAtPointer(_root, jsonPointer);

    /// <summary>Remove every object in the array at <paramref name="arrayPointer"/> where <paramref name="matchProperty"/> equals <paramref name="matchValue"/>.</summary>
    /// <returns>Number of elements removed.</returns>
    public int RemoveArrayElementsWhere(string arrayPointer, string matchProperty, object? matchValue)
    {
        var arr = ResolveArray(arrayPointer);
        var removed = 0;
        for (var i = arr.Count - 1; i >= 0; i--)
        {
            if (arr[i] is JsonObject obj && PropertyMatches(obj, matchProperty, matchValue))
            {
                arr.RemoveAt(i);
                removed++;
            }
        }

        return removed;
    }

    /// <summary>Set <paramref name="propertyPointer"/> (relative to each matched array element) to <paramref name="value"/>.</summary>
    /// <returns>Number of elements updated.</returns>
    public int SetOnArrayElementsWhere(
        string arrayPointer,
        string matchProperty,
        object? matchValue,
        string propertyPointer,
        object? value)
    {
        var pointer = NormalizeRelativePointer(propertyPointer);
        var updated = 0;
        foreach (var obj in EnumerateMatchingArrayObjects(arrayPointer, matchProperty, matchValue))
        {
            SetAtPointer(obj, pointer, JsonValue.Create(value), replace: true);
            updated++;
        }

        return updated;
    }

    /// <summary>Remove <paramref name="propertyPointer"/> (relative to each matched array element).</summary>
    /// <returns>Number of elements updated.</returns>
    public int RemovePropertyOnArrayElementsWhere(
        string arrayPointer,
        string matchProperty,
        object? matchValue,
        string propertyPointer)
    {
        var pointer = NormalizeRelativePointer(propertyPointer);
        var updated = 0;
        foreach (var obj in EnumerateMatchingArrayObjects(arrayPointer, matchProperty, matchValue))
        {
            RemoveAtPointer(obj, pointer);
            updated++;
        }

        return updated;
    }

    /// <summary>Replace property named <paramref name="propertyName"/> on every object in the tree.</summary>
    public void ReplaceAll(string propertyName, object? value) =>
        ReplaceAll(propertyName, value, underPointer: null);

    /// <summary>Replace property only within subtree at <paramref name="underPointer"/> (JSON pointer).</summary>
    public void ReplaceAll(string propertyName, object? value, string? underPointer)
    {
        if (string.IsNullOrWhiteSpace(underPointer))
        {
            Walk(_root, node =>
            {
                if (node is JsonObject obj && obj.ContainsKey(propertyName))
                    obj[propertyName] = JsonValue.Create(value);
            });
            return;
        }

        var subtree = ResolveSubtree(_root, underPointer)
            ?? throw new InvalidOperationException($"Subtree not found: {underPointer}");

        Walk(subtree, node =>
        {
            if (node is JsonObject obj && obj.ContainsKey(propertyName))
                obj[propertyName] = JsonValue.Create(value);
        });
    }

    private static JsonNode? ResolveSubtree(JsonNode root, string pointer)
    {
        if (!pointer.StartsWith('/'))
            throw new FormatException($"JSON pointer must start with '/': {pointer}");

        var segments = pointer.Trim('/').Split('/', StringSplitOptions.RemoveEmptyEntries)
            .Select(Unescape)
            .ToList();
        var current = root;
        foreach (var segment in segments)
        {
            current = Navigate(current, segment)
                ?? throw new InvalidOperationException($"Missing segment '{segment}' in {pointer}");
        }

        return current;
    }

    public bool ContainsArrayElementWhere(string arrayPointer, string matchProperty, object? matchValue) =>
        EnumerateMatchingArrayObjects(arrayPointer, matchProperty, matchValue).Any();

    public int CountArrayElementsWhere(string arrayPointer, string matchProperty, object? matchValue)
    {
        var count = 0;
        foreach (var _ in EnumerateMatchingArrayObjects(arrayPointer, matchProperty, matchValue))
            count++;
        return count;
    }

    /// <summary>Remove matching keys from every object in the tree (e.g. UE map keys for creature types).</summary>
    public int RemoveObjectKeys(Func<string, bool> keyPredicate)
    {
        var removed = 0;
        Walk(_root, node =>
        {
            if (node is not JsonObject obj)
                return;

            foreach (var key in obj.Select(static p => p.Key).Where(keyPredicate).ToList())
            {
                obj.Remove(key);
                removed++;
            }
        });
        return removed;
    }

    /// <summary>Remove matching elements from every array in the tree.</summary>
    public int RemoveArrayElementsWhere(Func<JsonObject, bool> predicate)
    {
        var removed = 0;
        Walk(_root, node =>
        {
            if (node is not JsonArray arr)
                return;

            for (var i = arr.Count - 1; i >= 0; i--)
            {
                if (arr[i] is JsonObject obj && predicate(obj))
                {
                    arr.RemoveAt(i);
                    removed++;
                }
            }
        });
        return removed;
    }

    public string? TryGetString(string jsonPointer)
    {
        if (!jsonPointer.StartsWith('/'))
            return null;

        var node = ResolveSubtree(_root, jsonPointer);
        return node is JsonValue v && v.TryGetValue<string>(out var s) ? s : null;
    }

    public string ToJson() => _root.ToJsonString(StratwareJson.Options);

    private static void Walk(JsonNode? node, Action<JsonNode> visit)
    {
        if (node is null)
            return;

        visit(node);

        switch (node)
        {
            case JsonObject obj:
                foreach (var child in obj)
                    Walk(child.Value, visit);
                break;
            case JsonArray arr:
                foreach (var child in arr)
                    Walk(child, visit);
                break;
        }
    }

    private static void SetAtPointer(JsonNode root, string pointer, JsonNode? value, bool replace)
    {
        var (parent, key) = ResolveParent(root, pointer);
        if (parent is JsonObject obj)
            obj[key] = value;
        else if (parent is JsonArray arr && int.TryParse(key, out var index))
        {
            if (replace)
                arr[index] = value;
            else
                arr.Insert(index, value);
        }
        else
            throw new InvalidOperationException($"Cannot set value at {pointer}");
    }

    private static void RemoveAtPointer(JsonNode root, string pointer)
    {
        var (parent, key) = ResolveParent(root, pointer);
        if (parent is JsonObject obj)
            obj.Remove(key);
        else if (parent is JsonArray arr && int.TryParse(key, out var index))
            arr.RemoveAt(index);
        else
            throw new InvalidOperationException($"Cannot remove at {pointer}");
    }

    private static (JsonNode Parent, string Key) ResolveParent(JsonNode root, string pointer)
    {
        if (!pointer.StartsWith('/'))
            throw new FormatException($"JSON pointer must start with '/': {pointer}");

        var segments = pointer.Trim('/').Split('/', StringSplitOptions.RemoveEmptyEntries)
            .Select(Unescape)
            .ToList();

        if (segments.Count == 0)
            throw new FormatException("Empty JSON pointer.");

        var current = root;
        for (var i = 0; i < segments.Count - 1; i++)
        {
            current = Navigate(current, segments[i])
                ?? throw new InvalidOperationException($"Missing segment '{segments[i]}' in {pointer}");
        }

        return (current, segments[^1]);
    }

    private static JsonNode? Navigate(JsonNode node, string segment)
    {
        if (node is JsonObject obj)
            return obj.TryGetPropertyValue(segment, out var child) ? child : null;

        if (node is JsonArray arr && int.TryParse(segment, out var index) && index >= 0 && index < arr.Count)
            return arr[index];

        return null;
    }

    private static string Unescape(string segment) =>
        segment.Replace("~1", "/").Replace("~0", "~");

    private static string NormalizeRelativePointer(string pointer)
    {
        if (string.IsNullOrWhiteSpace(pointer))
            throw new ArgumentException("Property pointer is required.", nameof(pointer));

        return pointer.StartsWith('/') ? pointer : "/" + pointer;
    }

    private IEnumerable<JsonObject> EnumerateMatchingArrayObjects(
        string arrayPointer,
        string matchProperty,
        object? matchValue)
    {
        var arr = ResolveArray(arrayPointer);
        for (var i = 0; i < arr.Count; i++)
        {
            if (arr[i] is JsonObject obj && PropertyMatches(obj, matchProperty, matchValue))
                yield return obj;
        }
    }

    private static JsonArray ResolveArray(JsonNode root, string pointer)
    {
        var node = string.IsNullOrWhiteSpace(pointer) || pointer == "/"
            ? root
            : ResolveSubtree(root, pointer)
                ?? throw new InvalidOperationException($"Array not found: {pointer}");

        if (node is JsonArray arr)
            return arr;

        throw new InvalidOperationException($"JSON node at {pointer} is not an array.");
    }

    private JsonArray ResolveArray(string pointer) => ResolveArray(_root, pointer);

    private static bool PropertyMatches(JsonObject obj, string propertyName, object? expected)
    {
        if (!obj.TryGetPropertyValue(propertyName, out var actual))
            return expected is null;

        return JsonValuesEqual(actual, JsonValue.Create(expected));
    }

    private static bool JsonValuesEqual(JsonNode? actual, JsonNode? expected)
    {
        if (actual is null && expected is null)
            return true;

        if (actual is null || expected is null)
            return false;

        if (actual is JsonValue av && expected is JsonValue ev)
        {
            if (av.TryGetValue<string>(out var asStr) && ev.TryGetValue<string>(out var esStr))
                return string.Equals(asStr, esStr, StringComparison.Ordinal);

            if (av.TryGetValue<bool>(out var asBool) && ev.TryGetValue<bool>(out var esBool))
                return asBool == esBool;

            if (av.TryGetValue<int>(out var asInt) && ev.TryGetValue<int>(out var esInt))
                return asInt == esInt;

            if (av.TryGetValue<long>(out var asLong) && ev.TryGetValue<long>(out var esLong))
                return asLong == esLong;

            if (av.TryGetValue<double>(out var asDbl) && ev.TryGetValue<double>(out var esDbl))
                return Math.Abs(asDbl - esDbl) < 1e-9;

            if (av.TryGetValue<decimal>(out var asDec) && ev.TryGetValue<decimal>(out var esDec))
                return asDec == esDec;
        }

        return actual.ToJsonString(StratwareJson.Options) == expected.ToJsonString(StratwareJson.Options);
    }
}
