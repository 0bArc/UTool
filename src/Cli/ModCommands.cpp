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

    std::vector<std::wstring> procArgs = {script.wstring(), L"--mods-dir", absMods.wstring()};
    if (all)
      procArgs.push_back(L"--all");
    else {
      procArgs.push_back(L"--id");
      procArgs.push_back(std::filesystem::path(*id).wstring());
    }

    std::cout << "Running python " << script.string() << '\n' << std::flush;
    const auto ran = Core::runProcess(L"python", procArgs);
    if (!ran.launched || ran.exitCode != 0) {
      std::cerr << "banner failed (exit " << ran.exitCode
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

}  // namespace UTool::Cli
