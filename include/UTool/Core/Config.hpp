#pragma once

#include <cstdint>
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

struct NexusFileEntry {
  std::string id;
  /// Game-scoped Nexus mod id (URL .../mods/213). Required for multi-mod ship.
  std::optional<std::int64_t> modId;
  std::int64_t fileId = 0;
  std::string match;
  std::optional<std::string> displayName;
  std::string category{"main"};
  bool archiveExisting = true;
  bool updateModVersion = false;
};

struct NexusSettings {
  std::optional<std::string> gameDomain;
  /// Optional default modId when a file entry omits modId (single-mod setups).
  std::optional<std::int64_t> modId;
  std::string apiKeyEnv{"NEXUSMODS_API_KEY"};
  std::optional<std::string> apiBase;
  std::vector<NexusFileEntry> files;
};

/// Workspace catalog entry: which mods exist, where, and whether ship builds them.
struct CatalogEntry {
  std::string id;
  std::string path;
  std::string category;
  bool ship = true;
  std::optional<std::string> bannerTitle;
  std::optional<std::string> bannerTagline;
};

struct Config {
  std::filesystem::path configDirectory;
  /// Path to the utool.json / csstratware.json that was loaded (empty if none).
  std::filesystem::path configFilePath;

  std::optional<std::string> unrealPak;
  std::optional<std::string> gamePaksDir;
  std::optional<std::string> dataPak;
  std::optional<std::string> defaultMountPoint;
  std::optional<std::string> playerDataDir;
  std::optional<std::string> extractedDir;
  std::optional<std::string> unrealEngineDir;
  std::optional<std::string> pakAesKey;
  /// Workspace week label for %updateversion% when a mod omits it.
  std::optional<std::string> updateVersion;
  /// Last week successfully uploaded with `utool ship ... --upload`.
  std::optional<std::string> lastShippedWeek;
  std::optional<NexusSettings> nexus;
  std::vector<CatalogEntry> catalog;

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
  bool zip = false;
  std::optional<std::string> zipTemplate;
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
  static constexpr const char* LuaManifestFileName = "mod.lua";

  std::string id;
  std::string name;
  std::string version;
  std::optional<std::string> description;
  std::optional<std::string> author;
  std::optional<std::string> updateVersion;
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
/// Mods that `utool ship` builds: catalog entries with ship=true, else full discover.
[[nodiscard]] std::vector<ModPackage> discoverShipMods(
    const std::filesystem::path& modsDir,
    const Config& config);
[[nodiscard]] std::vector<std::string> validateMod(const ModPackage& package);

[[nodiscard]] std::optional<std::filesystem::path> findRepoRoot(
    const std::filesystem::path& start = {});

/// Resolve week label: CLI override > mod manifest > utool.json.
[[nodiscard]] std::optional<std::string> resolveUpdateVersion(
    const Config& config,
    const ModManifest& manifest,
    const std::optional<std::string>& cliOverride = std::nullopt);

/// Write or replace top-level updateVersion in the loaded utool.json.
void writeConfigUpdateVersion(Config& config, const std::string& updateVersion);

/// Write lastShippedWeek after a successful Nexus upload.
void writeConfigLastShippedWeek(Config& config, const std::string& week);

/// Rewrite `updateVersion = ...` inside mod.lua (insert if missing and needed).
[[nodiscard]] bool rewriteModLuaUpdateVersion(
    const std::filesystem::path& modLuaPath,
    const std::string& updateVersion);

}  // namespace UTool::Core
