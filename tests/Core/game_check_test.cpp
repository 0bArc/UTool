#include "Check.hpp"
#include "UTool/Core/GameCheck.hpp"
#include "UTool/Core/ModSetup.hpp"

int runGameCheckTests() {
  using namespace UTool::Core;
  const int before = testFailures();

  CHECK(looksLikeFilesystemTarget(R"(D:\Games\Icarus)"));
  CHECK(looksLikeFilesystemTarget("Content/Paks"));
  CHECK(!looksLikeFilesystemTarget("Icarus"));

  Config cfg;
  cfg.configDirectory = std::filesystem::current_path();
  cfg.games["TestGame"] = GameSettings{
      .paksDir = std::string("missing/paks"),
      .dataPak = std::string("missing/data.pak"),
  };
  const auto report = checkConfiguredGame(cfg, "TestGame", cfg.games.at("TestGame"));
  CHECK(report.level == SupportLevel::Unsupported);

  const auto setup = generateModSetup(cfg, "NoSuchGame");
  CHECK(!setup.viable);
  CHECK(setup.modLua.empty());

  return testFailures() - before;
}
