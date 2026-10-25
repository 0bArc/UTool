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
          } catch (const std::exception&) {
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
          } catch (const std::exception&) {
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
