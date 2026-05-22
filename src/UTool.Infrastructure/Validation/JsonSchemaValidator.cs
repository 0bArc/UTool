using System.Text.Json;
using System.Text.Json.Nodes;

namespace UTool.Infrastructure.Validation;

/// <summary>Lightweight UE4-export JSON sanity checks (not full UObject schema).</summary>
public static class JsonSchemaValidator
{
    public static IReadOnlyList<string> ValidateUeExport(string json)
    {
        var issues = new List<string>();
        JsonNode? root;
        try
        {
            root = JsonNode.Parse(json);
        }
        catch (Exception ex)
        {
            issues.Add($"invalid JSON: {ex.Message}");
            return issues;
        }

        if (root is not JsonArray arr)
        {
            issues.Add("root must be JSON array (FModel UE export convention)");
            return issues;
        }

        if (arr.Count == 0)
            issues.Add("export array is empty");

        foreach (var item in arr)
        {
            if (item is not JsonObject obj)
                continue;

            if (!obj.ContainsKey("Type") && !obj.ContainsKey("Name") && !obj.ContainsKey("Properties"))
                issues.Add("array item missing Type/Name/Properties — may not be UE export");
        }

        return issues;
    }
}
