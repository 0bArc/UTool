namespace UTool.Infrastructure.Logging;

public enum LogLevel
{
    Debug,
    Info,
    Warn,
    Error,
}

public static class UToolLog
{
    public static LogLevel MinimumLevel { get; set; } = LogLevel.Info;
    public static TextWriter? Writer { get; set; }

    public static void Debug(string message, object? data = null) => Write(LogLevel.Debug, message, data);
    public static void Info(string message, object? data = null) => Write(LogLevel.Info, message, data);
    public static void Warn(string message, object? data = null) => Write(LogLevel.Warn, message, data);
    public static void Error(string message, object? data = null) => Write(LogLevel.Error, message, data);

    public static IDisposable Timed(string operation)
    {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        Debug($"{operation} start");
        return new TimedScope(operation, sw);
    }

    private static void Write(LogLevel level, string message, object? data)
    {
        if (level < MinimumLevel)
            return;

        var writer = Writer ?? Console.Error;
        var ts = DateTime.UtcNow.ToString("O");
        var suffix = data is null ? "" : $" {data}";
        writer.WriteLine($"[{ts}] {level.ToString().ToUpperInvariant()} {message}{suffix}");
    }

    private sealed class TimedScope : IDisposable
    {
        private readonly string _operation;
        private readonly System.Diagnostics.Stopwatch _sw;

        public TimedScope(string operation, System.Diagnostics.Stopwatch sw)
        {
            _operation = operation;
            _sw = sw;
        }

        public void Dispose() => Debug($"{_operation} done", new { ms = _sw.ElapsedMilliseconds });
    }
}
