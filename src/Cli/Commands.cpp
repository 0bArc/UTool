#include "UTool/Cli/Commands.hpp"

#include "UTool/Core/Config.hpp"
#include "UTool/Mod/Prepare.hpp"
#include "UTool/Version.hpp"

#include <iostream>
#include <optional>
#include <string_view>

namespace UTool::Cli {
namespace {

void printHelp() {
  std::cout
      << "utool " << VersionString << "\n"
      << "UE4/UE5 modding toolkit (Lua mods).\n\n"
      << "Usage:\n"
      << "  utool help\n"
      << "  utool --version\n"
      << "  utool discover <mods-dir>\n"
      << "  utool validate <mods-dir>\n"
      << "  utool pak build-mod <mod-dir> [-o out.pak] [--mount ...] [--force-extract] [-compress]\n";
}

[[nodiscard]] bool isVersionFlag(std::string_view arg) {
  return arg == "--version" || arg == "-V" || arg == "version";
}

[[nodiscard]] std::optional<std::string> getArg(
    const std::vector<std::string>& args,
    std::string_view name) {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name)
      return args[i + 1];
  }
  return std::nullopt;
}

[[nodiscard]] bool hasFlag(const std::vector<std::string>& args, std::string_view name) {
  for (const auto& a : args) {
    if (a == name)
      return true;
  }
  return false;
}

int cmdDiscover(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Usage: utool discover <mods-dir>\n";
    return 1;
  }
  const auto mods = Core::discoverMods(args[1]);
  if (mods.empty()) {
    std::cout << "No mods found in " << args[1] << '\n';
    return 0;
  }
  for (const auto& mod : mods) {
    std::cout << mod.manifest.id << "  " << mod.manifest.name << "  " << mod.manifest.version
              << "  (" << mod.rootPath.string() << ")\n";
  }
  return 0;
}

int cmdValidate(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "Usage: utool validate <mods-dir>\n";
    return 1;
  }
  const auto mods = Core::discoverMods(args[1]);
  int failures = 0;
  for (const auto& mod : mods) {
    const auto issues = Core::validateMod(mod);
    if (issues.empty()) {
      std::cout << "OK  " << mod.manifest.id << '\n';
      continue;
    }
    ++failures;
    std::cout << "FAIL  " << mod.manifest.id << '\n';
    for (const auto& issue : issues)
      std::cout << "  - " << issue << '\n';
  }
  return failures == 0 ? 0 : 1;
}

int cmdPakBuildMod(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    std::cerr << "Usage: utool pak build-mod <mod-dir>\n";
    return 1;
  }

  try {
    const auto package = Core::loadModPackage(args[2]);
    const auto config = Core::Config::load(package.rootPath);

    std::optional<std::filesystem::path> output;
    if (const auto oFlag = getArg(args, "-o"))
      output = *oFlag;
    else if (const auto outputFlag = getArg(args, "--output"))
      output = *outputFlag;

    std::optional<std::string> mount = getArg(args, "--mount");
    const bool compress = hasFlag(args, "-compress") || hasFlag(args, "--compress");
    const bool force = hasFlag(args, "--force-extract");

    const auto built = Mod::buildMod(package, config, output, mount, compress, force);
    if (!built.ok) {
      std::cerr << built.message << '\n';
      return 1;
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

}  // namespace

int run(const std::vector<std::string>& args) {
  if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
    printHelp();
    return 0;
  }

  if (isVersionFlag(args[0])) {
    std::cout << VersionString << '\n';
    return 0;
  }

  if (args[0] == "discover")
    return cmdDiscover(args);
  if (args[0] == "validate")
    return cmdValidate(args);

  if (args[0] == "pak") {
    if (args.size() >= 2 && args[1] == "build-mod")
      return cmdPakBuildMod(args);
    std::cerr << "utool pak: expected 'build-mod'\n";
    printHelp();
    return 1;
  }

  std::cerr << "Unknown command: " << args[0] << '\n';
  printHelp();
  return 1;
}

}  // namespace UTool::Cli
