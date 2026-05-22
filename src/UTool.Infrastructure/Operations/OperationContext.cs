namespace UTool.Infrastructure.Operations;

public sealed class OperationContext
{
    public CancellationToken CancellationToken { get; init; } = CancellationToken.None;
    public IProgress<OperationProgress>? Progress { get; init; }
    public bool Verbose { get; init; }

    public IDisposable BeginStep(string name, int? total = null)
    {
        Report(name, 0, total);
        return new StepScope(this, name, total);
    }

    public void Report(string message, int current = 0, int? total = null) =>
        Progress?.Report(new OperationProgress(message, current, total));

    private sealed class StepScope : IDisposable
    {
        private readonly OperationContext _ctx;
        private readonly string _name;
        private readonly int? _total;
        private readonly long _started = Environment.TickCount64;

        public StepScope(OperationContext ctx, string name, int? total)
        {
            _ctx = ctx;
            _name = name;
            _total = total;
        }

        public void Dispose()
        {
            var elapsed = Environment.TickCount64 - _started;
            _ctx.Progress?.Report(new OperationProgress($"{_name} done ({elapsed}ms)", _total ?? 0, _total));
        }
    }
}

public readonly record struct OperationProgress(string Message, int Current, int? Total);
