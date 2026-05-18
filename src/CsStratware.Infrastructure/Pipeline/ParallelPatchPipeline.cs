using CsStratware.Infrastructure.Operations;

namespace CsStratware.Infrastructure.Pipeline;

public static class ParallelPatchPipeline
{
    public static IReadOnlyList<TOut> Map<TIn, TOut>(
        IReadOnlyList<TIn> items,
        Func<TIn, TOut> map,
        OperationContext? context = null,
        int? maxDegreeOfParallelism = null)
    {
        var results = new TOut[items.Count];
        var parallelOptions = new ParallelOptions
        {
            MaxDegreeOfParallelism = maxDegreeOfParallelism ?? Environment.ProcessorCount,
            CancellationToken = context?.CancellationToken ?? CancellationToken.None,
        };

        var done = 0;
        Parallel.For(0, items.Count, parallelOptions, i =>
        {
            results[i] = map(items[i]);
            var completed = Interlocked.Increment(ref done);
            context?.Report($"patch {completed}/{items.Count}", completed, items.Count);
        });

        return results;
    }

    public static async Task<IReadOnlyList<TOut>> MapAsync<TIn, TOut>(
        IReadOnlyList<TIn> items,
        Func<TIn, CancellationToken, Task<TOut>> map,
        OperationContext? context = null,
        int? maxDegreeOfParallelism = null)
    {
        var gate = new SemaphoreSlim(maxDegreeOfParallelism ?? Environment.ProcessorCount);
        var results = new TOut[items.Count];
        var tasks = new Task[items.Count];
        var done = 0;

        for (var i = 0; i < items.Count; i++)
        {
            var index = i;
            tasks[i] = Task.Run(async () =>
            {
                await gate.WaitAsync(context?.CancellationToken ?? CancellationToken.None).ConfigureAwait(false);
                try
                {
                    results[index] = await map(items[index], context?.CancellationToken ?? CancellationToken.None)
                        .ConfigureAwait(false);
                    var completed = Interlocked.Increment(ref done);
                    context?.Report($"patch {completed}/{items.Count}", completed, items.Count);
                }
                finally
                {
                    gate.Release();
                }
            }, context?.CancellationToken ?? CancellationToken.None);
        }

        await Task.WhenAll(tasks).ConfigureAwait(false);
        return results;
    }
}
