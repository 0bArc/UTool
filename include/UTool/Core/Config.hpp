#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace UTool::Core {

struct GameSettings {
  std::optional<std::string> paksDir;
  std::optional<std::string> dataPak;
  std::optional<std::string> playerDataDir;
  std::optional<std::string> mountPoint;
  std::optional<std::string> pakAesKey;
};

struct Config {
  std::filesystem::path configDirectory;

  std::optional<std::string> unrealPak;
  std::optional<std::string> gamePaksDir;
  std::optional<std::string> dataPak;
  std::optional<std::string> defaultMountPoint;
  std::optional<std::string> playerDataDir;
  std::optional<std::string> extractedDir;
  std::optional<std::string> unrealEngineDir;
  std::optional<std::string> pakAesKey;

  std::unordered_map<std::string, GameSettings> games;

  // Legacy key aliases from older utool.json / csstratware.json
  std::optional<std::string> legacyIcarusPaksDir;
  std::optional<std::string> legacyIcarusDataPak;
  std::optional<std::string> legacyIcarusMountPoint;
  std::optional<std::string> legacyIcarusPlayerDataDir;
  std::optional<std::string> legacyDemoExtractedDir;

  [[nodiscard]] static Config load(const std::filesystem::path& startDirectory = {});

  [[nodiscard]] static bool isDataPakAlias(std::string_view token);
  [[nodiscard]] static bool isPaksDirAlias(std::string_view token);

  [[nodiscard]] std::optional<std::filesystem::path> resolvePaksDir(
      const std::optional<std::string>& gameId = std::nullopt) const;
  [[nodiscard]] std::filesystem::path resolveDataPak(
      const std::optional<std::string>& gameId = std::nullopt) const;
  [[nodiscard]] std::optional<std::string> resolveMountPoint(
      const std::optional<std::string>& gameId = std::nullopt) const;
  [[nodiscard]] std::optional<std::filesystem::path> resolveExtractedDir() const;
  [[nodiscard]] std::optional<std::filesystem::path> resolveExistingExtractedDir() const;
  [[nodiscard]] std::optional<std::filesystem::path> resolveSourcePak(
      const std::optional<std::string>& token,
      const std::optional<std::string>& gameId = std::nullopt) const;
  [[nodiscard]] std::vector<std::filesystem::path> resolveSourcePakPaths(
      const std::optional<std::string>& token,
      const std::optional<std::string>& gameId = std::nullopt) const;

  [[nodiscard]] std::filesystem::path resolvePath(const std::string& path) const;
};

struct ModPakSettings {
  std::optional<std::string> output;
  std::optional<std::string> mountPoint;
  std::optional<std::string> sourcePak;
  std::optional<std::string> curveSourcePak;
  std::optional<std::string> sourceFilter;
  bool useUnrealPak = false;
  bool keepCache = false;
};

struct Ue4Target {
  std::optional<std::string> gameId;
  std::optional<std::string> engineVersion;
  std::optional<std::string> minGameVersion;
  std::optional<std::string> maxGameVersion;
};

struct ModManifest {
  static constexpr int SchemaVersion = 1;
  static constexpr const char* ManifestFileName = "mod.json";

  std::string id;
  std::string name;
  std::string version;
  std::optional<std::string> description;
  std::optional<std::string> author;
  std::optional<Ue4Target> target;
  std::vector<std::string> contentRoots{"content"};
  std::vector<std::string> patchFiles;
  std::vector<std::string> scripts;
  std::optional<std::string> curvePatchesDir;
  std::optional<ModPakSettings> pak;
};

struct ModPackage {
  std::filesystem::path rootPath;
  ModManifest manifest;
};

[[nodiscard]] ModPackage loadModPackage(const std::filesystem::path& modDir);
[[nodiscard]] std::vector<ModPackage> discoverMods(const std::filesystem::path& modsDir);
[[nodiscard]] std::vector<std::string> validateMod(const ModPackage& package);

[[nodiscard]] std::optional<std::filesystem::path> findRepoRoot(
    const std::filesystem::path& start = {});

}  // namespace UTool::Core
