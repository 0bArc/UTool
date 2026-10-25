#include "Check.hpp"

int runConfigTests();
int runEnvTests();
int runGameCheckTests();
int runManifestTests();
int runCurveTests();
int runJsonTests();
int runPrepareTests();
int runNexusTests();

int main() {
  runConfigTests();
  runEnvTests();
  runGameCheckTests();
  runManifestTests();
  runCurveTests();
  runJsonTests();
  runPrepareTests();
  runNexusTests();
  if (testFailures() != 0) {
    std::cerr << testFailures() << " check(s) failed\n";
    return 1;
  }
  std::cout << "utool_tests ok\n";
  return 0;
}
