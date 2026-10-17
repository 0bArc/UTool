#include "UTool/Cli/Commands.hpp"

#include <string>
#include <vector>

int main(int argc, char* argv[]) {
  std::vector<std::string> args;
  args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);

  return UTool::Cli::run(args);
}
