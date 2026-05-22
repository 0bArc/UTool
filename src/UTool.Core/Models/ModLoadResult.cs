namespace UTool.Core.Models;

public sealed class ModLoadResult
{
    public required IReadOnlyList<ModPackage> Mods { get; init; }
    public required IReadOnlyList<ModLoadIssue> Issues { get; init; }

    public bool Success => Issues.All(i => i.Severity != ModIssueSeverity.Error);
}

public sealed class ModLoadIssue
{
    public required ModIssueSeverity Severity { get; init; }
    public required string Message { get; init; }
    public string? ModId { get; init; }
    public string? FilePath { get; init; }
}

public enum ModIssueSeverity
{
    Info,
    Warning,
    Error,
}
