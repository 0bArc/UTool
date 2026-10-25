#include "CliInternal.hpp"

#include "UTool/Cli/Interactive.hpp"
#include "UTool/Core/Config.hpp"
#include "UTool/Core/Env.hpp"
#include "UTool/Core/GameCheck.hpp"
#include "UTool/Core/ModSetup.hpp"
#include "UTool/Core/NewMod.hpp"
#include "UTool/Core/Process.hpp"
#include "UTool/Core/StringUtil.hpp"
#include "UTool/Mod/Prepare.hpp"
#include "UTool/Nexus/Client.hpp"
#include "UTool/Pak/AssetPreview.hpp"
#include "UTool/Pak/Inventory.hpp"
#include "UTool/Version.hpp"

#include <cctype>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

namespace UTool::Cli {
using Core::toLower;

void printModHelp() {
  std::cout
      << "utool mod - mod lifecycle\n\n"
      << "Usage:\n"
      << "  utool mod new                         create or link existing example + mod CLI\n"
      << "  utool mod list [mods-dir]              list mods (default: examples)\n"
      << "  utool mod validate [mods-dir]          validate mods\n"
      << "  utool mod open -f <query> | <id>       interactive mod CLI\n"
      << "  utool mod build -f <query> [...]       build pak/zips (--force-extract, ...)\n"
      << "  utool mod deploy -f <query>            copy built pak into game Paks/mods\n"
      << "  utool mod banner --all | -f <query>    Nexus banner/header images\n"
      << "  utool mod link -f <query> <nexusId>    wire nexus.files[].modId\n"
      << "  utool mod ship [week] [-f q] [--upload|--dry-run|--single]\n"
      << "  utool mod ship sync [mods-dir]         fill nexus fileIds from API\n\n"
      << "Tips:\n"
      << "  -f / --find matches catalog id, folder name, or mod name.\n"
      << "  Old commands (utool ship, utool deploy, utool pak build-mod, ...) still work.\n";
}

void printPakHelp() {
  std::cout
      << "utool pak - browse game pak data\n\n"
      << "Usage:\n"
      << "  utool pak list <source> [--ext json] [--offset N] [--limit N] [--json]\n"
      << "  utool pak search <query> --from <source> [--ext uasset] [--inside] [--json]\n"
      << "  utool pak preview <virtual-path> --from <source> [--pak chunk.pak] [--json]\n"
      << "  utool pak open <virtual-path> --from <source> [--pak chunk.pak] [--json]\n"
      << "  utool pak extract <virtual-path> --from <source> [--pak chunk.pak] -o <dir>\n"
      << "  utool pak snippet <virtual-path> --from <source> [--pak ...] [--row R] [--field F] [--json]\n\n"
      << "Sources: @data, @paks, game id (Icarus), or a .pak path.\n"
      << "Build a mod with: utool mod build -f <query>\n";
}

void printHelp() {
  std::cout
      << "utool " << VersionString << "\n"
      << "UE4/UE5 modding toolkit (Lua mods).\n\n"
      << "Usage:\n"
      << "  utool                         interactive menus (TTY)\n"
      << "  utool help | --version\n"
      << "  utool mod ...                 create, build, deploy, ship mods  (utool mod help)\n"
      << "  utool pak ...                 list/search/preview game assets  (utool pak help)\n"
      << "  utool games list|probe ...\n"
      << "  utool check [game-id-or-path]\n"
      << "  utool auto setup <game-id-or-path> [--id mod.id] [--name \"Mod Name\"]\n";
}

[[nodiscard]] std::filesystem::path defaultModsDirPath() {
  if (std::filesystem::is_directory("examples"))
    return std::filesystem::path("examples");
  return std::filesystem::current_path();
}

[[nodiscard]] std::vector<std::string> withHead(
    std::string head0,
    const std::vector<std::string>& args,
    size_t from,
    std::optional<std::string> head1) {
  std::vector<std::string> out;
  out.push_back(std::move(head0));
  if (head1)
    out.push_back(std::move(*head1));
  for (size_t i = from; i < args.size(); ++i)
    out.push_back(args[i]);
  return out;
}

[[nodiscard]] bool isVersionFlag(std::string_view arg) {
  return arg == "--version" || arg == "-V" || arg == "version";
}

[[nodiscard]] std::optional<std::string> getArg(
    const std::vector<std::string>& args,
    std::string_view name) {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name)
      return args[i + 1];
  }
  return std::nullopt;
}

[[nodiscard]] bool hasFlag(const std::vector<std::string>& args, std::string_view name) {
  for (const auto& a : args) {
    if (a == name)
      return true;
  }
  return false;
}

[[nodiscard]] bool containsInsensitive(std::string_view hay, std::string_view needleLower) {
  if (needleLower.empty())
    return true;
  return toLower(std::string(hay)).find(needleLower) != std::string::npos;
}

[[nodiscard]] std::optional<std::string> getFindQuery(const std::vector<std::string>& args) {
  if (auto q = getArg(args, "-f"))
    return q;
  return getArg(args, "--find");
}

[[nodiscard]] bool isValueTakingFlag(std::string_view a) {
  return a == "-f" || a == "--find" || a == "-o" || a == "--output" || a == "--mount" ||
         a == "--update-version" || a == "--pak" || a == "--from" || a == "--ext" ||
         a == "--offset" || a == "--limit" || a == "--row" || a == "--field" || a == "--id" ||
         a == "--name";
}

[[nodiscard]] bool packageMatchesFind(const Core::ModPackage& mod,
                                      std::string_view queryLower,
                                      const Core::CatalogEntry* catalog) {
  if (containsInsensitive(mod.manifest.id, queryLower) ||
      containsInsensitive(mod.manifest.name, queryLower) ||
      containsInsensitive(mod.rootPath.filename().string(), queryLower))
    return true;
  if (catalog) {
    if (containsInsensitive(catalog->id, queryLower) ||
        containsInsensitive(catalog->path, queryLower))
      return true;
  }
  return false;
}

[[nodiscard]] bool isExactFindHit(const Core::ModPackage& mod,
                                  std::string_view queryLower,
                                  const Core::CatalogEntry* catalog) {
  const auto q = std::string(queryLower);
  if (toLower(mod.rootPath.filename().string()) == q)
    return true;
  if (toLower(mod.manifest.id) == q)
    return true;
  if (catalog) {
    if (toLower(catalog->id) == q)
      return true;
    if (toLower(std::filesystem::path(catalog->path).filename().string()) == q)
      return true;
  }
  return false;
}

[[nodiscard]] const Core::CatalogEntry* catalogForMod(const Core::Config& config,
                                                      const std::filesystem::path& modsDir,
                                                      const Core::ModPackage& mod) {
  std::error_code ec;
  for (const auto& entry : config.catalog) {
    const auto entryRoot = modsDir / entry.path;
    if (std::filesystem::equivalent(entryRoot, mod.rootPath, ec))
      return &entry;
    if (!ec && entryRoot == mod.rootPath)
      return &entry;
  }
  return nullptr;
}

[[nodiscard]] std::vector<Core::ModPackage> filterModsByFind(
    std::vector<Core::ModPackage> mods,
    const Core::Config& config,
    const std::filesystem::path& modsDir,
    std::string_view query) {
  const auto queryLower = toLower(std::string(query));
  std::vector<Core::ModPackage> matched;
  matched.reserve(mods.size());

  for (auto& mod : mods) {
    const auto* cat = catalogForMod(config, modsDir, mod);
    if (packageMatchesFind(mod, queryLower, cat))
      matched.push_back(std::move(mod));
  }

  if (matched.size() <= 1)
    return matched;

  std::vector<Core::ModPackage> exact;
  for (auto& mod : matched) {
    const auto* cat = catalogForMod(config, modsDir, mod);
    if (isExactFindHit(mod, queryLower, cat))
      exact.push_back(std::move(mod));
  }
  if (exact.size() == 1)
    return exact;
  return matched;
}

void printAmbiguousFind(const std::vector<Core::ModPackage>& mods, std::string_view query) {
  std::cerr << "Ambiguous -f \"" << query << "\" (" << mods.size() << " matches):\n";
  for (const auto& m : mods)
    std::cerr << "  " << m.manifest.id << "  (" << m.rootPath.generic_string() << ")\n";
  std::cerr << "Narrow the query (e.g. folder name or catalog id).\n";
}

[[nodiscard]] std::optional<Core::ModPackage> resolveModByFind(
    const std::filesystem::path& modsDir,
    const Core::Config& config,
    std::string_view query,
    std::string& errorOut) {
  std::vector<Core::ModPackage> candidates;
  if (!config.catalog.empty()) {
    for (const auto& entry : config.catalog) {
      try {
        candidates.push_back(Core::loadModPackage(modsDir / entry.path));
      } catch (const std::exception& ex) {
        std::cerr << "catalog '" << entry.id << "': " << ex.what() << '\n';
      }
    }
  } else {
    candidates = Core::discoverMods(modsDir);
  }

  if (candidates.empty()) {
    errorOut = "No loadable mods under " + modsDir.string();
    return std::nullopt;
  }

  auto matched = filterModsByFind(std::move(candidates), config, modsDir, query);
  if (matched.empty()) {
    errorOut = "No mod matched -f \"" + std::string(query) + "\" under " + modsDir.string();
    return std::nullopt;
  }
  if (matched.size() > 1) {
    printAmbiguousFind(matched, query);
    errorOut.clear();
    return std::nullopt;
  }
  return std::move(matched.front());
}

[[nodiscard]] bool looksLikeWeekToken(std::string_view s) {
  if (s.empty())
    return false;
  for (char c : s) {
    if (c < '0' || c > '9')
      return false;
  }
  return true;
}

[[nodiscard]] std::string joinArgs(const std::vector<std::string>& args, size_t start) {
  if (start >= args.size())
    return {};
  std::string out = args[start];
  for (size_t i = start + 1; i < args.size(); ++i) {
    out += ' ';
    out += args[i];
  }
  return out;
}

}  // namespace UTool::Cli
