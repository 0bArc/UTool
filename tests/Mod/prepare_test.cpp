#include "Check.hpp"
#include "TempDir.hpp"
#include "UTool/Mod/Prepare.hpp"

int runPrepareTests() {
  using namespace UTool;
  const int before = testFailures();

  TempDir dir("utool-prepare");
  writeFile(dir.path / "content" / "a.txt", "from-content");
  writeFile(dir.path / "prepared" / "b.txt", "from-prepared");

  Core::ModPackage package;
  package.rootPath = dir.path;
  package.manifest.id = "prep";
  package.manifest.name = "Prep";
  package.manifest.version = "1";
  package.manifest.contentRoots = {"content"};

  const auto onlyContent = Mod::mergeForPack(package, dir.path / "missing-prepared");
  CHECK(onlyContent == dir.path / "content");
  CHECK(std::filesystem::is_regular_file(onlyContent / "a.txt"));

  const auto merged = Mod::mergeForPack(package, dir.path / "prepared");
  CHECK(std::filesystem::is_regular_file(merged / "a.txt"));
  CHECK(std::filesystem::is_regular_file(merged / "b.txt"));

  return testFailures() - before;
}
