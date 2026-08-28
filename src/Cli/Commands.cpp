#include "UTool/Cli/Commands.hpp"

#include "UTool/Core/Config.hpp"
#include "UTool/Core/GameCheck.hpp"
#include "UTool/Core/ModSetup.hpp"
#include "UTool/Mod/Prepare.hpp"
#include "UTool/Pak/AssetPreview.hpp"
#include "UTool/Pak/Inventory.hpp"
#include "UTool/Version.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <optional>
#include <set>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <nlohmann/json.hpp>

namespace UTool::Cli {
namespace {

void printHelp() {
  std::cout
      << "utool " << VersionString << "\n"
      << "UE4/UE5 modding toolkit (Lua mods).\n\n"
      << "Usage:\n"
      << "  utool help\n"
      << "  utool --version\n"
      << "  utool discover <mods-dir>\n"
      << "  utool validate <mods-dir>\n"
      << "  utool check [game-id-or-path]   (quote paths with spaces)\n"
      << "  utool games list [--json]   (configured games from utool.json)\n"
      << "  utool games probe <install-path> [--json]   (find Content/Paks under a folder)\n"
      << "  utool auto setup <game-id-or-path> [--id mod.id] [--name \"Mod Name\"] [--check]\n"
      << "  utool pak build-mod <mod-dir> [-o out.pak] [--mount ...] [--force-extract] [-compress]\n"
      << "  utool deploy <mod-dir>   (copy built *_P.pak → Content/Paks/mods)\n"
      << "  utool pak list <source> [--ext json] [--offset N] [--limit N] [--json]\n"
      << "  utool pak search <query> --from <source> [--ext uasset] [--inside] [--json]\n"
      << "  utool pak preview <virtual-path> --from <source> [--pak chunk.pak] [--json]\n"
      << "  utool pak open <virtual-path> --from <source> [--pak chunk.pak] [--json]\n"
      << "  utool pak extract <virtual-path> --from <source> [--pak chunk.pak] -o <dir>\n"
      << "  utool pak snippet <virtual-path> --from <source> [--pak ...] [--row R] [--field F] [--json]\n"
      << "  utool pak studio   (open Pak Studio GUI — not the same as utool.exe alone)\n";
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
  if (args.size() < 2) {
    std::cerr << "Usage: utool discover <mods-dir>\n";
    return 1;
  }
  const auto mods = Core::discoverMods(args[1]);
  if (mods.empty()) {
    std::cout << "No mods found in " << args[1] << '\n';
    return 0;
  }
  for (const auto& mod : mods) {
    std::cout << mod.manifest.id << "  " << mod.manifest.name << "  " << mod.manifest.version
              << "  (" << mod.rootPath.string() << ")\n";
  }
  return 0;
}

int cmdValidate(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Usage: utool validate <mods-dir>\n";
    return 1;
  }
  const auto mods = Core::discoverMods(args[1]);
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

std::optional<std::filesystem::path> findPakStudioLauncher() {
  if (const char* env = std::getenv("PAK_STUDIO_CMD")) {
    if (*env)
      return std::filesystem::path(env);
  }

  const auto cwd = std::filesystem::current_path();
  if (auto repo = Core::findRepoRoot(cwd)) {
    const auto cmd = *repo / "tools" / "PakStudio" / "run-pak-studio.cmd";
    std::error_code ec;
    if (std::filesystem::is_regular_file(cmd, ec))
      return cmd;
  }

#ifdef _WIN32
  wchar_t modulePath[MAX_PATH];
  if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0) {
    if (auto repo = Core::findRepoRoot(std::filesystem::path(modulePath).parent_path())) {
      const auto cmd = *repo / "tools" / "PakStudio" / "run-pak-studio.cmd";
      std::error_code ec;
      if (std::filesystem::is_regular_file(cmd, ec))
        return cmd;
    }
  }
#endif

  return std::nullopt;
}

int cmdPakStudio(const std::vector<std::string>&) {
#ifndef _WIN32
  std::cerr << "Pak Studio GUI is Windows-only. Use: utool pak list/search/preview --json\n";
  return 1;
#else
  const auto launcher = findPakStudioLauncher();
  if (!launcher) {
    std::cerr
        << "Pak Studio not found.\n\n"
        << "utool.exe is CLI-only — it does not open a window.\n\n"
        << "Start the Next.js UI once:\n"
        << "  cd tools/PakStudio\n"
        << "  npm install\n"
        << "  npm run dev\n\n"
        << "Then run: utool pak studio\n"
        << "Or set PAK_STUDIO_CMD to run-pak-studio.cmd\n";
    return 1;
  }

  const auto dir = launcher->parent_path().wstring();
  const auto cmd = launcher->wstring();
  const HINSTANCE result = ShellExecuteW(
      nullptr,
      L"open",
      cmd.c_str(),
      nullptr,
      dir.c_str(),
      SW_SHOWNORMAL);

  if (reinterpret_cast<intptr_t>(result) <= 32) {
    std::cerr << "Failed to start Pak Studio launcher\n";
    return 1;
  }

  std::cout << "Starting Pak Studio at http://127.0.0.1:3000\n";
  return 0;
#endif
}

int cmdPakBuildMod(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    std::cerr << "Usage: utool pak build-mod <mod-dir>\n";
    return 1;
  }

  try {
    const auto package = Core::loadModPackage(args[2]);
    const auto config = Core::Config::load(package.rootPath);

    std::optional<std::filesystem::path> output;
    if (const auto oFlag = getArg(args, "-o"))
      output = *oFlag;
    else if (const auto outputFlag = getArg(args, "--output"))
      output = *outputFlag;

    std::optional<std::string> mount = getArg(args, "--mount");
    const bool compress = hasFlag(args, "-compress") || hasFlag(args, "--compress");
    const bool force = hasFlag(args, "--force-extract");

    const auto built = Mod::buildMod(package, config, output, mount, compress, force);
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
  if (args.size() < 2) {
    std::cerr << "Usage: utool deploy <mod-dir>\n";
    return 1;
  }

  try {
    const auto package = Core::loadModPackage(args[1]);
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

int run(const std::vector<std::string>& args) {
  if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
    printHelp();
    return 0;
  }

  if (isVersionFlag(args[0])) {
    std::cout << VersionString << '\n';
    return 0;
  }

  if (args[0] == "discover")
    return cmdDiscover(args);
  if (args[0] == "validate")
    return cmdValidate(args);
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

  if (args[0] == "deploy")
    return cmdDeploy(args);

  if (args[0] == "pak") {
    if (args.size() >= 2 && args[1] == "build-mod")
      return cmdPakBuildMod(args);
    if (args.size() >= 2 && args[1] == "list")
      return cmdPakList(args);
    if (args.size() >= 2 && args[1] == "search")
      return cmdPakSearch(args);
    if (args.size() >= 2 && args[1] == "preview")
      return cmdPakPreview(args);
    if (args.size() >= 2 && args[1] == "open")
      return cmdPakOpen(args);
    if (args.size() >= 2 && args[1] == "extract")
      return cmdPakExtract(args);
    if (args.size() >= 2 && args[1] == "snippet")
      return cmdPakSnippet(args);
    if (args.size() >= 2 && args[1] == "studio")
      return cmdPakStudio(args);
    std::cerr << "utool pak: unknown subcommand\n";
    printHelp();
    return 1;
  }

  std::cerr << "Unknown command: " << args[0] << '\n';
  printHelp();
  return 1;
}

}  // namespace UTool::Cli
