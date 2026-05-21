namespace CsStratware.Pak;



/// <summary>Merge multiple .pak files; optional UE JSON row union for colliding assets.</summary>

public static class PakMerger

{

    public static PakBuildResult Merge(

        IReadOnlyList<string> pakPaths,

        string outputPakPath,

        PakMergeOptions? options = null)

    {

        options ??= new PakMergeOptions();

        var result = PakMergeExecutor.Run(new PakMergeRunRequest

        {

            PakPathsInOrder = pakPaths,

            OutputPakPath = outputPakPath,

            PakOpenOptions = options.PakOpenOptions,

            BuildOptions = options.BuildOptions,

            FilesDirectory = options.FilesDirectory,

            JsonMerge = options.JsonMerge,

            ClearExtractedDirectory = options.ClearExtractedDirectory,

            PreferUnrealPak = options.PreferUnrealPak,

            UnrealPakOptions = options.UnrealPakOptions,

            Log = options.Log,

        });



        return result.Build;

    }

}


