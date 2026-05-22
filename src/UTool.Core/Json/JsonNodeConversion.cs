using System.Text.Json;
using System.Text.Json.Nodes;

namespace UTool.Core.Json;

/// <summary>Convert patch values and CLR objects into <see cref="JsonNode"/> trees.</summary>
public static class JsonNodeConversion
{
    public static JsonNode? ToNode(object? value)
    {
        if (value is null)
            return null;

        if (value is JsonNode node)
            return node.DeepClone();

        if (value is JsonElement element)
            return JsonNode.Parse(element.GetRawText());

        if (value is JsonValue jsonValue)
            return jsonValue.DeepClone();

        if (value is string text && TryParseJson(text, out var parsed))
            return parsed;

        return JsonSerializer.SerializeToNode(value, UToolJson.Options);
    }

    private static bool TryParseJson(string text, out JsonNode? node)
    {
        node = null;
        var trimmed = text.AsSpan().TrimStart();
        if (trimmed.Length == 0 || (trimmed[0] != '{' && trimmed[0] != '['))
            return false;

        try
        {
            node = JsonNode.Parse(text);
            return node is not null;
        }
        catch (JsonException)
        {
            return false;
        }
    }
}
