#pragma once

#include "UTool/Core/Config.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Core {

enum class NewModTemplate {
  Blank,       // empty mod.lua ready to patch
  ScaleField,  // multi-tier row field scale stub (2x/5x/10x)
};

struct NewModRequest {
  std::string displayName;
  std::string catalogId;   // folder + catalog id, e.g. rapidcraft
  std::string category;    // survival / inventory / ...
  std::string description;
  std::string author{"you"};
  std::string gameId{"Icarus"};
  bool ship = true;
  NewModTemplate tmpl = NewModTemplate::Blank;
  /// For ScaleField: one or more asset file names + numeric field to divide.
  std::vector<std::string> assetFiles;
  std::optional<std::string> scaleField;
  /// Optional Nexus mods/.../mods/<id> — wired into nexus.files[].modId.
  std::optional<std::int64_t> nexusModId;
};

struct NewModResult {
  bool ok = false;
  std::string message;
  std::filesystem::path modDir;
  std::string catalogId;
  std::string modId;
};

struct LinkModResult {
  bool ok = false;
  std::string message;
  std::string catalogId;
  std::int64_t nexusModId = 0;
  int filesUpdated = 0;
};

[[nodiscard]] std::string slugifyModId(std::string_view name);
[[nodiscard]] std::string luaModId(std::string_view gameId, std::string_view catalogId);
[[nodiscard]] std::string zipStemFromDisplayName(std::string_view displayName);

/// Create mod folder + mod.lua + media stubs + catalog (+ optional nexus file stubs).
[[nodiscard]] NewModResult createNewMod(Config& config, const std::filesystem::path& modsDir,
                                        const NewModRequest& req);

/// Point existing catalog mod's nexus.files at a Nexus mod page id (e.g. 340).
[[nodiscard]] LinkModResult linkModToNexus(Config& config, const std::string& catalogId,
                                           std::int64_t nexusModId);

/// Append catalog entry. Throws if id already exists.
void writeConfigCatalogEntry(Config& config, const CatalogEntry& entry);

/// Append nexus.files stub with fileId 0 when missing.
void writeConfigNexusFileStub(Config& config, const std::string& fileEntryId,
                              const std::string& matchGlob,
                              const std::string& displayNameTemplate,
                              std::optional<std::int64_t> nexusModId = std::nullopt);

[[nodiscard]] bool isKnownModCategory(std::string_view category);
[[nodiscard]] const std::vector<std::string>& knownModCategories();

/// Split "a.json, b.json" / whitespace into asset paths.
[[nodiscard]] std::vector<std::string> splitAssetFileList(std::string_view text);

}  // namespace UTool::Core
