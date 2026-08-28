#include "Pak/PakInternal.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

int main() {
  using namespace UTool::Pak;

  std::ifstream fixture("tests/fixtures/unrealpak_list_sample.txt");
  if (!fixture) {
    fixture.open("fixtures/unrealpak_list_sample.txt");
  }
  assert(fixture && "fixture file missing");

  std::ostringstream ss;
  ss << fixture.rdbuf();
  const auto entries = parsePakListOutput(ss.str());
  assert(entries.size() == 2);
  assert(entries[0].virtualPath == "Data/Character/D_CharacterGrowth.json");
  assert(entries[0].size == 4096);
  assert(entries[0].offset == 0);
  assert(entries[1].virtualPath == "Data/Character/C_PlayerXP.uasset");
  assert(entries[1].size == 2048);
  assert(fileExtensionLower(entries[1].virtualPath) == "uasset");

  std::cout << "pak_list_parser_test ok\n";
  return 0;
}
