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

}  // namespace UTool::Cli
