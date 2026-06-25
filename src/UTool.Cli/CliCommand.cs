namespace UTool.Cli;

internal static class CliCommand
{
    public static int Unknown(string sub, Action printUsage, string scope = "command")
    {
        Console.Error.WriteLine($"Unknown {scope}: {sub}");
        printUsage();
        return 1;
    }

    public static int Missing(string usage, Action printUsage, string label = "Usage")
    {
        Console.Error.WriteLine($"{label}: {usage}");
        printUsage();
        return 1;
    }
}
