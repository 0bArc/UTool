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
}
