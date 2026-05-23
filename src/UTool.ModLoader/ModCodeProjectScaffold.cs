using UTool.Core.Models;

namespace UTool.ModLoader;

public static class ModCodeProjectScaffold
{
    public static string? EnsureProject(ModPackage mod)
    {
        if (ModCodeCompiler.TryResolveProjectPath(mod) is not null)
            return ModCodeCompiler.TryResolveProjectPath(mod);

        var codeDir = Path.Combine(mod.RootPath, ModCodeCompiler.DefaultCodeDirName);
        if (!Directory.Exists(codeDir))
            return null;

        var csFiles = Directory.GetFiles(codeDir, "*.cs", SearchOption.TopDirectoryOnly);
        if (csFiles.Length == 0)
            return null;

        var sdk = ResolveSdkProjectPath(mod.RootPath)
            ?? throw new InvalidOperationException(
                "Could not find UTool.Sdk.csproj. Place mod under a workspace with src/UTool.Sdk, or add code/*.csproj manually.");

        var assemblyName = SanitizeAssemblyName(mod.Manifest.Id);
        var csprojPath = Path.Combine(codeDir, $"{assemblyName}.csproj");
        if (File.Exists(csprojPath))
            return Path.GetFullPath(csprojPath);

        var relativeSdk = Path.GetRelativePath(codeDir, sdk).Replace('\\', '/');
        var content = $"""
            <Project Sdk="Microsoft.NET.Sdk">
              <PropertyGroup>
                <TargetFramework>net8.0</TargetFramework>
                <ImplicitUsings>enable</ImplicitUsings>
                <Nullable>enable</Nullable>
                <AssemblyName>{assemblyName}</AssemblyName>
                <RootNamespace>{assemblyName}</RootNamespace>
              </PropertyGroup>
              <ItemGroup>
                <Compile Include="*.cs" />
                <ProjectReference Include="{relativeSdk}" />
              </ItemGroup>
            </Project>
            """;

        File.WriteAllText(csprojPath, content);
        return Path.GetFullPath(csprojPath);
    }

    public static string? ResolveSdkProjectPath(string startDirectory)
    {
        var dir = Path.GetFullPath(startDirectory);
        for (var i = 0; i < 12 && !string.IsNullOrEmpty(dir); i++)
        {
            var sdk = Path.Combine(dir, "src", "UTool.Sdk", "UTool.Sdk.csproj");
            if (File.Exists(sdk))
                return Path.GetFullPath(sdk);

            var parent = Directory.GetParent(dir);
            dir = parent?.FullName ?? "";
        }

        return null;
    }

    private static string SanitizeAssemblyName(string modId)
    {
        var chars = modId.Select(c => char.IsLetterOrDigit(c) ? c : '_').ToArray();
        var name = new string(chars).Trim('_');
        if (name.Length == 0)
            name = "Mod";
        if (char.IsDigit(name[0]))
            name = "Mod_" + name;
        return name;
    }
}
