using System.Text.Json.Nodes;
using UTool.Core.Json;

namespace UTool.Sdk;

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
        SetAtPointer(_root, jsonPointer, JsonNodeConversion.ToNode(value), replace: true, createMissing: false);

    public void Add(string jsonPointer, object? value) =>
        SetAtPointer(_root, jsonPointer, JsonNodeConversion.ToNode(value), replace: false, createMissing: false);

    /// <summary>Set value at pointer; creates missing object segments along the path.</summary>
    public void Set(string jsonPointer, object? value) =>
        SetAtPointer(_root, jsonPointer, JsonNodeConversion.ToNode(value), replace: true, createMissing: true);

    /// <summary>Append an element to the array at <paramref name="arrayPointer"/>.</summary>
    public void Append(string arrayPointer, object? value)
    {
        var node = JsonNodeConversion.ToNode(value)
            ?? throw new ArgumentException("Value is required.", nameof(value));
        ResolveArray(arrayPointer).Add(node);
    }

    /// <summary>Parse JSON and append to the array at <paramref name="arrayPointer"/>.</summary>
    public void AppendJson(string arrayPointer, string json) =>
        Append(arrayPointer, JsonNode.Parse(json) ?? throw new InvalidOperationException("JSON is null."));

    /// <summary>Deep-merge an object at <paramref name="jsonPointer"/> (creates path if needed).</summary>
    public void MergeInto(string jsonPointer, object? value)
    {
        var overlay = JsonNodeConversion.ToNode(value) as JsonObject
            ?? throw new ArgumentException("Merge value must be a JSON object.", nameof(value));

        var target = ResolveOrCreateObject(_root, jsonPointer);
        DeepMergeObjects(target, overlay);
    }

    /// <summary>Update row matching <paramref name="keyProperty"/> or append if missing.</summary>
    /// <returns>(added, updated) counts.</returns>
    public (int Added, int Updated) UpsertArrayElement(
        string arrayPointer,
        string keyProperty,
        object? keyValue,
        object? element,
        bool merge = true)
    {
        var row = JsonNodeConversion.ToNode(element) as JsonObject
            ?? throw new ArgumentException("Element must be a JSON object.", nameof(element));

        var arr = ResolveArray(arrayPointer);
        for (var i = 0; i < arr.Count; i++)
        {
            if (arr[i] is not JsonObject existing || !PropertyMatches(existing, keyProperty, keyValue))
                continue;

            if (merge)
                DeepMergeObjects(existing, row);
            else
                arr[i] = row.DeepClone();

            return (0, 1);
        }

        arr.Add(row.DeepClone());
        return (1, 0);
    }

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
        var node = JsonNodeConversion.ToNode(value);
        var updated = 0;
        foreach (var obj in EnumerateMatchingArrayObjects(arrayPointer, matchProperty, matchValue))
        {
            SetAtPointer(obj, pointer, node, replace: true, createMissing: true);
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
        var nodeValue = JsonNodeConversion.ToNode(value);
        if (string.IsNullOrWhiteSpace(underPointer))
        {
            Walk(_root, node =>
            {
                if (node is JsonObject obj && obj.ContainsKey(propertyName))
                    obj[propertyName] = nodeValue?.DeepClone();
            });
            return;
        }

        var subtree = ResolveSubtree(_root, underPointer)
            ?? throw new InvalidOperationException($"Subtree not found: {underPointer}");

        Walk(subtree, node =>
        {
            if (node is JsonObject obj && obj.ContainsKey(propertyName))
                obj[propertyName] = nodeValue?.DeepClone();
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

    public string ToJson() => _root.ToJsonString(UToolJson.Options);

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

    private static void SetAtPointer(
        JsonNode root,
        string pointer,
        JsonNode? value,
        bool replace,
        bool createMissing)
    {
        var (parent, key) = ResolveParent(root, pointer, createMissing);
        ApplyToParent(parent, key, value, replace);
    }

    private static void ApplyToParent(JsonNode parent, string key, JsonNode? value, bool replace)
    {
        if (parent is JsonObject obj)
        {
            obj[key] = value;
            return;
        }

        if (parent is not JsonArray arr)
            throw new InvalidOperationException($"Cannot set value at parent type {parent.GetType().Name}");

        if (key == "-")
        {
            arr.Add(value);
            return;
        }

        if (!int.TryParse(key, out var index))
            throw new InvalidOperationException($"Invalid array index '{key}'.");

        if (replace)
            arr[index] = value;
        else
            arr.Insert(index, value);
    }

    private static void RemoveAtPointer(JsonNode root, string pointer)
    {
        var (parent, key) = ResolveParent(root, pointer, createMissing: false);
        if (parent is JsonObject obj)
            obj.Remove(key);
        else if (parent is JsonArray arr && int.TryParse(key, out var index))
            arr.RemoveAt(index);
        else
            throw new InvalidOperationException($"Cannot remove at {pointer}");
    }

    private static JsonObject ResolveOrCreateObject(JsonNode root, string pointer)
    {
        if (!pointer.StartsWith('/'))
            throw new FormatException($"JSON pointer must start with '/': {pointer}");

        var segments = pointer.Trim('/').Split('/', StringSplitOptions.RemoveEmptyEntries)
            .Select(Unescape)
            .ToList();

        if (segments.Count == 0)
        {
            if (root is JsonObject obj)
                return obj;

            throw new InvalidOperationException("Root is not an object.");
        }

        var current = root;
        foreach (var segment in segments)
        {
            var next = Navigate(current, segment);
            if (next is null)
            {
                next = new JsonObject();
                AssignChild(current, segment, next);
            }

            current = next;
        }

        return current as JsonObject
            ?? throw new InvalidOperationException($"JSON node at {pointer} is not an object.");
    }

    private static (JsonNode Parent, string Key) ResolveParent(
        JsonNode root,
        string pointer,
        bool createMissing)
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
            var segment = segments[i];
            var next = Navigate(current, segment);
            if (next is null)
            {
                if (!createMissing)
                    throw new InvalidOperationException($"Missing segment '{segment}' in {pointer}");

                next = new JsonObject();
                AssignChild(current, segment, next);
            }

            current = next;
        }

        return (current, segments[^1]);
    }

    private static void AssignChild(JsonNode parent, string segment, JsonNode child)
    {
        switch (parent)
        {
            case JsonObject obj:
                obj[segment] = child;
                break;
            case JsonArray arr when int.TryParse(segment, out var index):
                if (index < 0 || index > arr.Count)
                    throw new InvalidOperationException($"Array index out of range: {index}");
                if (index == arr.Count)
                    arr.Add(child);
                else
                    arr[index] = child;
                break;
            default:
                throw new InvalidOperationException($"Cannot assign child under {parent.GetType().Name}.");
        }
    }

    private static JsonNode? Navigate(JsonNode node, string segment)
    {
        if (node is JsonObject obj)
            return obj.TryGetPropertyValue(segment, out var child) ? child : null;

        if (node is JsonArray arr && int.TryParse(segment, out var index) && index >= 0 && index < arr.Count)
            return arr[index];

        return null;
    }

    private static void DeepMergeObjects(JsonObject target, JsonObject overlay)
    {
        foreach (var (key, overlayValue) in overlay)
        {
            if (overlayValue is null)
            {
                target.Remove(key);
                continue;
            }

            if (target.TryGetPropertyValue(key, out var existing)
                && existing is JsonObject existingObj
                && overlayValue is JsonObject overlayObj)
            {
                DeepMergeObjects(existingObj, overlayObj);
                continue;
            }

            target[key] = overlayValue.DeepClone();
        }
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

        return JsonValuesEqual(actual, JsonNodeConversion.ToNode(expected));
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

        return actual.ToJsonString(UToolJson.Options) == expected.ToJsonString(UToolJson.Options);
    }
}
