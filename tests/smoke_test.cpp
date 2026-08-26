#include "UTool/Mod/CurveFloat.hpp"
#include "UTool/Mod/JsonEditor.hpp"

#include <cassert>
#include <iostream>

int main() {
  using namespace UTool::Mod;

  {
    CurveEditor editor("C_Test", {{1.f, 10.f}, {2.f, 20.f}});
    assert(editor.lastKey().time == 2.f);
    editor.addKey(3.f, 30.f);
    assert(editor.keys().size() == 3);
  }

  {
    JsonAssetEditor editor(std::string(R"json({"Rows":[{"Name":"Player","MaxDisplayLevel":60}]})json"));
    editor.setOnArrayElementsWhere("/Rows", "Name", "Player", "/MaxDisplayLevel", 250);
    const auto json = editor.toJson(false);
    assert(json.find("250") != std::string::npos);
  }

  {
    const auto patched = applyPatchOperations(
        R"({"A":1})",
        nlohmann::json::parse(R"([{"op":"replace","path":"/A","value":2}])"));
    assert(patched.find('2') != std::string::npos);
  }

  std::cout << "utool_tests ok\n";
  return 0;
}
