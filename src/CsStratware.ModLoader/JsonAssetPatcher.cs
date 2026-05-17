using CsStratware.Sdk;

namespace CsStratware.ModLoader;

public static class JsonAssetPatcher
{
    public static string Apply(string json, IReadOnlyList<PatchOperation> operations)
    {
        var editor = new JsonAssetEditor(json);

        foreach (var op in operations)
        {
            switch (op.Op.ToLowerInvariant())
            {
                case "replace":
                    editor.Replace(op.Path, op.Value);
                    break;
                case "add":
                    editor.Add(op.Path, op.Value);
                    break;
                case "remove":
                    editor.Remove(op.Path);
                    break;
                case "replaceall":
                    editor.ReplaceAll(PropertyNameFromPath(op.Path), op.Value);
                    break;
                default:
                    throw new NotSupportedException($"Unknown patch op: {op.Op}");
            }
        }

        return editor.ToJson();
    }

    private static string PropertyNameFromPath(string path)
    {
        var trimmed = path.Trim('/');
        return trimmed.Split('/').LastOrDefault() ?? trimmed;
    }
}
