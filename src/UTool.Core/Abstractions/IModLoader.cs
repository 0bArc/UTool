using UTool.Core.Models;

namespace UTool.Core.Abstractions;

public interface IModLoader
{
    Task<ModLoadResult> LoadAsync(string modsDirectory, CancellationToken cancellationToken = default);
}
