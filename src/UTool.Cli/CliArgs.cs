namespace UTool.Cli;

internal static class CliArgs
{
    public static bool IsHelp(string[] args) =>
        args.Length == 0 || args[0] is "-h" or "--help" or "help";

    public static string? GetArg(string[] args, string flag)
    {
        for (var i = 0; i < args.Length - 1; i++)
        {
            if (string.Equals(args[i], flag, StringComparison.OrdinalIgnoreCase))
                return args[i + 1];
        }

        return null;
    }

    public static string? GetArgAny(string[] args, params string[] flags)
    {
        foreach (var flag in flags)
        {
            var value = GetArg(args, flag);
            if (value is not null)
                return value;
        }

        return null;
    }

    public static bool HasFlag(string[] args, string flag) =>
        args.Any(a => string.Equals(a, flag, StringComparison.OrdinalIgnoreCase));

    public static bool HasAnyFlag(string[] args, params string[] flags)
    {
        foreach (var flag in flags)
        {
            if (HasFlag(args, flag))
                return true;
        }

        return false;
    }

    public static bool TryGetPositiveInt(string[] args, string flag, out int value)
    {
        value = 0;
        return int.TryParse(GetArg(args, flag), out value) && value > 0;
    }

    public static bool IsVerbose(string[] args) => HasAnyFlag(args, "--verbose", "-v");
}
