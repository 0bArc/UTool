#include "Check.hpp"
#include "UTool/Mod/CurveFloat.hpp"

int runCurveTests() {
  using namespace UTool::Mod;
  const int before = testFailures();

  CurveEditor editor("C_Test", {{1.f, 10.f}, {2.f, 20.f}});
  CHECK(editor.lastKey().time == 2.f);
  CHECK(editor.lastKey().value == 20.f);
  editor.addKey(3.f, 30.f);
  CHECK(editor.keys().size() == 3);
  editor.scaleValues(2.f);
  CHECK(editor.keys().back().value == 60.f);
  editor.setKey(3.f, 5.f);
  CHECK(editor.keys().size() == 3);
  CHECK(editor.keys().back().value == 5.f);

  return testFailures() - before;
}
