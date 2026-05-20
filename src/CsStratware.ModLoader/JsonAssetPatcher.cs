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
                case "append":
                    editor.Append(op.Path, op.Value);
                    break;
                case "merge":
                    editor.MergeInto(op.Path, op.Value);
                    break;
                case "remove":
                    editor.Remove(op.Path);
                    break;
                case "replaceall":
                    editor.ReplaceAll(PropertyNameFromPath(op.Path), op.Value);
                    break;
                case "removewhere":
                    RequireMatch(op);
                    editor.RemoveArrayElementsWhere(op.Path, op.MatchProperty!, op.MatchValue);
                    break;
                case "setwhere":
                    RequireMatch(op);
                    RequireTarget(op);
                    editor.SetOnArrayElementsWhere(
                        op.Path,
                        op.MatchProperty!,
                        op.MatchValue,
                        op.TargetPath!,
                        op.Value);
                    break;
                case "removepropertywhere":
                    RequireMatch(op);
                    RequireTarget(op);
                    editor.RemovePropertyOnArrayElementsWhere(
                        op.Path,
                        op.MatchProperty!,
                        op.MatchValue,
                        op.TargetPath!);
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

    private static void RequireMatch(PatchOperation op)
    {
        if (string.IsNullOrWhiteSpace(op.MatchProperty))
            throw new InvalidOperationException($"Patch op '{op.Op}' requires matchProperty.");
    }

    private static void RequireTarget(PatchOperation op)
    {
        if (string.IsNullOrWhiteSpace(op.TargetPath))
            throw new InvalidOperationException($"Patch op '{op.Op}' requires targetPath.");
    }
}
