#pragma once

#include "UTool/Core/Config.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace UTool::Nexus {

struct UploadRequest {
  std::filesystem::path filePath;
  std::int64_t fileId = 0;
  std::string version;
  std::string displayName;
  std::string category{"main"};
  bool archiveExisting = true;
  bool updateModVersion = false;
  std::optional<std::string> description;
  std::optional<std::int64_t> modId;
  std::optional<std::string> changelog;
};

struct UploadResult {
  bool ok = false;
  std::string message;
  std::optional<std::string> versionId;
};

struct RemoteModFile {
  std::int64_t fileId = 0;
  std::string name;
  std::string category;
  std::string version;
};

struct ListFilesResult {
  bool ok = false;
  std::string message;
  std::vector<RemoteModFile> files;
};

struct ResolveIdsResult {
  bool ok = false;
  std::string message;
  std::int64_t modId = 0;
  int resolved = 0;
};

struct ValidateResult {
  bool ok = false;
  std::string message;
  std::string name;
  std::int64_t userId = 0;
};

/// Upload a new version of an existing Nexus file (v3 multipart + createModFileVersion).
[[nodiscard]] UploadResult uploadModFileVersion(
    const UploadRequest& request,
    const std::string& apiKey,
    const std::string& apiBase = "https://api.nexusmods.com/v3");

[[nodiscard]] UploadResult uploadFromManifestFile(
    const Core::NexusFileEntry& entry,
    const std::filesystem::path& zipPath,
    const std::string& updateVersion,
    const std::string& apiKey,
    const Core::NexusSettings& settings);

/// v3 file list for uploads: resolve game-scoped modId -> GET /v3/mods/{id}/files
/// (ids are parent mod-file ids, not classic v1/game-scoped file_id).
[[nodiscard]] ListFilesResult listModFiles(
    const std::string& gameDomain,
    std::int64_t modId,
    const std::string& apiKey);

[[nodiscard]] ValidateResult validateApiKey(const std::string& apiKey);

/// Find game-scoped mod id owned by `author` whose name fuzzy-matches `hint` (optional).
[[nodiscard]] std::optional<std::int64_t> findModIdForAuthor(
    const std::string& gameDomain,
    const std::string& author,
    const std::string& apiKey,
    const std::string& nameHint = {});

/// Find all game-scoped mod ids owned by `author` (updated.json scan).
[[nodiscard]] std::vector<std::int64_t> findModsForAuthor(
    const std::string& gameDomain,
    const std::string& author,
    const std::string& apiKey);

/// Match manifest entries to remote MAIN files by name; fill fileId (and modId).
/// Uses each entry's modId when set; otherwise scans author mods.
[[nodiscard]] ResolveIdsResult resolveManifestFileIds(
    Core::NexusSettings& settings,
    const std::string& apiKey,
    const std::string& authorHint = {});

/// Persist nexus.modId / nexus.files[].fileId back into utool.json.
void writeConfigNexusIds(Core::Config& config, const Core::NexusSettings& settings);

/// Lowercase, drop a trailing .zip, and strip a "week" plus up to four digits.
[[nodiscard]] std::string normalizeNexusFileKey(std::string_view name);

/// Match key for a manifest file entry (display name, else match glob).
[[nodiscard]] std::string nexusWantKey(const Core::NexusFileEntry& entry);

/// 100 exact main, 70 near-exact main, 0 when tier tokens (250/500/mount) disagree.
[[nodiscard]] int scoreNexusFileMatch(std::string_view wantKey, const RemoteModFile& remote);

/// One v3 mod_files[] object. Inactive files and empty names yield nullopt.
/// `version`, when set, supplies name, category, and version.
[[nodiscard]] std::optional<RemoteModFile> parseV3ModFile(const nlohmann::json& file,
                                                          const nlohmann::json* version);

}  // namespace UTool::Nexus
