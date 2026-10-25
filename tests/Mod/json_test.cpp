#include "Check.hpp"
#include "UTool/Mod/JsonEditor.hpp"

#include <stdexcept>

int runJsonTests() {
  using namespace UTool::Mod;
  const int before = testFailures();

  {
    JsonAssetEditor editor(std::string(R"json({"Rows":[{"Name":"Player","MaxDisplayLevel":60}]})json"));
    const int updated =
        editor.setOnArrayElementsWhere("/Rows", "Name", "Player", "/MaxDisplayLevel", 250);
    CHECK(updated == 1);
    CHECK(editor.get("/Rows/0/MaxDisplayLevel") == 250);
  }

  {
    const auto patched = applyPatchOperations(
        R"({"A":1})", nlohmann::json::parse(R"([{"op":"replace","path":"/A","value":2}])"));
    CHECK(nlohmann::json::parse(patched)["A"] == 2);
  }

  {
    JsonAssetEditor editor(std::string(R"({"A":1,"Items":[1,3]})"));
    editor.add("/B", 2);
    CHECK(editor.get("/B") == 2);
    editor.add("/Items/1", 2);
    CHECK(editor.get("/Items") == nlohmann::json::parse("[1,2,3]"));
    bool threw = false;
    try {
      editor.replace("/Missing", 1);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    CHECK(threw);
    editor.replace("/A", 9);
    CHECK(editor.get("/A") == 9);
    editor.replace("/Items/0", 7);
    CHECK(editor.get("/Items/0") == 7);
    CHECK(editor.get("/Items").size() == 3);
  }

  return testFailures() - before;
}
