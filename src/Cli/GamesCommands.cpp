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

}  // namespace UTool::Cli
