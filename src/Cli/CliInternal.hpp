#pragma once

#include "UTool/Core/Config.hpp"
#include "UTool/Core/Env.hpp"
#include "UTool/Pak/Inventory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace UTool::Cli {

void printModHelp();

void printPakHelp();

void printHelp();

[[nodiscard]] std::filesystem::path defaultModsDirPath();

[[nodiscard]] std::vector<std::string> withHead(
    std::string head0,
    const std::vector<std::string>& args,
    size_t from,
    std::optional<std::string> head1 = std::nullopt);

[[nodiscard]] bool isVersionFlag(std::string_view arg);

[[nodiscard]] std::optional<std::string> getArg(
    const std::vector<std::string>& args,
    std::string_view name);

[[nodiscard]] bool hasFlag(const std::vector<std::string>& args, std::string_view name);

[[nodiscard]] bool containsInsensitive(std::string_view hay, std::string_view needleLower);

[[nodiscard]] std::optional<std::string> getFindQuery(const std::vector<std::string>& args);

[[nodiscard]] bool isValueTakingFlag(std::string_view a);

[[nodiscard]] bool packageMatchesFind(const Core::ModPackage& mod,
                                      std::string_view queryLower,
                                      const Core::CatalogEntry* catalog);

[[nodiscard]] bool isExactFindHit(const Core::ModPackage& mod,
                                  std::string_view queryLower,
                                  const Core::CatalogEntry* catalog);

[[nodiscard]] const Core::CatalogEntry* catalogForMod(const Core::Config& config,
                                                      const std::filesystem::path& modsDir,
                                                      const Core::ModPackage& mod);

[[nodiscard]] std::vector<Core::ModPackage> filterModsByFind(
    std::vector<Core::ModPackage> mods,
    const Core::Config& config,
    const std::filesystem::path& modsDir,
    std::string_view query);

void printAmbiguousFind(const std::vector<Core::ModPackage>& mods, std::string_view query);

[[nodiscard]] std::optional<Core::ModPackage> resolveModByFind(
    const std::filesystem::path& modsDir,
    const Core::Config& config,
    std::string_view query,
    std::string& errorOut);

[[nodiscard]] bool looksLikeWeekToken(std::string_view s);

[[nodiscard]] std::string joinArgs(const std::vector<std::string>& args, size_t start);

int cmdDiscover(const std::vector<std::string>& args);

int cmdValidate(const std::vector<std::string>& args);

int cmdCheck(const std::vector<std::string>& args);

int cmdAutoSetup(const std::vector<std::string>& args);

nlohmann::json entryToJson(const Pak::PakFileEntry& entry);

nlohmann::json inventoryMeta(const Pak::PakInventory& inventory, std::size_t total,
                             std::size_t offset);

[[nodiscard]] std::optional<std::string> optionalGameIdFromSource(std::string_view source);

int printAmbiguousError(const std::vector<Pak::PakFileEntry>& matches, bool asJson);

std::filesystem::path makePreviewTempDir();

int cmdPakList(const std::vector<std::string>& args);

int cmdPakSearch(const std::vector<std::string>& args);

int cmdPakPreview(const std::vector<std::string>& args);

int cmdPakOpen(const std::vector<std::string>& args);

int cmdPakExtract(const std::vector<std::string>& args);

int cmdPakSnippet(const std::vector<std::string>& args);

int cmdGames(const std::vector<std::string>& args);

[[nodiscard]] bool matchGlob(std::string_view pattern, std::string_view name);

[[nodiscard]] std::optional<std::filesystem::path> pickZipForEntry(
    const std::vector<std::filesystem::path>& zips,
    std::string_view pattern,
    std::string_view week);

[[nodiscard]] std::vector<std::filesystem::path> collectBuiltZips(
    const std::filesystem::path& modRoot);

int cmdPakStudio(const std::vector<std::string>&);

[[nodiscard]] std::optional<std::string> shipApiKey(const Core::Config& config,
                                                    const Core::EnvMap& env);

int cmdShipNexusSync(const std::vector<std::string>& args);

int cmdShip(const std::vector<std::string>& args);

int cmdBanner(const std::vector<std::string>& args);

int cmdPakBuildMod(const std::vector<std::string>& args);

int cmdDeploy(const std::vector<std::string>& args);

}  // namespace UTool::Cli
