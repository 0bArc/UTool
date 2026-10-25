#include "Check.hpp"
#include "UTool/Nexus/Client.hpp"

int runNexusTests() {
  using namespace UTool::Nexus;
  const int before = testFailures();

  CHECK(normalizeNexusFileKey("More XP Week 247 1.5x.zip") == "morexp15x");
  CHECK(normalizeNexusFileKey("Mounts-Week-12.zip").find("week") == std::string::npos);

  UTool::Core::NexusFileEntry entry;
  entry.match = "MoreXP %updateversion% 1.5x*.zip";
  entry.displayName = "More XP Week-%updateversion% 1.5x";
  const auto want = nexusWantKey(entry);
  CHECK(want.find("247") == std::string::npos);
  CHECK(want.find("15x") != std::string::npos);

  RemoteModFile remote;
  remote.name = "More XP Week 247 1.5x";
  remote.category = "MAIN";
  CHECK(scoreNexusFileMatch(want, remote) >= 70);

  RemoteModFile other = remote;
  other.name = "More XP Week 247 500";
  CHECK(scoreNexusFileMatch(want, other) == 0);

  const auto file = nlohmann::json::parse(R"({"id":"42","name":"Old","is_active":true})");
  const auto version = nlohmann::json::parse(R"({"name":"More XP","category":"MAIN","version":"3"})");
  const auto parsed = parseV3ModFile(file, &version);
  CHECK(parsed && parsed->fileId == 42);
  CHECK(parsed->name == "More XP");
  CHECK(parsed->category == "MAIN");
  CHECK(parsed->version == "3");

  const auto inactive = parseV3ModFile(
      nlohmann::json::parse(R"({"id":1,"name":"Gone","is_active":false})"), nullptr);
  CHECK(!inactive);

  return testFailures() - before;
}
