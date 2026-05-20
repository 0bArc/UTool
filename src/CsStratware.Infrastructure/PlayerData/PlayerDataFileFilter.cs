namespace CsStratware.Infrastructure.PlayerData;

public static class PlayerDataFileFilter
{
    public static bool IsWritableSaveFile(string fileName)
    {
        if (string.IsNullOrWhiteSpace(fileName))
            return false;

        if (!fileName.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            return false;

        if (fileName.Contains(".backup", StringComparison.OrdinalIgnoreCase))
            return false;

        if (fileName.Contains(".auto_", StringComparison.OrdinalIgnoreCase))
            return false;

        return true;
    }

    public static bool LooksLikeProfileId(string name) =>
        name.Length >= 15 && name.All(char.IsDigit);
}
