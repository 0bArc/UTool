#include "UTool/Cli/Interactive.hpp"

#include "UTool/Cli/Commands.hpp"
#include "UTool/Core/Config.hpp"
#include "UTool/Core/NewMod.hpp"
#include "UTool/Version.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace UTool::Cli {
namespace {

namespace fs = std::filesystem;

bool g_color = true;

void prepareConsole() {
#ifdef _WIN32
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(out, mode);
    }
  }

  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(in, &mode)) {
      mode |= ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
      mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
      mode &= ~ENABLE_QUICK_EDIT_MODE;
      mode |= ENABLE_EXTENDED_FLAGS;
      SetConsoleMode(in, mode);
    }
  }
#endif
}

[[nodiscard]] bool readLine(std::string& line) {
  line.clear();
#ifdef _WIN32
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  DWORD consoleMode = 0;
  if (in != INVALID_HANDLE_VALUE && GetConsoleMode(in, &consoleMode)) {
    wchar_t wbuf[2048];
    DWORD n = 0;
    if (!ReadConsoleW(in, wbuf, 2047, &n, nullptr))
      return false;
    while (n > 0 && (wbuf[n - 1] == L'\n' || wbuf[n - 1] == L'\r'))
      --n;
    if (n == 0)
      return true;
    const int need =
        WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
    if (need <= 0)
      return true;
    line.resize(static_cast<size_t>(need));
    WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(n), line.data(), need, nullptr, nullptr);
    return true;
  }
#endif
  return static_cast<bool>(std::getline(std::cin, line));
}

void paint(std::ostream& out, const char* ansi, const std::string& text) {
  if (g_color)
    out << ansi << text << "\033[0m";
  else
    out << text;
}

void paintDim(std::ostream& out, const std::string& text) {
  paint(out, "\033[90m", text);
}

void paintErr(std::ostream& out, const std::string& text) {
  paint(out, "\033[31m", text);
}

[[nodiscard]] bool promptLine(std::ostream& out, const std::string& label,
                              const std::string& defaultValue, std::string& value) {
  paint(out, "\033[36m", label);
  if (!defaultValue.empty()) {
    out << " [";
    paintDim(out, defaultValue);
    out << "]";
  }
  out << ": " << std::flush;
  std::string line;
  if (!readLine(line))
    return false;
  value = line.empty() ? defaultValue : line;
  return true;
}

[[nodiscard]] bool promptYesNo(std::ostream& out, const std::string& label, bool defaultYes,
                               bool& value) {
  const std::string def = defaultYes ? "Y/n" : "y/N";
  paint(out, "\033[36m", label);
  out << " [" << def << "]: " << std::flush;
  std::string line;
  if (!readLine(line))
    return false;
  if (line.empty()) {
    value = defaultYes;
    return true;
  }
  const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[0])));
  value = (c == 'y');
  return true;
}

enum class Menu { Root, Games, Mods, Pak };

[[nodiscard]] const char* menuName(Menu m) {
  switch (m) {
    case Menu::Root:
      return "utool";
    case Menu::Games:
      return "games";
    case Menu::Mods:
      return "mods";
    case Menu::Pak:
      return "pak";
  }
  return "utool";
}

[[nodiscard]] std::optional<Menu> parseMenu(const std::string& s) {
  if (s == "games")
    return Menu::Games;
  if (s == "mods" || s == "mod" || s == "ship")
    return Menu::Mods;
  if (s == "pak")
    return Menu::Pak;
  if (s == "utool" || s == ".." || s == "up" || s == "root")
    return Menu::Root;
  return std::nullopt;
}

void printHelp(std::ostream& out, Menu m) {
  paint(out, "\033[36m", "help");
  out << "  now=" << menuName(m) << "   .. up   exit quit\n";
  out << "  games  list | check <id> | probe <path>\n"
      << "  mods   list | new | open <id> | build <id> | deploy <id> | validate | dir <path>\n"
      << "         banner --all|-f q | link <id> <nexusId>\n"
      << "         ship week <n> | dry-run <n> | upload <n> | sync\n"
      << "  pak    list [@paks] | search <q> <@paks> | preview <path> <@data>\n"
      << "  root   version | help-full | check <id>\n";
  paintDim(out, "jump: games|mods|pak   one-shot: mods list | mods ship week 251 | pak list\n");
  paintDim(out, "CLI:  utool mod help   utool pak help\n");
}

void usage(std::ostream& out, Menu m, const std::string& cmd) {
  paintErr(out, "usage: ");
  switch (m) {
    case Menu::Pak:
      if (cmd == "list")
        out << "list [@paks|@data|<path>]   (default @paks)\n";
      else if (cmd == "search")
        out << "search <query> <@paks|@data|<path>>\n";
      else
        printHelp(out, m);
      break;
    case Menu::Mods:
      if (cmd == "build" || cmd == "deploy" || cmd == "open" || cmd == "link")
        out << cmd << " <mod-id>\n";
      else if (cmd == "dir")
        out << "dir <mods-path>\n";
      else if (cmd == "ship" || cmd == "week" || cmd == "dry-run" || cmd == "upload")
        out << "ship week|dry-run|upload <updateVersion> [-f query]\n";
      else
        printHelp(out, m);
      break;
    case Menu::Games:
      if (cmd == "check")
        out << "check <game-id-or-path>\n";
      else if (cmd == "probe")
        out << "probe <install-path>\n";
      else
        printHelp(out, m);
      break;
    default:
      printHelp(out, m);
      break;
  }
}

void printBanner(std::ostream& out, const fs::path& modsDir) {
  const auto cfg = Core::Config::load(fs::current_path());
  paint(out, "\033[32;1m", "utool ");
  out << VersionString;
  if (cfg.updateVersion)
    out << "  week=" << *cfg.updateVersion;
  out << "  mods=" << modsDir.string() << '\n';
  paintDim(out, "type a menu (games|mods|ship|pak), or help.  .. goes up.  exit quits.\n");
}

void invoke(std::ostream& out, std::vector<std::string> args) {
  const int code = run(std::move(args));
  if (code != 0)
    paintErr(out, "(exit " + std::to_string(code) + ")\n");
}

[[nodiscard]] std::optional<fs::path> findModPath(const fs::path& modsDir, const std::string& idOrPath) {
  const fs::path asPath(idOrPath);
  if (fs::is_directory(asPath) &&
      (fs::is_regular_file(asPath / Core::ModManifest::LuaManifestFileName) ||
       fs::is_regular_file(asPath / Core::ModManifest::ManifestFileName))) {
    return fs::weakly_canonical(asPath);
  }

  for (const auto& mod : Core::discoverMods(modsDir)) {
    if (mod.manifest.id == idOrPath || mod.manifest.name == idOrPath ||
        mod.rootPath.filename().string() == idOrPath) {
      return mod.rootPath;
    }
  }

  const auto cfg = Core::Config::load(modsDir);
  for (const auto& entry : cfg.catalog) {
    if (entry.id == idOrPath) {
      const auto p = modsDir / entry.path;
      if (fs::is_directory(p))
        return fs::weakly_canonical(p);
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::string catalogIdForMod(const fs::path& modsDir, const fs::path& modDir) {
  const auto cfg = Core::Config::load(modsDir);
  std::error_code ec;
  const auto canon = fs::weakly_canonical(modDir, ec);
  for (const auto& entry : cfg.catalog) {
    const auto p = fs::weakly_canonical(modsDir / entry.path, ec);
    if (!ec && p == canon)
      return entry.id;
  }
  return modDir.filename().string();
}

[[nodiscard]] fs::path defaultModsDir() {
  if (fs::is_directory("examples"))
    return fs::path("examples");
  return fs::current_path();
}

[[nodiscard]] std::vector<std::string> splitArgs(const std::string& line) {
  std::vector<std::string> out;
  std::istringstream iss(line);
  std::string tok;
  while (iss >> tok)
    out.push_back(tok);
  return out;
}

void listMods(std::ostream& out, const fs::path& modsDir) {
  out << "Scanning " << modsDir.string() << '\n';
  const auto found = Core::discoverMods(modsDir);
  if (found.empty()) {
    out << "No mods found\n";
    return;
  }
  for (const auto& m : found) {
    paint(out, "\033[36m", m.manifest.id);
    out << "  " << m.manifest.name << "  " << m.manifest.version << "  (" << m.rootPath.string()
        << ")\n";
  }
  paintDim(out, "next: open <id> | build <id> | new\n");
}

void printModHelp(std::ostream& out, const std::string& catalogId) {
  paint(out, "\033[36m", "mod:" + catalogId);
  out << "  commands:\n"
      << "  build [--force]     build this mod\n"
      << "  deploy              copy built pak to game Paks/mods\n"
      << "  banner              render Nexus media banners\n"
      << "  ship dry-run|upload [week]\n"
      << "  link <nexusModId>   set nexus.files[].modId (e.g. link 340)\n"
      << "  info                manifest + paths\n"
      << "  open                explorer this folder (Windows)\n"
      << "  ..                  leave mod CLI\n"
      << "  exit / quit         quit utool\n";
}

[[nodiscard]] bool runGames(std::ostream& out, const std::vector<std::string>& a) {
  if (a.empty() || a[0] == "list") {
    invoke(out, {"games", "list"});
    return true;
  }
  if (a[0] == "check") {
    if (a.size() < 2) {
      usage(out, Menu::Games, "check");
      return true;
    }
    invoke(out, {"check", a[1]});
    return true;
  }
  if (a[0] == "probe") {
    if (a.size() < 2) {
      usage(out, Menu::Games, "probe");
      return true;
    }
    invoke(out, {"games", "probe", a[1]});
    return true;
  }
  return false;
}

[[nodiscard]] bool runMods(std::ostream& out, fs::path& modsDir, const std::vector<std::string>& a) {
  if (a.empty() || a[0] == "list") {
    listMods(out, modsDir);
    return true;
  }
  if (a[0] == "dir") {
    if (a.size() < 2) {
      usage(out, Menu::Mods, "dir");
      return true;
    }
    modsDir = a[1];
    out << "mods dir -> " << modsDir.string() << '\n';
    return true;
  }
  if (a[0] == "validate") {
    invoke(out, {"mod", "validate", modsDir.string()});
    return true;
  }
  if (a[0] == "new") {
    (void)runNewModWizard(modsDir);
    return true;
  }
  if (a[0] == "open") {
    if (a.size() < 2) {
      usage(out, Menu::Mods, "open");
      return true;
    }
    const auto path = findModPath(modsDir, a[1]);
    if (!path) {
      out << "Mod not found: " << a[1] << "  (try: list)\n";
      return true;
    }
    (void)runModSession(*path, catalogIdForMod(modsDir, *path));
    return true;
  }
  if (a[0] == "build" || a[0] == "deploy") {
    if (a.size() < 2) {
      usage(out, Menu::Mods, a[0]);
      return true;
    }
    invoke(out, {"mod", a[0], "-f", a[1]});
    return true;
  }
  if (a[0] == "banner") {
    std::vector<std::string> args{"mod", "banner"};
    for (size_t i = 1; i < a.size(); ++i)
      args.push_back(a[i]);
    if (a.size() == 1)
      args.push_back("--all");
    args.push_back(modsDir.string());
    invoke(out, std::move(args));
    return true;
  }
  if (a[0] == "link") {
    if (a.size() < 3) {
      usage(out, Menu::Mods, "link");
      return true;
    }
    invoke(out, {"mod", "link", "-f", a[1], a[2]});
    return true;
  }
  if (a[0] == "ship") {
    if (a.size() < 2) {
      usage(out, Menu::Mods, "ship");
      return true;
    }
    std::vector<std::string> rest(a.begin() + 1, a.end());
    return runMods(out, modsDir, rest);
  }
  if (a[0] == "sync" || a[0] == "nexus-sync") {
    invoke(out, {"mod", "ship", "sync", modsDir.string()});
    return true;
  }
  if (a[0] == "week" || a[0] == "dry-run" || a[0] == "upload") {
    if (a.size() < 2) {
      usage(out, Menu::Mods, a[0]);
      return true;
    }
    std::vector<std::string> args{"mod", "ship", a[1], modsDir.string()};
    if (a[0] == "dry-run")
      args.push_back("--dry-run");
    else if (a[0] == "upload") {
      args.push_back("--upload");
      for (size_t i = 2; i < a.size(); ++i) {
        if (a[i] == "--single" || a[i] == "-single" || a[i] == "single" || a[i] == "--force")
          args.push_back("--single");
        else if ((a[i] == "-f" || a[i] == "--find") && i + 1 < a.size()) {
          args.push_back("-f");
          args.push_back(a[++i]);
        }
      }
    }
    invoke(out, std::move(args));
    return true;
  }
  return false;
}

[[nodiscard]] bool runPak(std::ostream& out, const std::vector<std::string>& a) {
  if (a.empty())
    return false;
  if (a[0] == "list") {
    const std::string src = a.size() >= 2 ? a[1] : "@paks";
    invoke(out, {"pak", "list", src});
    return true;
  }
  if (a[0] == "search") {
    if (a.size() < 3) {
      usage(out, Menu::Pak, "search");
      return true;
    }
    invoke(out, {"pak", "search", a[1], "--from", a[2]});
    return true;
  }
  if (a[0] == "preview" || a[0] == "open" || a[0] == "extract" || a[0] == "snippet") {
    std::vector<std::string> args{"pak", a[0]};
    for (size_t i = 1; i < a.size(); ++i)
      args.push_back(a[i]);
    invoke(out, std::move(args));
    return true;
  }
  if (a[0] == "build-mod") {
    // Quiet alias -> mod build
    if (a.size() < 2) {
      out << "use: mods build <id>   or   utool mod build -f <query>\n";
      return true;
    }
    if (a[1] == "-f" || a[1] == "--find") {
      if (a.size() < 3) {
        out << "use: mods build <id>\n";
        return true;
      }
      invoke(out, {"mod", "build", "-f", a[2]});
      return true;
    }
    invoke(out, {"mod", "build", a[1]});
    return true;
  }
  return false;
}

[[nodiscard]] bool runInMenu(std::ostream& out, Menu menu, fs::path& modsDir,
                             const std::vector<std::string>& a) {
  switch (menu) {
    case Menu::Games:
      return runGames(out, a);
    case Menu::Mods:
      return runMods(out, modsDir, a);
    case Menu::Pak:
      return runPak(out, a);
    case Menu::Root:
      break;
  }

  if (a.empty())
    return false;
  if (a[0] == "version") {
    out << VersionString << '\n';
    return true;
  }
  if (a[0] == "help-full") {
    invoke(out, {"help"});
    return true;
  }
  if (a[0] == "color") {
    g_color = true;
    out << "Colors ON\n";
    return true;
  }
  if (a[0] == "nocolor") {
    g_color = false;
    out << "Colors OFF\n";
    return true;
  }
  if (a[0] == "check" && a.size() >= 2) {
    invoke(out, {"check", a[1]});
    return true;
  }
  if (a[0] == "discover") {
    invoke(out, {"mod", "list", modsDir.string()});
    return true;
  }
  if (a[0] == "banner") {
    std::vector<std::string> args{"mod", "banner"};
    for (size_t i = 1; i < a.size(); ++i)
      args.push_back(a[i]);
    if (a.size() == 1)
      args.push_back("--all");
    args.push_back(modsDir.string());
    invoke(out, std::move(args));
    return true;
  }
  return false;
}

void unknown(std::ostream& out, Menu menu, const std::string& line) {
  paintErr(out, "unknown: " + line + "\n");
  paintDim(out, "try: help\n");
  (void)menu;
}

[[nodiscard]] fs::path modsDirFor(const fs::path& modDir) {
  if (modDir.has_parent_path() && modDir.parent_path().has_parent_path())
    return modDir.parent_path().parent_path();
  return defaultModsDir();
}

}  // namespace

bool isInteractiveTty() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

int runModSession(const fs::path& modDir, const std::string& catalogIdIn) {
  prepareConsole();
  const fs::path modsDir = modsDirFor(modDir);
  const std::string catalogId =
      catalogIdIn.empty() ? catalogIdForMod(modsDir, modDir) : catalogIdIn;

  Core::ModPackage pkg;
  try {
    pkg = Core::loadModPackage(modDir);
  } catch (const std::exception& ex) {
    paintErr(std::cout, std::string("load failed: ") + ex.what() + "\n");
    return 1;
  }

  paint(std::cout, "\033[32;1m", "mod ");
  paint(std::cout, "\033[36m", catalogId);
  std::cout << "  " << pkg.manifest.name << "  (" << modDir.string() << ")\n";
  printModHelp(std::cout, catalogId);

  std::string line;
  while (true) {
    paint(std::cout, "\033[33m", "mod:" + catalogId);
    std::cout << "> " << std::flush;
    if (!readLine(line))
      break;

    auto args = splitArgs(line);
    if (args.empty())
      continue;

    if (args[0] == "exit" || args[0] == "quit" || args[0] == "q") {
      std::cout << "Bye.\n";
      return 0;
    }
    if (args[0] == ".." || args[0] == "back" || args[0] == "up") {
      std::cout << "Leaving mod CLI.\n";
      return 0;
    }
    if (args[0] == "help" || args[0] == "?") {
      printModHelp(std::cout, catalogId);
      continue;
    }
    if (args[0] == "info") {
      std::cout << "id=" << pkg.manifest.id << "\n"
                << "name=" << pkg.manifest.name << "\n"
                << "version=" << pkg.manifest.version << "\n"
                << "dir=" << modDir.string() << "\n"
                << "catalog=" << catalogId << "\n";
      if (pkg.manifest.updateVersion)
        std::cout << "updateVersion=" << *pkg.manifest.updateVersion << "\n";
      continue;
    }
    if (args[0] == "build") {
      std::vector<std::string> cmd{"mod", "build", modDir.string()};
      for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--force" || args[i] == "force")
          cmd.push_back("--force-extract");
      }
      invoke(std::cout, std::move(cmd));
      continue;
    }
    if (args[0] == "deploy") {
      invoke(std::cout, {"mod", "deploy", modDir.string()});
      continue;
    }
    if (args[0] == "banner") {
      invoke(std::cout, {"mod", "banner", catalogId, modsDir.string()});
      continue;
    }
    if (args[0] == "link") {
      if (args.size() < 2) {
        paintErr(std::cout, "usage: link <nexusModId>   e.g. link 340\n");
        continue;
      }
      try {
        const auto nid = std::stoll(args[1]);
        auto cfg = Core::Config::load(modsDir);
        const auto linked = Core::linkModToNexus(cfg, catalogId, nid);
        if (!linked.ok)
          paintErr(std::cout, linked.message + "\n");
        else
          paint(std::cout, "\033[32m", linked.message + "\n");
      } catch (...) {
        paintErr(std::cout, "Bad Nexus mod id\n");
      }
      continue;
    }
    if (args[0] == "open") {
#ifdef _WIN32
      const std::string cmd = "explorer \"" + modDir.string() + "\"";
      std::system(cmd.c_str());
#else
      std::cout << modDir.string() << '\n';
#endif
      continue;
    }
    if (args[0] == "ship") {
      if (args.size() < 2) {
        paintErr(std::cout, "usage: ship dry-run|upload [week]\n");
        continue;
      }
      const auto cfg = Core::Config::load(modsDir);
      std::string week =
          cfg.updateVersion && !cfg.updateVersion->empty() ? *cfg.updateVersion : "";
      const std::string mode = args[1];
      size_t i = 2;
      if (i < args.size() && args[i].find_first_not_of("0123456789") == std::string::npos) {
        week = args[i];
        ++i;
      }
      if (week.empty()) {
        paintErr(std::cout, "no week — pass week number or set updateVersion\n");
        continue;
      }
      std::vector<std::string> cmd{"mod", "ship", week, modsDir.string(), "-f", catalogId, "--single"};
      if (mode == "dry-run" || mode == "dry")
        cmd.push_back("--dry-run");
      else if (mode == "upload")
        cmd.push_back("--upload");
      else {
        paintErr(std::cout, "usage: ship dry-run|upload [week]\n");
        continue;
      }
      invoke(std::cout, std::move(cmd));
      continue;
    }

    paintErr(std::cout, "unknown: " + line + "\n");
    paintDim(std::cout, "try: help\n");
  }

  std::cout << "Bye.\n";
  return 0;
}

int runNewModWizard(const fs::path& modsDir) {
  prepareConsole();
  if (!isInteractiveTty()) {
    std::cerr << "utool new mod needs a TTY. Run from a terminal.\n";
    return 1;
  }

  auto config = Core::Config::load(modsDir);
  if (config.configFilePath.empty()) {
    paintErr(std::cout, "No utool.json found near " + modsDir.string() + "\n");
    return 1;
  }

  paint(std::cout, "\033[32;1m", "utool new mod\n");
  paintDim(std::cout, "Scaffold or link an existing examples mod, then open mod CLI.\n");

  bool existing = false;
  if (!promptYesNo(std::cout, "Already in examples", false, existing)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  if (existing) {
    paintDim(std::cout, "Pick catalog id (folder name), e.g. rapidcraft — not the Nexus number.\n");
    std::string catalogId;
    if (!promptLine(std::cout, "Catalog id", "", catalogId) || catalogId.empty()) {
      paintErr(std::cout, "Cancelled.\n");
      return 1;
    }
    catalogId = Core::slugifyModId(catalogId);

    const auto path = findModPath(modsDir, catalogId);
    if (!path) {
      paintErr(std::cout, "Mod not found: " + catalogId + "  (try: utool mods list)\n");
      return 1;
    }

    // Ensure catalog entry exists (discover-only folders can still open).
    bool inCatalog = false;
    for (const auto& e : config.catalog) {
      if (e.id == catalogId) {
        inCatalog = true;
        break;
      }
    }
    if (!inCatalog) {
      paintDim(std::cout, "Not in catalog yet — add ship entry?\n");
      bool add = true;
      if (!promptYesNo(std::cout, "Add to ship catalog", true, add)) {
        paintErr(std::cout, "Cancelled.\n");
        return 1;
      }
      if (add) {
        Core::CatalogEntry entry;
        entry.id = catalogId;
        // path relative to modsDir
        auto rel = std::filesystem::relative(*path, modsDir);
        entry.path = rel.generic_string();
        entry.category = path->parent_path().filename().string();
        entry.ship = true;
        try {
          Core::writeConfigCatalogEntry(config, entry);
        } catch (const std::exception& ex) {
          paintErr(std::cout, std::string(ex.what()) + "\n");
          return 1;
        }
      }
    }

    std::string nexusStr;
    paintDim(std::cout, "Nexus page id from url .../mods/340 — Enter to skip.\n");
    if (!promptLine(std::cout, "Nexus mod id", "", nexusStr)) {
      paintErr(std::cout, "Cancelled.\n");
      return 1;
    }
    if (!nexusStr.empty()) {
      try {
        const auto nid = std::stoll(nexusStr);
        const auto linked = Core::linkModToNexus(config, catalogId, nid);
        if (!linked.ok) {
          paintErr(std::cout, linked.message + "\n");
          paintDim(std::cout, "Tip: stubs need id '" + catalogId + "' or '" + catalogId +
                                  "-*'. Continuing to mod CLI.\n");
        } else {
          paint(std::cout, "\033[32m", linked.message + "\n");
        }
      } catch (...) {
        paintErr(std::cout, "Bad Nexus mod id\n");
        return 1;
      }
    }

    paintDim(std::cout, "Opening mod CLI.\n");
    return runModSession(*path, catalogId);
  }

  Core::NewModRequest req;

  if (!promptLine(std::cout, "Display name", "", req.displayName) || req.displayName.empty()) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  paintDim(std::cout, "Folder + ship key (rapidcraft). NOT the Nexus /mods/340 number.\n");
  const std::string idDefault = Core::slugifyModId(req.displayName);
  if (!promptLine(std::cout, "Catalog id / folder", idDefault, req.catalogId) ||
      req.catalogId.empty()) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }
  req.catalogId = Core::slugifyModId(req.catalogId);

  {
    std::ostringstream cats;
    for (size_t i = 0; i < Core::knownModCategories().size(); ++i) {
      if (i)
        cats << "/";
      cats << Core::knownModCategories()[i];
    }
    paintDim(std::cout, "categories: " + cats.str() + " (or custom)\n");
  }
  if (!promptLine(std::cout, "Category", "survival", req.category)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  if (!promptLine(std::cout, "Description", req.displayName, req.description)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  if (!promptLine(std::cout, "Author", "you", req.author)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  if (!promptLine(std::cout, "Game id", "Icarus", req.gameId)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  if (!promptYesNo(std::cout, "Add to ship catalog", true, req.ship)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }

  {
    std::string nexusStr;
    paintDim(std::cout, "Optional Nexus .../mods/<id>. Link later: utool mod link <id> <nexusId>\n");
    if (!promptLine(std::cout, "Nexus mod id", "", nexusStr)) {
      paintErr(std::cout, "Cancelled.\n");
      return 1;
    }
    if (!nexusStr.empty()) {
      try {
        req.nexusModId = std::stoll(nexusStr);
      } catch (...) {
        paintErr(std::cout, "Bad Nexus mod id\n");
        return 1;
      }
    }
  }

  paintDim(std::cout, "template: blank | scale  (scale = multi-tier field divide stub)\n");
  std::string tmpl;
  if (!promptLine(std::cout, "Template", "blank", tmpl)) {
    paintErr(std::cout, "Cancelled.\n");
    return 1;
  }
  if (tmpl == "scale" || tmpl == "scale-field" || tmpl == "tiers") {
    req.tmpl = Core::NewModTemplate::ScaleField;
    std::string assets;
    std::string field;
    paintDim(std::cout, "Comma-separated, e.g. D_ProcessorRecipes.json, D_ExtractorRecipes.json\n");
    if (!promptLine(std::cout, "Asset files", "D_Example.json", assets) ||
        !promptLine(std::cout, "Field to divide", "Value", field)) {
      paintErr(std::cout, "Cancelled.\n");
      return 1;
    }
    req.assetFiles = Core::splitAssetFileList(assets);
    if (req.assetFiles.empty())
      req.assetFiles.push_back("D_Example.json");
    req.scaleField = field;
  } else {
    req.tmpl = Core::NewModTemplate::Blank;
  }

  const auto created = Core::createNewMod(config, modsDir, req);
  if (!created.ok) {
    paintErr(std::cout, created.message + "\n");
    return 1;
  }

  paint(std::cout, "\033[32m", created.message + "\n");
  paintDim(std::cout, "Opening mod CLI. Edit mod.lua, then: build / banner / ship dry-run\n");
  return runModSession(created.modDir, created.catalogId);
}

int runInteractiveSession() {
  prepareConsole();
  fs::path modsDir = defaultModsDir();
  Menu menu = Menu::Root;

  printBanner(std::cout, modsDir);
  std::cout << std::flush;

  std::string line;
  while (true) {
    paint(std::cout, "\033[33m", menuName(menu));
    std::cout << "> " << std::flush;
    if (!readLine(line))
      break;

    auto args = splitArgs(line);
    while (!args.empty() && args.front() == "utool")
      args.erase(args.begin());
    if (args.empty())
      continue;

    if (args[0] == "exit" || args[0] == "quit" || args[0] == "q") {
      std::cout << "Bye.\n";
      return 0;
    }
    if (args[0] == "help" || args[0] == "?" || args[0] == "hints") {
      printHelp(std::cout, menu);
      continue;
    }

    if (const auto jump = parseMenu(args[0])) {
      std::vector<std::string> rest(args.begin() + 1, args.end());
      if (rest.empty()) {
        menu = *jump;
        if (menu != Menu::Root)
          printHelp(std::cout, menu);
        continue;
      }
      if (!runInMenu(std::cout, *jump, modsDir, rest))
        unknown(std::cout, *jump, line);
      continue;
    }

    if (!runInMenu(std::cout, menu, modsDir, args))
      unknown(std::cout, menu, line);
  }

  std::cout << "Bye.\n";
  return 0;
}

}  // namespace UTool::Cli
