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
      } catch (const std::exception&) {
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

int cmdPakStudio(const std::vector<std::string>&) {
  std::cerr << "Pak Studio has been archived (see archive/PakStudio).\n"
            << "Use: utool pak list/search/preview/snippet\n";
  return 1;
}

}  // namespace UTool::Cli
