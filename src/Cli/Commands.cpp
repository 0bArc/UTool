#include "UTool/Cli/Commands.hpp"
#include "UTool/Cli/Interactive.hpp"

#include "UTool/Core/Config.hpp"
#include "UTool/Core/Env.hpp"
#include "UTool/Core/GameCheck.hpp"
#include "UTool/Core/ModSetup.hpp"
#include "UTool/Core/NewMod.hpp"
#include "UTool/Mod/Prepare.hpp"
#include "UTool/Nexus/Client.hpp"
#include "UTool/Pak/AssetPreview.hpp"
#include "UTool/Pak/Inventory.hpp"
#include "UTool/Version.hpp"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <optional>
#include <set>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

namespace UTool::Cli {
namespace {

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
    std::optional<std::string> head1 = std::nullopt) {
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

[[nodiscard]] std::string toLowerCopy(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

[[nodiscard]] bool containsInsensitive(std::string_view hay, std::string_view needleLower) {
  if (needleLower.empty())
    return true;
  return toLowerCopy(std::string(hay)).find(needleLower) != std::string::npos;
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
  if (toLowerCopy(mod.rootPath.filename().string()) == q)
    return true;
  if (toLowerCopy(mod.manifest.id) == q)
    return true;
  if (catalog) {
    if (toLowerCopy(catalog->id) == q)
      return true;
    if (toLowerCopy(std::filesystem::path(catalog->path).filename().string()) == q)
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
  const auto queryLower = toLowerCopy(std::string(query));
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

int cmdDiscover(const std::vector<std::string>& args) {
  std::filesystem::path modsDir = defaultModsDirPath();
  if (args.size() >= 2 && args[1][0] != '-')
    modsDir = args[1];
  const auto mods = Core::discoverMods(modsDir);
  if (mods.empty()) {
    std::cout << "No mods found in " << modsDir.string() << '\n';
    return 0;
  }
  for (const auto& mod : mods) {
    std::cout << mod.manifest.id << "  " << mod.manifest.name << "  " << mod.manifest.version
              << "  (" << mod.rootPath.string() << ")\n";
  }
  return 0;
}

int cmdValidate(const std::vector<std::string>& args) {
  std::filesystem::path modsDir = defaultModsDirPath();
  if (args.size() >= 2 && args[1][0] != '-')
    modsDir = args[1];
  const auto mods = Core::discoverMods(modsDir);
  int failures = 0;
  for (const auto& mod : mods) {
    const auto issues = Core::validateMod(mod);
    if (issues.empty()) {
      std::cout << "OK  " << mod.manifest.id << '\n';
      continue;
    }
    ++failures;
    std::cout << "FAIL  " << mod.manifest.id << '\n';
    for (const auto& issue : issues)
      std::cout << "  - " << issue << '\n';
  }
  return failures == 0 ? 0 : 1;
}

int cmdCheck(const std::vector<std::string>& args) {
  const auto config = Core::Config::load(std::filesystem::current_path());

  if (args.size() < 2) {
    const auto reports = Core::checkAllConfiguredGames(config);
    if (reports.size() == 1 && reports[0].lines.size() == 1 &&
        reports[0].lines[0].status == Core::CheckLine::Status::Fail) {
      std::cerr << reports[0].lines[0].message << '\n';
      std::cerr << "Usage: utool check <game-id-or-path>\n";
      return 1;
    }

    int worst = 0;
    for (size_t i = 0; i < reports.size(); ++i) {
      if (i > 0)
        std::cout << '\n';
      Core::printGameCheckReport(reports[i], std::cout);
      worst = std::max(worst, Core::exitCodeForSupport(reports[i].level));
    }
    return worst;
  }

  const std::string target = joinArgs(args, 1);
  const auto report = Core::checkGameTarget(config, target);
  Core::printGameCheckReport(report, std::cout);
  return Core::exitCodeForSupport(report.level);
}

int cmdAutoSetup(const std::vector<std::string>& args) {
  if (args.size() < 3 || args[1] != "setup") {
    std::cerr << "Usage: utool auto setup <game-id-or-path> [--id mod.id] [--name \"Mod Name\"] "
                 "[--check]\n";
    return 1;
  }

  Core::ModSetupOptions options;
  bool showCheck = false;
  std::string target;

  for (size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--check") {
      showCheck = true;
      continue;
    }
    if (args[i] == "--id" && i + 1 < args.size()) {
      options.modId = args[++i];
      continue;
    }
    if (args[i] == "--name" && i + 1 < args.size()) {
      options.modName = args[++i];
      continue;
    }
    if (target.empty())
      target = args[i];
    else
      target += " " + args[i];
  }

  if (target.empty()) {
    std::cerr << "Usage: utool auto setup <game-id-or-path> [--id mod.id] [--name \"Mod Name\"] "
                 "[--check]\n";
    return 1;
  }

  const auto config = Core::Config::load(std::filesystem::current_path());

  if (showCheck) {
    const auto report = Core::checkGameTarget(config, target);
    Core::printGameCheckReport(report, std::cerr);
    std::cerr << '\n';
  }

  const auto setup = Core::generateModSetup(config, target, options);
  for (const auto& note : setup.notes)
    std::cerr << "note: " << note << '\n';

  if (!setup.viable || setup.modLua.empty()) {
    std::cerr << "auto setup failed: insufficient game support or no paks detected\n";
    return 1;
  }

  std::cout << setup.modLua;
  if (!setup.modLua.empty() && setup.modLua.back() != '\n')
    std::cout << '\n';
  return 0;
}

nlohmann::json entryToJson(const Pak::PakFileEntry& entry) {
  return nlohmann::json{
      {"sourcePak", entry.sourcePak},
      {"virtualPath", entry.virtualPath},
      {"size", entry.size},
      {"offset", entry.offset},
      {"extension", entry.extension},
  };
}

nlohmann::json inventoryMeta(const Pak::PakInventory& inventory, std::size_t total,
                             std::size_t offset) {
  return nlohmann::json{
      {"sourceLabel", inventory.sourceLabel},
      {"fromCache", inventory.fromCache},
      {"total", total},
      {"offset", offset},
  };
}

[[nodiscard]] std::optional<std::string> optionalGameIdFromSource(std::string_view source) {
  if (source == "@paks" || source == "@data")
    return std::nullopt;
  if (Core::looksLikeFilesystemTarget(source))
    return std::nullopt;
  return std::string(source);
}

int printAmbiguousError(const std::vector<Pak::PakFileEntry>& matches, bool asJson) {
  if (asJson) {
    nlohmann::json doc;
    doc["error"] = "ambiguous_virtual_path";
    doc["candidates"] = nlohmann::json::array();
    for (const auto& m : matches)
      doc["candidates"].push_back(entryToJson(m));
    std::cout << doc.dump(2) << '\n';
  } else {
    std::cerr << "Ambiguous virtual path. Specify --pak:\n";
    for (const auto& m : matches)
      std::cerr << "  " << m.sourcePak << "  " << m.virtualPath << '\n';
  }
  return 1;
}

std::filesystem::path makePreviewTempDir() {
  return std::filesystem::temp_directory_path() /
         ("utool-preview-" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

int cmdPakList(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    std::cerr << "Usage: utool pak list <source> [--ext json] [--offset N] [--limit N] [--json]\n";
    return 1;
  }

  try {
    const auto config = Core::Config::load(std::filesystem::current_path());
    const std::string source = args[2];
    const auto gameId = optionalGameIdFromSource(source);
    const auto ctx = Pak::makeResolveContext(config, gameId);
    const auto inventory = Pak::resolvePakSource(source, ctx);

    std::vector<Pak::PakFileEntry> entries = inventory.entries;
    if (const auto ext = getArg(args, "--ext"))
      entries = Pak::filterInventoryByExtension(inventory, *ext);

    const std::size_t total = entries.size();
    std::size_t offset = 0;
    std::size_t limit = entries.size();
    if (const auto off = getArg(args, "--offset"))
      offset = static_cast<std::size_t>(std::stoull(*off));
    if (const auto lim = getArg(args, "--limit"))
      limit = static_cast<std::size_t>(std::stoull(*lim));

    if (offset > entries.size())
      offset = entries.size();
    const std::size_t end = std::min(entries.size(), offset + limit);
    const bool asJson = hasFlag(args, "--json");

    if (asJson) {
      nlohmann::json doc = inventoryMeta(inventory, total, offset);
      doc["entries"] = nlohmann::json::array();
      for (std::size_t i = offset; i < end; ++i)
        doc["entries"].push_back(entryToJson(entries[i]));
      std::cout << doc.dump(2) << '\n';
    } else {
      std::cout << "Source: " << inventory.sourceLabel
                << (inventory.fromCache ? " (cached)" : " (indexed)") << '\n';
      for (std::size_t i = offset; i < end; ++i) {
        const auto& e = entries[i];
        std::cout << e.sourcePak << '\t' << e.virtualPath << '\t' << e.size << '\n';
      }
      std::cout << "Showing " << (end - offset) << " of " << total << " entries\n";
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdPakSearch(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    std::cerr << "Usage: utool pak search <query> --from <source> [--ext uasset] [--inside] [--json]\n";
    return 1;
  }
  const auto from = getArg(args, "--from");
  if (!from) {
    std::cerr << "Usage: utool pak search <query> --from <source>\n";
    return 1;
  }

  try {
    const auto config = Core::Config::load(std::filesystem::current_path());
    const std::string query = args[2];
    const auto gameId = optionalGameIdFromSource(*from);
    const auto ctx = Pak::makeResolveContext(config, gameId);
    const auto inventory = Pak::resolvePakSource(*from, ctx);

    std::optional<std::string_view> ext;
    if (const auto extArg = getArg(args, "--ext"))
      ext = *extArg;
    const bool inside = hasFlag(args, "--inside");
    const auto pathMatches = Pak::searchInventory(inventory, query, ext);
    std::vector<Pak::PakFileEntry> matches = pathMatches;
    if (inside) {
      const auto insideMatches = Pak::searchInventoryInside(inventory, query, ctx.unrealPak);
      std::set<std::string> seen;
      matches.clear();
      for (const auto& e : pathMatches) {
        const auto key = e.sourcePak + '\0' + e.virtualPath;
        if (seen.insert(key).second)
          matches.push_back(e);
      }
      for (const auto& e : insideMatches) {
        const auto key = e.sourcePak + '\0' + e.virtualPath;
        if (seen.insert(key).second)
          matches.push_back(e);
      }
    }
    const bool asJson = hasFlag(args, "--json");

    if (asJson) {
      nlohmann::json doc = inventoryMeta(inventory, matches.size(), 0);
      doc["query"] = query;
      doc["inside"] = inside;
      doc["entries"] = nlohmann::json::array();
      for (const auto& e : matches)
        doc["entries"].push_back(entryToJson(e));
      std::cout << doc.dump(2) << '\n';
    } else {
      for (const auto& e : matches)
        std::cout << e.sourcePak << '\t' << e.virtualPath << '\t' << e.size << '\n';
      std::cout << matches.size() << " match(es)\n";
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdPakPreview(const std::vector<std::string>& args) {
  const auto from = getArg(args, "--from");
  if (args.size() < 3 || !from) {
    std::cerr << "Usage: utool pak preview <virtual-path> --from <source> [--pak chunk.pak] [--json]\n";
    return 1;
  }

  try {
    const auto config = Core::Config::load(std::filesystem::current_path());
    const std::string virtualPath = args[2];
    const auto gameId = optionalGameIdFromSource(*from);
    const auto ctx = Pak::makeResolveContext(config, gameId);
    const auto inventory = Pak::resolvePakSource(*from, ctx);

    const auto pakFilter = getArg(args, "--pak");
    const auto matches = Pak::findEntries(
        inventory, virtualPath, pakFilter ? std::optional<std::string_view>{*pakFilter} : std::nullopt);
    if (matches.empty()) {
      std::cerr << "Virtual path not found: " << virtualPath << '\n';
      return 1;
    }
    if (matches.size() > 1)
      return printAmbiguousError(matches, hasFlag(args, "--json"));

    const auto& entry = matches.front();
    const auto pakPath = Pak::findPakPathOnDisk(inventory, entry.sourcePak);
    if (!pakPath) {
      std::cerr << "Cannot resolve pak file for " << entry.sourcePak << '\n';
      return 1;
    }

    const auto tempDir = makePreviewTempDir();
    const auto preview = Pak::previewEntry(entry, *pakPath, ctx.unrealPak, tempDir);
    const bool asJson = hasFlag(args, "--json");

    if (asJson) {
      nlohmann::json doc;
      doc["kind"] = Pak::previewKindName(preview.kind);
      doc["sourcePak"] = preview.sourcePak;
      doc["virtualPath"] = preview.virtualPath;
      doc["payload"] = preview.payload;
      std::cout << doc.dump(2) << '\n';
    } else {
      std::cout << preview.sourcePak << " :: " << preview.virtualPath << " ["
                << Pak::previewKindName(preview.kind) << "]\n";
      std::cout << preview.payload.dump(2) << '\n';
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdPakOpen(const std::vector<std::string>& args) {
  const auto from = getArg(args, "--from");
  if (args.size() < 3 || !from) {
    std::cerr << "Usage: utool pak open <virtual-path> --from <source> [--pak chunk.pak] [--json]\n";
    return 1;
  }

  try {
    const auto config = Core::Config::load(std::filesystem::current_path());
    const std::string virtualPath = args[2];
    const auto gameId = optionalGameIdFromSource(*from);
    const auto ctx = Pak::makeResolveContext(config, gameId);
    const auto inventory = Pak::resolvePakSource(*from, ctx);

    const auto pakFilter = getArg(args, "--pak");
    const auto matches = Pak::findEntries(
        inventory, virtualPath, pakFilter ? std::optional<std::string_view>{*pakFilter} : std::nullopt);
    if (matches.empty()) {
      std::cerr << "Virtual path not found: " << virtualPath << '\n';
      return 1;
    }
    if (matches.size() > 1)
      return printAmbiguousError(matches, hasFlag(args, "--json"));

    const auto& entry = matches.front();
    const auto pakPath = Pak::findPakPathOnDisk(inventory, entry.sourcePak);
    if (!pakPath) {
      std::cerr << "Cannot resolve pak file for " << entry.sourcePak << '\n';
      return 1;
    }

    const auto preview = Pak::openEntry(entry, *pakPath, ctx.unrealPak);
    const bool asJson = hasFlag(args, "--json");

    if (asJson) {
      nlohmann::json doc;
      doc["kind"] = Pak::previewKindName(preview.kind);
      doc["sourcePak"] = preview.sourcePak;
      doc["virtualPath"] = preview.virtualPath;
      doc["payload"] = preview.payload;
      std::cout << doc.dump(2) << '\n';
    } else {
      std::cout << preview.sourcePak << " :: " << preview.virtualPath << " ["
                << Pak::previewKindName(preview.kind) << "]\n";
      if (preview.payload.contains("extractedPath"))
        std::cout << "Extracted: " << preview.payload["extractedPath"].get<std::string>() << '\n';
      std::cout << preview.payload.dump(2) << '\n';
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdPakExtract(const std::vector<std::string>& args) {
  const auto from = getArg(args, "--from");
  const auto outDir = getArg(args, "-o");
  if (args.size() < 3 || !from || !outDir) {
    std::cerr << "Usage: utool pak extract <virtual-path> --from <source> [--pak chunk.pak] -o <dir>\n";
    return 1;
  }

  try {
    const auto config = Core::Config::load(std::filesystem::current_path());
    const std::string virtualPath = args[2];
    const auto gameId = optionalGameIdFromSource(*from);
    const auto ctx = Pak::makeResolveContext(config, gameId);
    const auto inventory = Pak::resolvePakSource(*from, ctx);

    const auto pakFilter = getArg(args, "--pak");
    const auto matches = Pak::findEntries(
        inventory, virtualPath, pakFilter ? std::optional<std::string_view>{*pakFilter} : std::nullopt);
    if (matches.empty()) {
      std::cerr << "Virtual path not found: " << virtualPath << '\n';
      return 1;
    }
    if (matches.size() > 1)
      return printAmbiguousError(matches, false);

    const auto& entry = matches.front();
    const auto pakPath = Pak::findPakPathOnDisk(inventory, entry.sourcePak);
    if (!pakPath) {
      std::cerr << "Cannot resolve pak file for " << entry.sourcePak << '\n';
      return 1;
    }

    const auto extracted =
        Pak::extractEntryToDir(entry, *pakPath, ctx.unrealPak, std::filesystem::path(*outDir));
    std::cout << "Extracted: " << extracted.string() << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdPakSnippet(const std::vector<std::string>& args) {
  const auto from = getArg(args, "--from");
  if (args.size() < 3 || !from) {
    std::cerr << "Usage: utool pak snippet <virtual-path> --from <source> [--pak ...] [--row R] [--field F] [--json]\n";
    return 1;
  }

  try {
    const auto config = Core::Config::load(std::filesystem::current_path());
    const std::string virtualPath = args[2];
    const auto gameId = optionalGameIdFromSource(*from);
    const auto ctx = Pak::makeResolveContext(config, gameId);
    const auto inventory = Pak::resolvePakSource(*from, ctx);

    const auto pakFilter = getArg(args, "--pak");
    const auto matches = Pak::findEntries(
        inventory, virtualPath, pakFilter ? std::optional<std::string_view>{*pakFilter} : std::nullopt);
    if (matches.empty()) {
      std::cerr << "Virtual path not found: " << virtualPath << '\n';
      return 1;
    }
    if (matches.size() > 1)
      return printAmbiguousError(matches, hasFlag(args, "--json"));

    const auto& entry = matches.front();
    const auto pakPath = Pak::findPakPathOnDisk(inventory, entry.sourcePak);
    if (!pakPath) {
      std::cerr << "Cannot resolve pak file for " << entry.sourcePak << '\n';
      return 1;
    }

    const auto tempDir = makePreviewTempDir();
    const auto preview = Pak::previewEntry(entry, *pakPath, ctx.unrealPak, tempDir);

    Pak::SnippetRequest snippetReq;
    snippetReq.row = getArg(args, "--row");
    snippetReq.field = getArg(args, "--field");
    if (const auto val = getArg(args, "--value")) {
      try {
        snippetReq.value = nlohmann::json::parse(*val);
      } catch (...) {
        snippetReq.value = *val;
      }
    }

    const auto snippet = Pak::generateSnippet(preview, snippetReq);
    const bool asJson = hasFlag(args, "--json");

    if (asJson) {
      nlohmann::json doc;
      doc["snippet"] = snippet;
      doc["kind"] = Pak::previewKindName(preview.kind);
      doc["virtualPath"] = preview.virtualPath;
      doc["sourcePak"] = preview.sourcePak;
      std::cout << doc.dump(2) << '\n';
    } else {
      std::cout << snippet << '\n';
    }

    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdGames(const std::vector<std::string>& args) {
  const auto config = Core::Config::load(std::filesystem::current_path());
  const bool asJson = hasFlag(args, "--json");

  if (args.size() >= 2 && args[1] == "probe") {
    std::string target;
    for (size_t i = 2; i < args.size(); ++i) {
      if (args[i] == "--json")
        continue;
      if (target.empty())
        target = args[i];
      else
        target += " " + args[i];
    }
    if (target.empty()) {
      std::cerr << "Usage: utool games probe <install-path> [--json]\n";
      return 1;
    }

    const auto probe = Core::probeInstallPath(target);
    const auto matched = Core::findConfigGameIdForPaths(config, probe);

    std::string source = target;
    if (matched)
      source = *matched;
    else if (probe.paksDir)
      source = probe.paksDir->string();

    const bool ready = probe.singlePakFile || probe.pakCount > 0;

    if (asJson) {
      nlohmann::json doc;
      doc["inputPath"] = probe.inputPath.string();
      if (probe.paksDir)
        doc["paksDir"] = probe.paksDir->string();
      doc["pakCount"] = probe.pakCount;
      if (probe.dataPak)
        doc["dataPak"] = probe.dataPak->string();
      if (matched)
        doc["matchedGameId"] = *matched;
      doc["source"] = source;
      doc["ready"] = ready;
      std::cout << doc.dump(2) << '\n';
      return 0;
    }

    if (!ready) {
      std::cerr << "No .pak files found under: " << target << '\n';
      return 1;
    }
    if (probe.paksDir)
      std::cout << "paksDir: " << *probe.paksDir << " (" << probe.pakCount << " files)\n";
    if (matched)
      std::cout << "matched utool.json game: " << *matched << '\n';
    std::cout << "pak source: " << source << '\n';
    return 0;
  }

  if (args.size() >= 2 && args[1] != "list") {
    std::cerr << "Usage: utool games list [--json] | utool games probe <install-path> [--json]\n";
    return 1;
  }

  nlohmann::json doc;
  doc["configFound"] = !config.configDirectory.empty();
  if (!config.configDirectory.empty())
    doc["configDir"] = config.configDirectory.string();
  doc["games"] = nlohmann::json::array();

  for (const auto& [id, settings] : config.games) {
    nlohmann::json g;
    g["id"] = id;
    if (settings.paksDir) {
      try {
        const auto dir = config.resolvePath(*settings.paksDir);
        g["paksDir"] = dir.string();
        const auto probe = Core::probeInstallPath(dir);
        g["pakCount"] = probe.pakCount;
      } catch (const std::exception& ex) {
        g["paksDir"] = *settings.paksDir;
        g["error"] = ex.what();
      }
    }
    if (settings.dataPak)
      g["dataPak"] = *settings.dataPak;
    doc["games"].push_back(g);
  }

  if (asJson) {
    std::cout << doc.dump(2) << '\n';
    return 0;
  }

  if (!doc["configFound"].get<bool>()) {
    std::cerr << "No utool.json found (walk up from cwd or set UTOOL_CONFIG_DIR).\n";
    return 1;
  }
  std::cout << "Config: " << config.configDirectory.string() << '\n';
  for (const auto& g : doc["games"]) {
    std::cout << "  " << g["id"].get<std::string>();
    if (g.contains("paksDir"))
      std::cout << "  " << g["paksDir"].get<std::string>();
    if (g.contains("pakCount"))
      std::cout << "  (" << g["pakCount"].get<std::size_t>() << " paks)";
    std::cout << '\n';
  }
  return 0;
}

[[nodiscard]] bool matchGlob(std::string_view pattern, std::string_view name) {
  const auto star = pattern.find('*');
  if (star == std::string_view::npos)
    return pattern == name;
  const auto prefix = pattern.substr(0, star);
  const auto suffix = pattern.substr(star + 1);
  if (name.size() < prefix.size() + suffix.size())
    return false;
  if (name.substr(0, prefix.size()) != prefix)
    return false;
  return name.substr(name.size() - suffix.size()) == suffix;
}

[[nodiscard]] std::optional<std::filesystem::path> pickZipForEntry(
    const std::vector<std::filesystem::path>& zips,
    std::string_view pattern,
    std::string_view week) {
  std::optional<std::filesystem::path> fallback;
  const std::string weekToken = std::string("Week-") + std::string(week);
  for (const auto& z : zips) {
    const auto name = z.filename().string();
    if (!matchGlob(pattern, name))
      continue;
    if (name.find(weekToken) != std::string::npos)
      return z;
    if (!fallback)
      fallback = z;
  }
  return fallback;
}

[[nodiscard]] std::vector<std::filesystem::path> collectBuiltZips(
    const std::filesystem::path& modRoot) {
  std::vector<std::filesystem::path> zips;
  const auto dist = modRoot / "dist";
  if (!std::filesystem::is_directory(dist))
    return zips;
  for (const auto& entry : std::filesystem::directory_iterator(dist)) {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() == ".zip")
      zips.push_back(entry.path());
  }
  return zips;
}

int cmdPakStudio(const std::vector<std::string>&) {
  std::cerr << "Pak Studio has been archived (see archive/PakStudio).\n"
            << "Use: utool pak list/search/preview/snippet\n";
  return 1;
}

[[nodiscard]] std::optional<std::string> shipApiKey(const Core::Config& config,
                                                    const Core::EnvMap& env) {
  if (!config.nexus)
    return std::nullopt;
  const std::string keyEnv =
      config.nexus->apiKeyEnv.empty() ? "NEXUSMODS_API_KEY" : config.nexus->apiKeyEnv;
  return Core::getEnv(keyEnv, env);
}

int cmdShipNexusSync(const std::vector<std::string>& args) {
  std::filesystem::path modsDir = defaultModsDirPath();
  if (args.size() >= 3 && args[2][0] != '-')
    modsDir = args[2];

  try {
    auto config = Core::Config::load(modsDir);
    if (config.configFilePath.empty())
      config = Core::Config::load(std::filesystem::current_path());
    const auto env = Core::loadDotEnv(config.configDirectory.empty()
                                          ? std::filesystem::current_path()
                                          : config.configDirectory);

    if (!config.nexus || config.nexus->files.empty()) {
      std::cerr << "utool.json has no nexus.files mapping to fill\n";
      return 1;
    }

    auto apiKey = shipApiKey(config, env);
    if (!apiKey || apiKey->empty()) {
      std::cerr << "Missing NEXUSMODS_API_KEY in .env\n";
      return 1;
    }

    std::string author;
    if (const auto u = Core::getEnv("NEXUSMODS_USER", env))
      author = *u;
    if (author.empty()) {
      auto v = Nexus::validateApiKey(*apiKey);
      if (!v.ok) {
        std::cerr << "API key validate failed: " << v.message << '\n';
        return 1;
      }
      author = v.name;
      std::cout << "Nexus user: " << author << '\n';
    }

    auto settings = *config.nexus;
    auto resolved = Nexus::resolveManifestFileIds(settings, *apiKey, author);
    if (!resolved.ok) {
      std::cerr << resolved.message << '\n';
      return 1;
    }

    Nexus::writeConfigNexusIds(config, settings);
    std::cout << resolved.message << '\n';
    std::cout << "Wrote ids -> " << config.configFilePath.string() << '\n';
    for (const auto& f : settings.files)
      std::cout << "  " << f.id << " -> fileId " << f.fileId << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdShip(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Usage: utool mod ship [week] [mods-dir] [-f query] [--upload] [--dry-run] [--single]\n"
              << "       utool mod ship -f query [--upload] [--single]\n"
              << "       utool mod ship sync [mods-dir]\n";
    return 1;
  }

  if (args[1] == "nexus-sync" || args[1] == "sync")
    return cmdShipNexusSync(args);

  const auto findQuery = getFindQuery(args);
  const bool upload = hasFlag(args, "--upload");
  const bool dryRun = hasFlag(args, "--dry-run");
  const bool allowOverride =
      hasFlag(args, "--single") || hasFlag(args, "-single") || hasFlag(args, "--force");

  std::optional<std::string> week;
  std::filesystem::path modsDir = defaultModsDirPath();
  for (size_t i = 1; i < args.size(); ++i) {
    const auto& a = args[i];
    if (isValueTakingFlag(a)) {
      if (i + 1 < args.size())
        ++i;
      continue;
    }
    if (!a.empty() && a[0] == '-')
      continue;
    if (looksLikeWeekToken(a) && !week)
      week = a;
    else
      modsDir = a;
  }

  try {
    auto config = Core::Config::load(modsDir);
    if (config.configFilePath.empty())
      config = Core::Config::load(std::filesystem::current_path());
    const auto env = Core::loadDotEnv(config.configDirectory.empty()
                                          ? std::filesystem::current_path()
                                          : config.configDirectory);

    if (!week || week->empty()) {
      if (const auto envWeek = Core::getEnv("UTOOL_UPDATE_VERSION", env))
        week = *envWeek;
      else if (config.updateVersion)
        week = *config.updateVersion;
    }
    if (!week || week->empty()) {
      std::cerr << "Missing updateVersion. Pass a week, or set UTOOL_UPDATE_VERSION / utool.json.\n";
      return 1;
    }

    std::cout << "Ship week " << *week << (dryRun ? " (dry-run)" : "");
    if (findQuery)
      std::cout << " find=\"" << *findQuery << '"';
    std::cout << '\n';

    if (!dryRun) {
      if (config.configFilePath.empty()) {
        std::cerr << "No utool.json found - create one to store updateVersion\n";
        return 1;
      }
      Core::writeConfigUpdateVersion(config, *week);
      std::cout << "Wrote updateVersion=" << *week << " -> " << config.configFilePath.string()
                << '\n';
    } else {
      std::cout << "Would write updateVersion=" << *week << " to "
                << (config.configFilePath.empty() ? "(no utool.json)" : config.configFilePath.string())
                << '\n';
    }

    auto mods = Core::discoverShipMods(modsDir, config);
    if (findQuery) {
      mods = filterModsByFind(std::move(mods), config, modsDir, *findQuery);
      if (mods.empty()) {
        std::cerr << "No ship mod matched -f \"" << *findQuery << "\"\n";
        return 1;
      }
      if (mods.size() > 1) {
        printAmbiguousFind(mods, *findQuery);
        return 1;
      }
      std::cout << "Matched " << mods.front().manifest.id << " ("
                << mods.front().rootPath.generic_string() << ")\n";
    }
    if (mods.empty()) {
      std::cerr << "No ship mods found in " << modsDir.string();
      if (!config.catalog.empty())
        std::cerr << " (catalog has no ship:true entries)";
      std::cerr << '\n';
      return 1;
    }

    if (!config.catalog.empty() && !findQuery)
      std::cout << "Using catalog (" << mods.size() << " ship mod(s))\n";

    int failed = 0;
    std::vector<std::filesystem::path> allZips;

    for (auto& mod : mods) {
      const auto luaPath = mod.rootPath / Core::ModManifest::LuaManifestFileName;
      if (std::filesystem::is_regular_file(luaPath)) {
        if (dryRun) {
          std::cout << "Would rewrite " << luaPath.string() << '\n';
        } else if (Core::rewriteModLuaUpdateVersion(luaPath, *week)) {
          std::cout << "Updated " << luaPath.string() << '\n';
          mod = Core::loadModPackage(mod.rootPath);
        }
      }

      std::cout << (dryRun ? "Would build " : "Building ") << mod.manifest.id << " ...\n";
      if (dryRun)
        continue;

      const auto built =
          Mod::buildMod(mod, config, std::nullopt, std::nullopt, false, true, *week);
      if (!built.ok) {
        std::cerr << "  FAIL " << mod.manifest.id << ": " << built.message << '\n';
        ++failed;
        continue;
      }
      std::cout << "  OK " << built.outputPak.string() << '\n';
      for (auto& z : collectBuiltZips(mod.rootPath))
        allZips.push_back(std::move(z));
      if (built.outputPak.extension() == ".zip")
        allZips.push_back(built.outputPak);
    }

    if (dryRun) {
      std::cout << "Dry-run complete (" << mods.size() << " mods)\n";
      return 0;
    }

    if (failed)
      std::cerr << failed << " build(s) failed\n";

    if (!upload) {
      std::cout << "Ship builds done. Pass --upload to push Nexus files.\n";
      return failed ? 1 : 0;
    }

    if (!config.nexus || config.nexus->files.empty()) {
      std::cerr << "utool.json has no nexus.files mapping\n";
      return 1;
    }

    {
      const auto envWeek = Core::getEnv("UTOOL_UPDATE_VERSION", env);
      if (!envWeek || envWeek->empty()) {
        std::cerr << "Upload blocked: set UTOOL_UPDATE_VERSION=" << *week
                  << " in .env (new week required to upload).\n";
        return 1;
      }
      if (*envWeek != *week) {
        std::cerr << "Upload blocked: CLI week " << *week << " != .env UTOOL_UPDATE_VERSION="
                  << *envWeek << "\n"
                  << "Bump UTOOL_UPDATE_VERSION in .env, then: utool mod ship " << *envWeek
                  << " examples --upload\n";
        return 1;
      }
      if (!allowOverride && config.lastShippedWeek && *config.lastShippedWeek == *week) {
        std::cerr << "Upload blocked: week " << *week
                  << " already shipped (lastShippedWeek in utool.json).\n"
                  << "Bump UTOOL_UPDATE_VERSION in .env to a new week, or:\n"
                  << "  utool mod ship " << *week << " -f \""
                  << (findQuery ? *findQuery : "<query>") << "\" --upload --single\n";
        return 1;
      }
      if (allowOverride && config.lastShippedWeek && *config.lastShippedWeek == *week)
        std::cout << "WARNING: --single re-upload override of week " << *week << '\n';
    }

    const std::string keyEnv = config.nexus->apiKeyEnv.empty() ? "NEXUSMODS_API_KEY"
                                                               : config.nexus->apiKeyEnv;
    auto apiKey = Core::getEnv(keyEnv, env);
    if (!apiKey || apiKey->empty()) {
      std::cerr << "Missing API key (" << keyEnv << "). Set it in .env or the environment.\n";
      return 1;
    }

    {
      bool needsResolve = false;
      for (const auto& f : config.nexus->files) {
        if (f.modId && *f.modId > 0 && f.fileId <= 0)
          needsResolve = true;
      }
      if (needsResolve) {
        std::string author;
        if (const auto u = Core::getEnv("NEXUSMODS_USER", env))
          author = *u;
        if (author.empty()) {
          auto v = Nexus::validateApiKey(*apiKey);
          if (v.ok)
            author = v.name;
        }
        auto settings = *config.nexus;
        auto resolved = Nexus::resolveManifestFileIds(settings, *apiKey, author);
        if (!resolved.ok) {
          std::cerr << "nexus id resolve failed: " << resolved.message << '\n';
          std::cerr << "Run: utool mod ship sync\n";
          return 1;
        }
        Nexus::writeConfigNexusIds(config, settings);
        config.nexus = settings;
        std::cout << "Auto-resolved Nexus ids: " << resolved.message << '\n';
      }
    }

    std::set<std::string> seen;
    std::vector<std::filesystem::path> uniqueZips;
    for (const auto& z : allZips) {
      const auto key = std::filesystem::weakly_canonical(z).string();
      if (seen.insert(key).second)
        uniqueZips.push_back(z);
    }

    int uploadFail = 0;
    int uploadSkip = 0;
    int uploadOk = 0;
    for (const auto& entry : config.nexus->files) {
      if (!entry.modId || *entry.modId <= 0 || entry.fileId <= 0) {
        if (!findQuery)
          std::cerr << "Skip " << entry.id << ": missing modId/fileId\n";
        ++uploadSkip;
        continue;
      }
      std::optional<std::filesystem::path> match =
          pickZipForEntry(uniqueZips, entry.match, *week);
      if (!match) {
        if (!findQuery)
          std::cout << "Skip " << entry.id << ": no zip matched " << entry.match << '\n';
        ++uploadSkip;
        continue;
      }
      std::cout << "Uploading " << match->filename().string() << " -> mod "
                << (entry.modId ? std::to_string(*entry.modId) : "?") << " fileId " << entry.fileId
                << " ...\n";
      auto up = Nexus::uploadFromManifestFile(entry, *match, *week, *apiKey, *config.nexus);
      if (!up.ok) {
        std::cerr << "  FAIL: " << up.message << '\n';
        ++uploadFail;
      } else {
        ++uploadOk;
        std::cout << "  OK";
        if (up.versionId)
          std::cout << " version_id=" << *up.versionId;
        std::cout << '\n';
      }
    }

    if (uploadSkip && !findQuery)
      std::cout << uploadSkip << " Nexus mapping(s) skipped (no mod/file id yet)\n";

    if (uploadOk > 0 && uploadFail == 0) {
      if (findQuery) {
        std::cout << "Upload ok for -f match; lastShippedWeek unchanged (partial ship).\n";
      } else {
        Core::writeConfigLastShippedWeek(config, *week);
        std::cout << "Recorded lastShippedWeek=" << *week << " -> "
                  << config.configFilePath.string() << '\n'
                  << "Bump UTOOL_UPDATE_VERSION in .env before the next --upload.\n";
      }
    } else if (uploadOk > 0 && uploadFail > 0) {
      std::cerr << "Partial upload (" << uploadOk << " ok, " << uploadFail
                << " fail); lastShippedWeek not updated.\n";
    }

    return (failed || uploadFail) ? 1 : 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdBanner(const std::vector<std::string>& args) {
  // utool mod banner [--all | -f query | <id>] [mods-dir]
  bool all = hasFlag(args, "--all");
  std::optional<std::string> id = getFindQuery(args);
  std::filesystem::path modsDir = defaultModsDirPath();

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--all")
      continue;
    if (isValueTakingFlag(args[i])) {
      if (i + 1 < args.size())
        ++i;
      continue;
    }
    if (args[i][0] == '-')
      continue;
    if (!id && !all && args[i].find('/') == std::string::npos &&
        args[i].find('\\') == std::string::npos &&
        !std::filesystem::is_directory(args[i])) {
      id = args[i];
      continue;
    }
    modsDir = args[i];
  }

  if (!all && id) {
    // Resolve -f / fuzzy id to catalog id when possible.
    auto config = Core::Config::load(modsDir);
    if (config.configFilePath.empty())
      config = Core::Config::load(std::filesystem::current_path());
    std::string err;
    if (auto resolved = resolveModByFind(modsDir, config, *id, err)) {
      if (const auto* cat = catalogForMod(config, modsDir, *resolved))
        id = cat->id;
      else
        id = resolved->rootPath.filename().string();
    }
  }

  if (!all && !id) {
    std::cerr << "Usage: utool mod banner --all [mods-dir]\n"
              << "       utool mod banner <catalog-id> [mods-dir]\n"
              << "       utool mod banner -f <query> [mods-dir]\n";
    return 1;
  }

  try {
    const auto repo = Core::findRepoRoot(std::filesystem::current_path());
    if (!repo) {
      std::cerr << "Could not find repo root (CMakeLists.txt / assets)\n";
      return 1;
    }
    const auto script = *repo / "scripts" / "banner" / "render.py";
    if (!std::filesystem::is_regular_file(script)) {
      std::cerr << "Missing banner script: " << script.string() << '\n';
      return 1;
    }

    std::filesystem::path absMods = modsDir;
    if (!absMods.is_absolute())
      absMods = std::filesystem::absolute(modsDir);

    std::string cmd = "python \"" + script.string() + "\" --mods-dir \"" + absMods.string() + "\"";
    if (all)
      cmd += " --all";
    else
      cmd += " --id \"" + *id + "\"";

    std::cout << "Running " << cmd << '\n' << std::flush;
    const int code = std::system(cmd.c_str());
    if (code != 0) {
      std::cerr << "banner failed (exit " << code
                << "). Install Pillow: pip install -r scripts/banner/requirements.txt\n";
      return 1;
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdPakBuildMod(const std::vector<std::string>& args) {
  const auto findQuery = getFindQuery(args);
  std::optional<std::filesystem::path> modDir;
  if (findQuery) {
    // ok
  } else if (args.size() >= 3 && args[2][0] != '-') {
    modDir = args[2];
  } else {
    std::cerr << "Usage: utool mod build <mod-dir>\n"
              << "       utool mod build -f <query> [--force-extract] ...\n";
    return 1;
  }

  try {
    Core::ModPackage package;
    if (findQuery) {
      std::filesystem::path modsDir = defaultModsDirPath();
      for (size_t i = 2; i < args.size(); ++i) {
        if (isValueTakingFlag(args[i])) {
          if (i + 1 < args.size())
            ++i;
          continue;
        }
        if (!args[i].empty() && args[i][0] == '-')
          continue;
        modsDir = args[i];
        break;
      }
      auto configProbe = Core::Config::load(modsDir);
      if (configProbe.configFilePath.empty())
        configProbe = Core::Config::load(std::filesystem::current_path());
      std::string err;
      auto resolved = resolveModByFind(modsDir, configProbe, *findQuery, err);
      if (!resolved) {
        if (!err.empty())
          std::cerr << err << '\n';
        return 1;
      }
      package = std::move(*resolved);
      std::cout << "Matched " << package.manifest.id << " (" << package.rootPath.generic_string()
                << ")\n";
    } else {
      package = Core::loadModPackage(*modDir);
    }

    const auto config = Core::Config::load(package.rootPath);
    (void)Core::loadDotEnv(package.rootPath);

    std::optional<std::filesystem::path> output;
    if (const auto oFlag = getArg(args, "-o"))
      output = *oFlag;
    else if (const auto outputFlag = getArg(args, "--output"))
      output = *outputFlag;

    std::optional<std::string> mount = getArg(args, "--mount");
    std::optional<std::string> updateVersion = getArg(args, "--update-version");
    const bool compress = hasFlag(args, "-compress") || hasFlag(args, "--compress");
    const bool force = hasFlag(args, "--force-extract");

    const auto built =
        Mod::buildMod(package, config, output, mount, compress, force, updateVersion);
    if (!built.ok) {
      std::cerr << built.message << '\n';
      return 1;
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int cmdDeploy(const std::vector<std::string>& args) {
  const auto findQuery = getFindQuery(args);
  if (!findQuery && args.size() < 2) {
    std::cerr << "Usage: utool mod deploy <mod-dir>\n"
              << "       utool mod deploy -f <query>\n";
    return 1;
  }

  try {
    Core::ModPackage package;
    if (findQuery) {
      std::filesystem::path modsDir = defaultModsDirPath();
      for (size_t i = 1; i < args.size(); ++i) {
        if (isValueTakingFlag(args[i])) {
          if (i + 1 < args.size())
            ++i;
          continue;
        }
        if (!args[i].empty() && args[i][0] == '-')
          continue;
        modsDir = args[i];
        break;
      }
      auto configProbe = Core::Config::load(modsDir);
      if (configProbe.configFilePath.empty())
        configProbe = Core::Config::load(std::filesystem::current_path());
      std::string err;
      auto resolved = resolveModByFind(modsDir, configProbe, *findQuery, err);
      if (!resolved) {
        if (!err.empty())
          std::cerr << err << '\n';
        return 1;
      }
      package = std::move(*resolved);
      std::cout << "Matched " << package.manifest.id << " (" << package.rootPath.generic_string()
                << ")\n";
    } else {
      package = Core::loadModPackage(args[1]);
    }

    const auto config = Core::Config::load(package.rootPath);
    const auto deployed = Mod::deployMod(package, config);
    if (!deployed.ok) {
      std::cerr << deployed.message << '\n';
      return 1;
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

}  // namespace

int run(const std::vector<std::string>& argsIn) {
  std::vector<std::string> args = argsIn;

  // Quiet aliases -> utool mod ...
  if (!args.empty()) {
    if (args[0] == "ship") {
      args.insert(args.begin(), "mod");
      if (args.size() >= 3 && args[2] == "nexus-sync")
        args[2] = "sync";
    } else if (args[0] == "banner" || args[0] == "deploy") {
      args.insert(args.begin(), "mod");
    } else if (args[0] == "discover") {
      args[0] = "mod";
      args.insert(args.begin() + 1, "list");
    } else if (args[0] == "validate") {
      args.insert(args.begin(), "mod");
    } else if (args[0] == "new" && args.size() >= 2 && args[1] == "mod") {
      args = {"mod", "new"};
      for (size_t i = 2; i < argsIn.size(); ++i)
        args.push_back(argsIn[i]);
    } else if (args[0] == "pak" && args.size() >= 2 && args[1] == "build-mod") {
      args[0] = "mod";
      args[1] = "build";
    }
  }

  if (args.empty()) {
    if (isInteractiveTty())
      return runInteractiveSession();
    printHelp();
    return 0;
  }
  if (args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
    printHelp();
    return 0;
  }

  if (isVersionFlag(args[0])) {
    std::cout << VersionString << '\n';
    return 0;
  }

  if (args[0] == "check")
    return cmdCheck(args);
  if (args[0] == "games")
    return cmdGames(args);

  if (args[0] == "auto") {
    if (args.size() >= 2 && args[1] == "setup")
      return cmdAutoSetup(args);
    std::cerr << "utool auto: expected 'setup'\n";
    printHelp();
    return 1;
  }

  if (args[0] == "mod") {
    auto defaultModsDir = []() -> std::filesystem::path { return defaultModsDirPath(); };

    auto positionalAfter = [&](size_t start) {
      std::vector<std::string> positionals;
      for (size_t i = start; i < args.size(); ++i) {
        if (isValueTakingFlag(args[i])) {
          if (i + 1 < args.size())
            ++i;
          continue;
        }
        if (!args[i].empty() && args[i][0] == '-')
          continue;
        positionals.push_back(args[i]);
      }
      return positionals;
    };

    auto catalogIdOf = [&](const Core::Config& config, const std::filesystem::path& modsDir,
                           const Core::ModPackage& mod) -> std::string {
      if (const auto* cat = catalogForMod(config, modsDir, mod))
        return cat->id;
      return mod.rootPath.filename().string();
    };

    auto openModSession = [&](const std::filesystem::path& modDir,
                              const std::string& catalogId) -> int {
      return runModSession(modDir, catalogId);
    };

    if (args.size() < 2 || args[1] == "help" || args[1] == "--help" || args[1] == "-h") {
      printModHelp();
      return 0;
    }

    const std::string& sub = args[1];

    if (sub == "new") {
      std::filesystem::path modsDir = defaultModsDir();
      if (args.size() >= 3 && args[2][0] != '-')
        modsDir = args[2];
      return runNewModWizard(modsDir);
    }

    if (sub == "list") {
      return cmdDiscover(withHead("discover", args, 2));
    }

    if (sub == "validate") {
      return cmdValidate(withHead("validate", args, 2));
    }

    if (sub == "build") {
      return cmdPakBuildMod(withHead("pak", args, 2, std::string("build-mod")));
    }

    if (sub == "deploy") {
      return cmdDeploy(withHead("deploy", args, 2));
    }

    if (sub == "banner") {
      return cmdBanner(withHead("banner", args, 2));
    }

    if (sub == "ship") {
      auto shipArgs = withHead("ship", args, 2);
      if (shipArgs.size() >= 2 && shipArgs[1] == "sync")
        shipArgs[1] = "nexus-sync";
      return cmdShip(shipArgs);
    }

    if (sub == "link") {
      const auto findQuery = getFindQuery(args);
      const auto positionals = positionalAfter(2);

      std::filesystem::path modsDir = defaultModsDir();
      auto config = Core::Config::load(modsDir);

      std::string catalogId;
      std::int64_t nexusId = 0;

      auto takeNexusId = [&](const std::vector<std::string>& toks) -> bool {
        for (auto it = toks.rbegin(); it != toks.rend(); ++it) {
          if (!looksLikeWeekToken(*it))
            continue;
          try {
            nexusId = std::stoll(*it);
            return nexusId > 0;
          } catch (...) {
            return false;
          }
        }
        return false;
      };

      if (findQuery) {
        std::string err;
        auto resolved = resolveModByFind(modsDir, config, *findQuery, err);
        if (!resolved) {
          if (!err.empty())
            std::cerr << err << '\n';
          return 1;
        }
        catalogId = catalogIdOf(config, modsDir, *resolved);
        if (!takeNexusId(positionals)) {
          std::cerr << "Usage: utool mod link -f <query> <nexus-mod-id>\n"
                    << "  example: utool mod link -f rapidcraft 340\n";
          return 1;
        }
      } else {
        if (positionals.size() < 2) {
          std::cerr << "Usage: utool mod link -f <query> <nexus-mod-id>\n"
                    << "       utool mod link <catalog-id> <nexus-mod-id>\n"
                    << "  example: utool mod link -f rapidcraft 340\n";
          return 1;
        }
        catalogId = positionals[0];
        if (!takeNexusId(positionals)) {
          std::cerr << "Bad Nexus mod id (need a number, e.g. 340)\n";
          return 1;
        }
        std::string err;
        if (auto resolved = resolveModByFind(modsDir, config, catalogId, err))
          catalogId = catalogIdOf(config, modsDir, *resolved);
      }

      const auto linked = Core::linkModToNexus(config, catalogId, nexusId);
      if (!linked.ok) {
        std::cerr << linked.message << '\n';
        return 1;
      }
      std::cout << linked.message << '\n';
      return 0;
    }

    if (sub == "open" || getFindQuery(args) || sub.find('/') != std::string::npos ||
        sub.find('\\') != std::string::npos || std::filesystem::is_directory(sub) ||
        (sub[0] != '-' && sub != "help")) {
      // open / bare id / -f  -> mod CLI session
      const auto findQuery = getFindQuery(args);
      std::filesystem::path modsDir = defaultModsDir();
      std::filesystem::path modDir;
      std::string catalogId;

      size_t idIndex = (sub == "open") ? 2 : 1;
      if (findQuery) {
        auto config = Core::Config::load(modsDir);
        std::string err;
        auto resolved = resolveModByFind(modsDir, config, *findQuery, err);
        if (!resolved) {
          if (!err.empty())
            std::cerr << err << '\n';
          return 1;
        }
        modDir = resolved->rootPath;
        catalogId = catalogIdOf(config, modsDir, *resolved);
      } else if (idIndex < args.size()) {
        const std::string idOrPath = args[idIndex];
        modDir = idOrPath;
        if (!std::filesystem::is_directory(modDir) ||
            !(std::filesystem::is_regular_file(modDir / Core::ModManifest::LuaManifestFileName) ||
              std::filesystem::is_regular_file(modDir / Core::ModManifest::ManifestFileName))) {
          modDir.clear();
          auto cfg = Core::Config::load(modsDir);
          std::string err;
          if (auto resolved = resolveModByFind(modsDir, cfg, idOrPath, err)) {
            modDir = resolved->rootPath;
            catalogId = catalogIdOf(cfg, modsDir, *resolved);
          } else if (!err.empty()) {
            std::cerr << err << '\n';
            return 1;
          }
        }
        if (modDir.empty() || !std::filesystem::is_directory(modDir)) {
          std::cerr << "Mod not found: " << idOrPath << '\n';
          std::cerr << "Try: utool mod open -f <query>   or   utool mod help\n";
          return 1;
        }
        if (catalogId.empty()) {
          auto cfg = Core::Config::load(modsDir);
          try {
            const auto pkg = Core::loadModPackage(modDir);
            catalogId = catalogIdOf(cfg, modsDir, pkg);
          } catch (...) {
            catalogId = modDir.filename().string();
          }
        }
      } else {
        std::cerr << "Usage: utool mod open -f <query>\n"
                  << "       utool mod open <catalog-id|mod-dir>\n";
        return 1;
      }

      return openModSession(modDir, catalogId);
    }

    std::cerr << "Unknown mod subcommand: " << sub << '\n';
    printModHelp();
    return 1;
  }

  if (args[0] == "pak") {
    if (args.size() < 2 || args[1] == "help" || args[1] == "--help" || args[1] == "-h") {
      printPakHelp();
      return 0;
    }
    if (args[1] == "list")
      return cmdPakList(args);
    if (args[1] == "search")
      return cmdPakSearch(args);
    if (args[1] == "preview")
      return cmdPakPreview(args);
    if (args[1] == "open")
      return cmdPakOpen(args);
    if (args[1] == "extract")
      return cmdPakExtract(args);
    if (args[1] == "snippet")
      return cmdPakSnippet(args);
    if (args[1] == "studio")
      return cmdPakStudio(args);
    if (args[1] == "build-mod") {
      // quiet alias path if someone bypasses rewrite
      return cmdPakBuildMod(args);
    }
    std::cerr << "utool pak: unknown subcommand\n";
    printPakHelp();
    return 1;
  }

  std::cerr << "Unknown command: " << args[0] << '\n';
  printHelp();
  return 1;
}

}  // namespace UTool::Cli
