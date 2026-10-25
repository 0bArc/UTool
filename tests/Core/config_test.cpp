#include "Check.hpp"
#include "TempDir.hpp"
#include "UTool/Core/Config.hpp"

#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void clearConfigDirEnv() {
#ifdef _WIN32
  SetEnvironmentVariableA("UTOOL_CONFIG_DIR", nullptr);
#else
  unsetenv("UTOOL_CONFIG_DIR");
#endif
}

}  // namespace

int runConfigTests() {
  using namespace UTool::Core;
  const int before = testFailures();

  CHECK(Config::isDataPakAlias("@data"));
  CHECK(Config::isDataPakAlias("@ICARUS-DATA"));
  CHECK(!Config::isDataPakAlias("@paks"));
  CHECK(Config::isPaksDirAlias("@paks"));
  CHECK(Config::isPaksDirAlias("@Game-Paks"));
  CHECK(!Config::isPaksDirAlias("data.pak"));

  clearConfigDirEnv();
  TempDir dir("utool-config");
  writeFile(dir.path / "utool.json", R"({
    "updateVersion": "12",
    "lastShippedWeek": "11",
    "games": {
      "Icarus": { "paksDir": "paks", "dataPak": "data.pak", "mountPoint": "../../../" }
    },
    "catalog": [
      { "id": "morexp", "path": "morexp", "ship": false, "category": "gameplay" }
    ],
    "nexus": { "gameDomain": "icarus", "files": [ { "id": "morexp", "match": "MoreXP*.zip" } ] }
  })");

  const auto cfg = Config::load(dir.path);
  CHECK(cfg.updateVersion && *cfg.updateVersion == "12");
  CHECK(cfg.lastShippedWeek && *cfg.lastShippedWeek == "11");
  CHECK(cfg.games.count("Icarus") == 1);
  CHECK(cfg.games.at("Icarus").paksDir && *cfg.games.at("Icarus").paksDir == "paks");
  CHECK(cfg.catalog.size() == 1);
  CHECK(cfg.catalog[0].id == "morexp");
  CHECK(!cfg.catalog[0].ship);
  CHECK(cfg.nexus && cfg.nexus->gameDomain && *cfg.nexus->gameDomain == "icarus");
  CHECK(cfg.nexus->files.size() == 1);
  CHECK(cfg.nexus->files[0].match == "MoreXP*.zip");

  ModManifest manifest;
  manifest.updateVersion = "9";
  const auto fromCli = resolveUpdateVersion(cfg, manifest, "3");
  CHECK(fromCli && *fromCli == "3");
  const auto fromManifest = resolveUpdateVersion(cfg, manifest);
  CHECK(fromManifest && *fromManifest == "9");
  manifest.updateVersion.reset();
  const auto fromConfig = resolveUpdateVersion(cfg, manifest);
  CHECK(fromConfig && *fromConfig == "12");

  return testFailures() - before;
}
