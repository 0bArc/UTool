using CsStratware.Core.Models;

namespace CsStratware.Core.Abstractions;

public interface IModLoader
{
    Task<ModLoadResult> LoadAsync(string modsDirectory, CancellationToken cancellationToken = default);
}
