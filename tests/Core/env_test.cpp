#include "Check.hpp"
#include "TempDir.hpp"
#include "UTool/Core/Env.hpp"

int runEnvTests() {
  using namespace UTool::Core;
  const int before = testFailures();

  TempDir dir("utool-env");
  writeFile(dir.path / ".env",
            "# comment\n"
            "\n"
            "export FOO=bar\n"
            "BAZ=\"qux\"\n"
            "FOO=nope\n"
            "EMPTY=\n"
            "SPACED = value \n"
            "NOEQUALS\n");

  EnvMap map;
  loadEnvFile(dir.path / ".env", map);
  CHECK(map["FOO"] == "bar");
  CHECK(map["BAZ"] == "qux");
  CHECK(map["SPACED"] == "value");
  CHECK(map["EMPTY"].empty());
  CHECK(!map.contains("NOEQUALS"));

  map["FOO"] = "kept";
  loadEnvFile(dir.path / ".env", map);
  CHECK(map["FOO"] == "kept");

  return testFailures() - before;
}
