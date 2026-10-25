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

namespace {

struct ShipArgs {
  std::optional<std::string> findQuery;
  bool upload = false;
  bool dryRun = false;
  bool allowOverride = false;
  std::optional<std::string> week;
  std::filesystem::path modsDir;
};

struct ShipBuild {
  int failed = 0;
  std::vector<std::filesystem::path> zips;
};

ShipArgs resolveShipArgs(const std::vector<std::string>& args) {
  ShipArgs out;
  out.findQuery = getFindQuery(args);
  out.upload = hasFlag(args, "--upload");
  out.dryRun = hasFlag(args, "--dry-run");
  out.allowOverride =
      hasFlag(args, "--single") || hasFlag(args, "-single") || hasFlag(args, "--force");
  out.modsDir = defaultModsDirPath();
  for (size_t i = 1; i < args.size(); ++i) {
    const auto& a = args[i];
    if (isValueTakingFlag(a)) {
      if (i + 1 < args.size())
        ++i;
      continue;
    }
    if (!a.empty() && a[0] == '-')
      continue;
    if (looksLikeWeekToken(a) && !out.week)
      out.week = a;
    else
      out.modsDir = a;
  }
  return out;
}

bool resolveShipWeek(std::optional<std::string>& week, const Core::Config& config,
                     const Core::EnvMap& env) {
  if (!week || week->empty()) {
    if (const auto envWeek = Core::getEnv("UTOOL_UPDATE_VERSION", env))
      week = *envWeek;
    else if (config.updateVersion)
      week = *config.updateVersion;
  }
  if (!week || week->empty()) {
    std::cerr << "Missing updateVersion. Pass a week, or set UTOOL_UPDATE_VERSION / utool.json.\n";
    return false;
  }
  return true;
}

std::optional<std::vector<Core::ModPackage>> selectShipMods(
    const std::filesystem::path& modsDir, const Core::Config& config,
    const std::optional<std::string>& findQuery) {
  auto mods = Core::discoverShipMods(modsDir, config);
  if (findQuery) {
    mods = filterModsByFind(std::move(mods), config, modsDir, *findQuery);
    if (mods.empty()) {
      std::cerr << "No ship mod matched -f \"" << *findQuery << "\"\n";
      return std::nullopt;
    }
    if (mods.size() > 1) {
      printAmbiguousFind(mods, *findQuery);
      return std::nullopt;
    }
    std::cout << "Matched " << mods.front().manifest.id << " ("
              << mods.front().rootPath.generic_string() << ")\n";
  }
  if (mods.empty()) {
    std::cerr << "No ship mods found in " << modsDir.string();
    if (!config.catalog.empty())
      std::cerr << " (catalog has no ship:true entries)";
    std::cerr << '\n';
    return std::nullopt;
  }
  if (!config.catalog.empty() && !findQuery)
    std::cout << "Using catalog (" << mods.size() << " ship mod(s))\n";
  return mods;
}

ShipBuild buildShipMods(std::vector<Core::ModPackage> mods, const Core::Config& config,
                        const std::string& week, bool dryRun) {
  ShipBuild built;
  for (auto& mod : mods) {
    const auto luaPath = mod.rootPath / Core::ModManifest::LuaManifestFileName;
    if (std::filesystem::is_regular_file(luaPath)) {
      if (dryRun) {
        std::cout << "Would rewrite " << luaPath.string() << '\n';
      } else if (Core::rewriteModLuaUpdateVersion(luaPath, week)) {
        std::cout << "Updated " << luaPath.string() << '\n';
        mod = Core::loadModPackage(mod.rootPath);
      }
    }

    std::cout << (dryRun ? "Would build " : "Building ") << mod.manifest.id << " ...\n";
    if (dryRun)
      continue;

    const auto result = Mod::buildMod(mod, config, std::nullopt, std::nullopt, false, true, week);
    if (!result.ok) {
      std::cerr << "  FAIL " << mod.manifest.id << ": " << result.message << '\n';
      ++built.failed;
      continue;
    }
    std::cout << "  OK " << result.outputPak.string() << '\n';
    for (auto& z : collectBuiltZips(mod.rootPath))
      built.zips.push_back(std::move(z));
    if (result.outputPak.extension() == ".zip")
      built.zips.push_back(result.outputPak);
  }
  return built;
}

bool gateShipUpload(const Core::Config& config, const Core::EnvMap& env, const ShipArgs& ship) {
  const auto envWeek = Core::getEnv("UTOOL_UPDATE_VERSION", env);
  if (!envWeek || envWeek->empty()) {
    std::cerr << "Upload blocked: set UTOOL_UPDATE_VERSION=" << *ship.week
              << " in .env (new week required to upload).\n";
    return false;
  }
  if (*envWeek != *ship.week) {
    std::cerr << "Upload blocked: CLI week " << *ship.week << " != .env UTOOL_UPDATE_VERSION="
              << *envWeek << "\n"
              << "Bump UTOOL_UPDATE_VERSION in .env, then: utool mod ship " << *envWeek
              << " examples --upload\n";
    return false;
  }
  if (!ship.allowOverride && config.lastShippedWeek && *config.lastShippedWeek == *ship.week) {
    std::cerr << "Upload blocked: week " << *ship.week
              << " already shipped (lastShippedWeek in utool.json).\n"
              << "Bump UTOOL_UPDATE_VERSION in .env to a new week, or:\n"
              << "  utool mod ship " << *ship.week << " -f \""
              << (ship.findQuery ? *ship.findQuery : "<query>") << "\" --upload --single\n";
    return false;
  }
  if (ship.allowOverride && config.lastShippedWeek && *config.lastShippedWeek == *ship.week)
    std::cout << "WARNING: --single re-upload override of week " << *ship.week << '\n';
  return true;
}

int uploadShipZips(Core::Config& config, const Core::EnvMap& env, const ShipArgs& ship,
                   const std::vector<std::filesystem::path>& allZips) {
  const std::string keyEnv =
      config.nexus->apiKeyEnv.empty() ? "NEXUSMODS_API_KEY" : config.nexus->apiKeyEnv;
  auto apiKey = Core::getEnv(keyEnv, env);
  if (!apiKey || apiKey->empty()) {
    std::cerr << "Missing API key (" << keyEnv << "). Set it in .env or the environment.\n";
    return -1;
  }

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
      return -1;
    }
    Nexus::writeConfigNexusIds(config, settings);
    config.nexus = settings;
    std::cout << "Auto-resolved Nexus ids: " << resolved.message << '\n';
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
      if (!ship.findQuery)
        std::cerr << "Skip " << entry.id << ": missing modId/fileId\n";
      ++uploadSkip;
      continue;
    }
    std::optional<std::filesystem::path> match =
        pickZipForEntry(uniqueZips, entry.match, *ship.week);
    if (!match) {
      if (!ship.findQuery)
        std::cout << "Skip " << entry.id << ": no zip matched " << entry.match << '\n';
      ++uploadSkip;
      continue;
    }
    std::cout << "Uploading " << match->filename().string() << " -> mod "
              << (entry.modId ? std::to_string(*entry.modId) : "?") << " fileId " << entry.fileId
              << " ...\n";
    auto up = Nexus::uploadFromManifestFile(entry, *match, *ship.week, *apiKey, *config.nexus);
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

  if (uploadSkip && !ship.findQuery)
    std::cout << uploadSkip << " Nexus mapping(s) skipped (no mod/file id yet)\n";

  if (uploadOk > 0 && uploadFail == 0) {
    if (ship.findQuery) {
      std::cout << "Upload ok for -f match; lastShippedWeek unchanged (partial ship).\n";
    } else {
      Core::writeConfigLastShippedWeek(config, *ship.week);
      std::cout << "Recorded lastShippedWeek=" << *ship.week << " -> "
                << config.configFilePath.string() << '\n'
                << "Bump UTOOL_UPDATE_VERSION in .env before the next --upload.\n";
    }
  } else if (uploadOk > 0 && uploadFail > 0) {
    std::cerr << "Partial upload (" << uploadOk << " ok, " << uploadFail
              << " fail); lastShippedWeek not updated.\n";
  }
  return uploadFail;
}

}  // namespace

int cmdShip(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Usage: utool mod ship [week] [mods-dir] [-f query] [--upload] [--dry-run] [--single]\n"
              << "       utool mod ship -f query [--upload] [--single]\n"
              << "       utool mod ship sync [mods-dir]\n";
    return 1;
  }

  if (args[1] == "nexus-sync" || args[1] == "sync")
    return cmdShipNexusSync(args);

  auto ship = resolveShipArgs(args);

  try {
    auto config = Core::Config::load(ship.modsDir);
    if (config.configFilePath.empty())
      config = Core::Config::load(std::filesystem::current_path());
    const auto env = Core::loadDotEnv(config.configDirectory.empty()
                                          ? std::filesystem::current_path()
                                          : config.configDirectory);

    if (!resolveShipWeek(ship.week, config, env))
      return 1;

    std::cout << "Ship week " << *ship.week << (ship.dryRun ? " (dry-run)" : "");
    if (ship.findQuery)
      std::cout << " find=\"" << *ship.findQuery << '"';
    std::cout << '\n';

    if (!ship.dryRun) {
      if (config.configFilePath.empty()) {
        std::cerr << "No utool.json found - create one to store updateVersion\n";
        return 1;
      }
      Core::writeConfigUpdateVersion(config, *ship.week);
      std::cout << "Wrote updateVersion=" << *ship.week << " -> " << config.configFilePath.string()
                << '\n';
    } else {
      std::cout << "Would write updateVersion=" << *ship.week << " to "
                << (config.configFilePath.empty() ? "(no utool.json)"
                                                  : config.configFilePath.string())
                << '\n';
    }

    auto mods = selectShipMods(ship.modsDir, config, ship.findQuery);
    if (!mods)
      return 1;
    const auto modCount = mods->size();

    const auto built = buildShipMods(std::move(*mods), config, *ship.week, ship.dryRun);
    if (ship.dryRun) {
      std::cout << "Dry-run complete (" << modCount << " mods)\n";
      return 0;
    }

    if (built.failed)
      std::cerr << built.failed << " build(s) failed\n";

    if (!ship.upload) {
      std::cout << "Ship builds done. Pass --upload to push Nexus files.\n";
      return built.failed ? 1 : 0;
    }

    if (!config.nexus || config.nexus->files.empty()) {
      std::cerr << "utool.json has no nexus.files mapping\n";
      return 1;
    }

    if (!gateShipUpload(config, env, ship))
      return 1;

    const int uploadFail = uploadShipZips(config, env, ship, built.zips);
    if (uploadFail < 0)
      return 1;
    return (built.failed || uploadFail) ? 1 : 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

}  // namespace UTool::Cli
