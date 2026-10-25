#include "Check.hpp"
#include "TempDir.hpp"
#include "UTool/Core/Config.hpp"

int runManifestTests() {
  using namespace UTool::Core;
  const int before = testFailures();

  {
    TempDir dir("utool-lua-mod");
    writeFile(dir.path / "mod.lua",
              "utool.mod {\n"
              "  id = \"sample\",\n"
              "  name = \"Sample\",\n"
              "  version = \"1.2.0\",\n"
              "  target = { gameId = \"Icarus\", minGameVersion = \"1.0.0\", maxGameVersion = \"2.0.0\" },\n"
              "  pak = { output = \"dist/sample_P.pak\" },\n"
              "  notARealField = true,\n"
              "}\n");
    const auto pkg = loadModPackage(dir.path);
    CHECK(pkg.manifest.id == "sample");
    CHECK(pkg.manifest.name == "Sample");
    CHECK(pkg.manifest.version == "1.2.0");
    CHECK(pkg.manifest.pak && pkg.manifest.pak->useUnrealPak);
    CHECK(pkg.manifest.target && pkg.manifest.target->gameId &&
          *pkg.manifest.target->gameId == "Icarus");
    CHECK(validateMod(pkg).empty());
  }

  {
    TempDir dir("utool-json-mod");
    writeFile(dir.path / "mod.json",
              R"({
                "id": "jsonmod",
                "name": "Json Mod",
                "version": "0.1.0",
                "pak": { "output": "dist/jsonmod_P.pak" },
                "target": { "minGameVersion": "2.0.0", "maxGameVersion": "1.0.0" }
              })");
    const auto pkg = loadModPackage(dir.path);
    CHECK(pkg.manifest.id == "jsonmod");
    CHECK(pkg.manifest.pak && pkg.manifest.pak->useUnrealPak);
    const auto issues = validateMod(pkg);
    bool found = false;
    for (const auto& issue : issues) {
      if (issue.find("minGameVersion") != std::string::npos)
        found = true;
    }
    CHECK(found);
  }

  {
    TempDir dir("utool-bad-script");
    writeFile(dir.path / "mod.json",
              R"({"id":"s","name":"S","version":"1","scripts":["missing.lua"]})");
    const auto pkg = loadModPackage(dir.path);
    const auto issues = validateMod(pkg);
    CHECK(!issues.empty());
  }

  return testFailures() - before;
}
