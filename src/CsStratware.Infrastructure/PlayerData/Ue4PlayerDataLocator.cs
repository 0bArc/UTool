namespace CsStratware.Infrastructure.PlayerData;



public static class Ue4PlayerDataLocator

{

    public const string PlayerDataEnvVar = "CSSTRATWARE_PLAYER_DATA";



    public static string Resolve(

        string? explicitPath = null,

        string? configPlayerDataDir = null,

        string? gameId = null)

    {

        if (!string.IsNullOrWhiteSpace(explicitPath))

            return Path.GetFullPath(explicitPath);



        var env = Environment.GetEnvironmentVariable(PlayerDataEnvVar);

        if (!string.IsNullOrWhiteSpace(env))

            return Path.GetFullPath(env);



        if (!string.IsNullOrWhiteSpace(configPlayerDataDir))

            return Path.GetFullPath(configPlayerDataDir);



        if (string.IsNullOrWhiteSpace(gameId))

        {

            throw new InvalidOperationException(

                $"Set {PlayerDataEnvVar}, csstratware.json playerDataDir, or pass target.gameId from mod.json.");

        }



        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

        return Path.Combine(local, gameId, "Saved", "PlayerData");

    }

}

