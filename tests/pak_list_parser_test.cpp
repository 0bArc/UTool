#include "Check.hpp"
#include "Pak/PakInternal.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

int main() {
  using namespace UTool::Pak;

  std::ifstream in("tests/fixtures/unrealpak_list_sample.txt");
  if (!in)
    in.open("fixtures/unrealpak_list_sample.txt");
  CHECK(in && "fixture file missing");
  if (!in)
    return 1;

  std::ostringstream ss;
  ss << in.rdbuf();
  const auto entries = parsePakListOutput(ss.str());
  CHECK(entries.size() == 2);
  if (entries.size() == 2) {
    CHECK(entries[0].virtualPath == "Data/Character/D_CharacterGrowth.json");
    CHECK(entries[0].size == 4096);
    CHECK(entries[0].offset == 0);
    CHECK(entries[1].virtualPath == "Data/Character/C_PlayerXP.uasset");
    CHECK(entries[1].size == 2048);
    CHECK(fileExtensionLower(entries[1].virtualPath) == "uasset");
  }

  if (testFailures() != 0) {
    std::cerr << testFailures() << " check(s) failed\n";
    return 1;
  }
  std::cout << "pak_list_parser_test ok\n";
  return 0;
}
