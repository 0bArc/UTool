#include "UTool/Core/GameCheck.hpp"

#include "UTool/Pak/UnrealPak.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <system_error>

namespace UTool::Core {
namespace {

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

std::string statusLabel(CheckLine::Status status) {
  switch (status) {
    case CheckLine::Status::Ok:
      return "ok";
    case CheckLine::Status::Warn:
      return "warn";
    case CheckLine::Status::Fail:
      return "fail";
    default:
      return "info";
  }
}

std::string supportLabel(SupportLevel level) {
  switch (level) {
    case SupportLevel::Supported:
      return "supported";
    case SupportLevel::Partial:
      return "partial";
    default:
      return "unsupported";
  }
}

std::optional<std::filesystem::path> weakCanonical(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec))
    return std::nullopt;
  const auto c = std::filesystem::weakly_canonical(path, ec);
  if (ec)
    return path;
  return c;
}

bool pathEquivalent(const std::filesystem::path& a, const std::filesystem::path& b) {
  const auto ca = weakCanonical(a);
  const auto cb = weakCanonical(b);
  if (!ca || !cb)
    return false;
  return iequals(ca->string(), cb->string());
}

void addLine(GameCheckReport& report, CheckLine::Status status, std::string message) {
  report.lines.push_back({status, std::move(message)});
}

void addDetail(GameCheckReport& report, CheckLine::Status status, std::string message) {
  report.details.push_back({status, std::move(message)});
}

std::string formatBytes(std::uintmax_t bytes) {
  if (bytes >= 1024 * 1024 * 1024)
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
  if (bytes >= 1024 * 1024)
    return std::to_string(bytes / (1024 * 1024)) + " MB";
  if (bytes >= 1024)
    return std::to_string(bytes / 1024) + " KB";
  return std::to_string(bytes) + " B";
}

std::string jsonEscapePath(const std::filesystem::path& path) {
  std::string s = path.string();
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '\\')
      out += "\\\\";
    else
      out += c;
  }
  return out;
}

struct PakEntry {
  std::filesystem::path path;
  std::uintmax_t bytes = 0;
};

std::vector<PakEntry> listPakEntries(const std::filesystem::path& dir) {
  std::vector<PakEntry> entries;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec))
    return entries;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec || !entry.is_regular_file())
      continue;
    if (!iequals(entry.path().extension().string(), ".pak"))
      continue;
    PakEntry pe;
    pe.path = entry.path();
    pe.bytes = entry.file_size(ec);
    entries.push_back(std::move(pe));
  }
  std::sort(entries.begin(), entries.end(), [](const PakEntry& a, const PakEntry& b) {
    return a.path.filename().string() < b.path.filename().string();
  });
  return entries;
}

std::optional<std::filesystem::path> detectProjectRoot(const ProbedInstall& probe) {
  if (!probe.paksDir)
    return std::nullopt;
  const auto content = probe.paksDir->parent_path();
  if (iequals(content.filename().string(), "Content"))
    return content.parent_path();
  return std::nullopt;
}

std::string suggestGameId(const ProbedInstall& probe) {
  if (auto project = detectProjectRoot(probe)) {
    std::string name = project->filename().string();
    std::string id;
    id.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
      const char c = name[i];
      if (i == 0)
        id += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      else if (c == ' ' || c == '-' || c == '_')
        continue;
      else
        id += c;
    }
    if (!id.empty())
      return id;
  }
  return "MyGame";
}

void appendFeatureMatrix(GameCheckReport& report, bool hasDataPak, bool paksReadable, bool hasAesKey) {
  addDetail(report, CheckLine::Status::Info, "Feature support:");
  addDetail(report, CheckLine::Status::Info,
            "  pack content/ into .pak: yes (requires UnrealPak)");
  addDetail(report, CheckLine::Status::Info,
            hasDataPak ? "  JSON table patching (@data): yes"
                       : "  JSON table patching (@data): no (no data.pak)");
  if (paksReadable)
    addDetail(report, CheckLine::Status::Info,
              "  extract/list game paks (@paks): yes (tested UnrealPak -List)");
  else if (hasAesKey)
    addDetail(report, CheckLine::Status::Info,
              "  extract/list game paks (@paks): likely (pakAesKey configured, not re-tested here)");
  else
    addDetail(report, CheckLine::Status::Warn,
              "  extract/list game paks (@paks): blocked (paks encrypted or need pakAesKey)");
  addDetail(report, CheckLine::Status::Info,
            paksReadable || hasAesKey
                ? "  curve patching (patch_curve): maybe (needs extractable .uasset in paks)"
                : "  curve patching (patch_curve): unlikely until paks are readable");
}

void appendPartialDetails(
    GameCheckReport& report,
    const ProbedInstall& probe,
    const Config& config,
    const GameSettings* matchedSettings) {
  if (!probe.paksDir && !probe.singlePakFile)
    return;

  addDetail(report, CheckLine::Status::Info, "--- details ---");

  if (auto project = detectProjectRoot(probe))
    addDetail(report, CheckLine::Status::Info, "project folder: " + project->string());

  const std::vector<PakEntry> paks =
      probe.singlePakFile
          ? std::vector<PakEntry>{{{probe.inputPath, [&] {
                                       std::error_code ec;
                                       return std::filesystem::file_size(probe.inputPath, ec);
                                     }()}}}
          : listPakEntries(*probe.paksDir);

  if (!paks.empty()) {
    addDetail(report, CheckLine::Status::Info, "pak files:");
    for (const auto& pak : paks)
      addDetail(report, CheckLine::Status::Info,
                "  " + pak.path.filename().string() + " (" + formatBytes(pak.bytes) + ")");
  }

  bool paksReadable = false;
  std::optional<Pak::UnrealPakOptions> pakOptions;
  try {
    const auto toolchain = Pak::resolveToolchain(
        config.unrealPak, config.unrealEngineDir, config.configDirectory, false);
    pakOptions = Pak::toOptions(toolchain);
  } catch (...) {
  }

  if (pakOptions && !paks.empty()) {
    const PakEntry* testPak = &paks.front();
    for (const auto& pak : paks) {
      if (pak.bytes > testPak->bytes)
        testPak = &pak;
    }
    addDetail(report, CheckLine::Status::Info,
              "testing UnrealPak -List on: " + testPak->path.filename().string());
    paksReadable = Pak::tryListPak(testPak->path, *pakOptions);
    if (paksReadable)
      addDetail(report, CheckLine::Status::Ok, "pak list succeeded (not encrypted, or key in Crypto.json)");
    else
      addDetail(report, CheckLine::Status::Warn,
                "pak list failed: likely encrypted; set pakAesKey in utool.json for this game");
  }

  const bool hasAesKey = matchedSettings && matchedSettings->pakAesKey.has_value();
  appendFeatureMatrix(report, probe.dataPak.has_value(), paksReadable, hasAesKey);

  if (!report.matchedConfigId && probe.paksDir) {
    const std::string gameId = suggestGameId(probe);
    addDetail(report, CheckLine::Status::Info, "suggested utool.json entry:");
    addDetail(report, CheckLine::Status::Info, "  \"games\": {");
    addDetail(report, CheckLine::Status::Info, "    \"" + gameId + "\": {");
    addDetail(report, CheckLine::Status::Info,
              "      \"paksDir\": \"" + jsonEscapePath(*probe.paksDir) + "\",");
    if (probe.dataPak)
      addDetail(report, CheckLine::Status::Info,
                "      \"dataPak\": \"" + jsonEscapePath(*probe.dataPak) + "\",");
    if (!paksReadable)
      addDetail(report, CheckLine::Status::Info, "      \"pakAesKey\": \"YOUR_AES_KEY\",");
    addDetail(report, CheckLine::Status::Info, "    }");
    addDetail(report, CheckLine::Status::Info, "  }");
    addDetail(report, CheckLine::Status::Info,
              "Then use target = { gameId = \"" + gameId + "\" } in mod.lua");
  }
}

void appendConfiguredPartialDetails(
    GameCheckReport& report,
    std::string_view gameId,
    const GameSettings& settings,
    const Config& config) {
  addDetail(report, CheckLine::Status::Info, "--- details ---");

  if (settings.paksDir) {
    try {
      const auto dir = config.resolvePath(*settings.paksDir);
      for (const auto& pak : listPakEntries(dir))
        addDetail(report, CheckLine::Status::Info,
                  "  pak: " + pak.path.filename().string() + " (" + formatBytes(pak.bytes) + ")");
    } catch (...) {
    }
  }

  bool paksReadable = false;
  if (settings.paksDir) {
    try {
      const auto toolchain = Pak::resolveToolchain(
          config.unrealPak, config.unrealEngineDir, config.configDirectory, false);
      const auto opts = Pak::toOptions(toolchain);
      const auto dir = config.resolvePath(*settings.paksDir);
      const auto paks = listPakEntries(dir);
      if (!paks.empty()) {
        const PakEntry* testPak = &paks.front();
        for (const auto& pak : paks) {
          if (pak.bytes > testPak->bytes)
            testPak = &pak;
        }
        paksReadable = Pak::tryListPak(testPak->path, opts);
      }
    } catch (...) {
    }
  }

  const bool hasDataPak = settings.dataPak.has_value();
  appendFeatureMatrix(report, hasDataPak, paksReadable, settings.pakAesKey.has_value());

  if (!hasDataPak && !iequals(gameId, "Icarus"))
    addDetail(report, CheckLine::Status::Info,
              "This game uses paks-only layout (no data.pak). Use content/ mods or curve patches, not utool.asset on @data.");
}

std::size_t countPakFiles(const std::filesystem::path& dir) {
  std::size_t count = 0;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec))
    return 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file())
      continue;
    if (iequals(entry.path().extension().string(), ".pak"))
      ++count;
  }
  return count;
}

std::optional<std::filesystem::path> findPaksDir(const std::filesystem::path& root) {
  namespace fs = std::filesystem;
  const auto tryDir = [](const fs::path& candidate) -> std::optional<fs::path> {
    std::error_code ec;
    if (!fs::is_directory(candidate, ec))
      return std::nullopt;
    if (countPakFiles(candidate) > 0)
      return weakCanonical(candidate);
    return std::nullopt;
  };

  if (auto d = tryDir(root))
    return d;

  const fs::path contentPaks = root / "Content" / "Paks";
  if (auto d = tryDir(contentPaks))
    return d;

  std::error_code ec;
  if (fs::is_directory(root, ec)) {
    for (const auto& entry : fs::directory_iterator(root, ec)) {
      if (ec || !entry.is_directory())
        continue;
      if (auto d = tryDir(entry.path() / "Content" / "Paks"))
        return d;
    }
  }

  if (iequals(root.filename().string(), "Paks"))
    return tryDir(root);

  return std::nullopt;
}

std::optional<std::filesystem::path> findDataPak(
    const std::filesystem::path& installRoot,
    const std::optional<std::filesystem::path>& paksDir) {
  namespace fs = std::filesystem;

  const auto tryFile = [](const fs::path& candidate) -> std::optional<fs::path> {
    std::error_code ec;
    if (fs::is_regular_file(candidate, ec))
      return weakCanonical(candidate);
    return std::nullopt;
  };

  if (paksDir) {
    if (auto p = tryFile(*paksDir / ".." / "Data" / "data.pak"))
      return p;
  }

  if (auto p = tryFile(installRoot / "Content" / "Data" / "data.pak"))
    return p;

  if (paksDir) {
    if (auto p = tryFile(paksDir->parent_path().parent_path() / "Data" / "data.pak"))
      return p;
  }

  return std::nullopt;
}

void checkUnrealPak(GameCheckReport& report, const Config& config) {
  try {
    const auto paths = Pak::resolveToolchain(
        config.unrealPak, config.unrealEngineDir, config.configDirectory, false);
    addLine(report, CheckLine::Status::Ok, "UnrealPak: " + paths.executable.string());
  } catch (const std::exception& ex) {
    addLine(report, CheckLine::Status::Fail, std::string("UnrealPak: ") + ex.what());
  }
}

void checkAliasResolution(GameCheckReport& report, const Config& config, std::string_view gameId) {
  const std::optional<std::string> id = std::string(gameId);

  try {
    const auto data = config.resolveDataPak(id);
    addLine(report, CheckLine::Status::Ok, "@data resolves to " + data.string());
  } catch (const std::exception& ex) {
    addLine(report, CheckLine::Status::Fail, std::string("@data: ") + ex.what());
  }

  try {
    const auto paks = config.resolveSourcePakPaths("@paks", id);
    if (paks.empty())
      addLine(report, CheckLine::Status::Warn, "@paks resolved but no .pak files were found");
    else
      addLine(report, CheckLine::Status::Ok,
              "@paks resolves (" + std::to_string(paks.size()) + " .pak files)");
  } catch (const std::exception& ex) {
    addLine(report, CheckLine::Status::Fail, std::string("@paks: ") + ex.what());
  }
}

SupportLevel computeLevel(const GameCheckReport& report) {
  bool anyFail = false;
  bool anyWarn = false;
  for (const auto& line : report.lines) {
    if (line.status == CheckLine::Status::Fail)
      anyFail = true;
    if (line.status == CheckLine::Status::Warn)
      anyWarn = true;
  }
  if (anyFail)
    return SupportLevel::Unsupported;
  if (anyWarn)
    return SupportLevel::Partial;
  return SupportLevel::Supported;
}

}  // namespace

bool looksLikeFilesystemTarget(std::string_view target) {
  if (target.empty())
    return false;
  if (target.find_first_of("\\/") != std::string_view::npos)
    return true;
  if (target.size() >= 2 && std::isalpha(static_cast<unsigned char>(target[0])) && target[1] == ':')
    return true;

  std::error_code ec;
  const std::filesystem::path p{std::string(target)};
  if (std::filesystem::exists(p, ec))
    return true;
  return false;
}

ProbedInstall probeInstallPath(const std::filesystem::path& path) {
  ProbedInstall probe;
  probe.inputPath = path;

  std::error_code ec;
  if (std::filesystem::is_regular_file(path, ec) && iequals(path.extension().string(), ".pak")) {
    probe.singlePakFile = true;
    probe.installRoot = weakCanonical(path.parent_path()).value_or(path.parent_path());
    probe.paksDir = probe.installRoot;
    probe.pakCount = 1;
    probe.dataPak = std::nullopt;
    return probe;
  }

  const auto root = weakCanonical(path).value_or(path);
  probe.installRoot = root;
  probe.paksDir = findPaksDir(root);
  if (probe.paksDir)
    probe.pakCount = countPakFiles(*probe.paksDir);
  probe.dataPak = findDataPak(root, probe.paksDir);
  return probe;
}

std::optional<std::string> findConfigGameIdForPaths(
    const Config& config,
    const ProbedInstall& probe) {
  for (const auto& [id, settings] : config.games) {
    if (settings.paksDir) {
      try {
        const auto configured = config.resolvePath(*settings.paksDir);
        if (probe.paksDir && pathEquivalent(configured, *probe.paksDir))
          return id;
      } catch (...) {
      }
    }
    if (settings.dataPak) {
      try {
        const auto configured = config.resolvePath(*settings.dataPak);
        if (probe.dataPak && pathEquivalent(configured, *probe.dataPak))
          return id;
      } catch (...) {
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> findConfigGameIdByName(
    const Config& config,
    std::string_view gameId) {
  for (const auto& [key, value] : config.games) {
    (void)value;
    if (iequals(key, gameId))
      return key;
  }
  return std::nullopt;
}

GameCheckReport checkConfiguredGame(
    const Config& config,
    std::string_view gameId,
    const GameSettings& settings) {
  GameCheckReport report;
  report.gameId = std::string(gameId);

  if (config.configDirectory.empty())
    addLine(report, CheckLine::Status::Warn, "No utool.json found (walk up from current directory)");
  else
    addLine(report, CheckLine::Status::Ok, "utool.json: " + config.configDirectory.string());

  addLine(report, CheckLine::Status::Info, "Checking configured game \"" + report.gameId.value() + "\"");

  if (settings.paksDir) {
    try {
      const auto dir = config.resolvePath(*settings.paksDir);
      const auto count = countPakFiles(dir);
      if (count == 0)
        addLine(report, CheckLine::Status::Fail,
                "paksDir exists but contains no .pak files: " + dir.string());
      else
        addLine(report, CheckLine::Status::Ok,
                "paksDir: " + dir.string() + " (" + std::to_string(count) + " .pak files)");
    } catch (const std::exception& ex) {
      addLine(report, CheckLine::Status::Fail, std::string("paksDir: ") + ex.what());
    }
  } else {
    addLine(report, CheckLine::Status::Warn, "paksDir not set in utool.json for this game");
  }

  if (settings.dataPak) {
    try {
      const auto pak = config.resolvePath(*settings.dataPak);
      std::error_code ec;
      if (std::filesystem::is_regular_file(pak, ec))
        addLine(report, CheckLine::Status::Ok, "dataPak: " + pak.string());
      else
        addLine(report, CheckLine::Status::Fail, "dataPak not found: " + pak.string());
    } catch (const std::exception& ex) {
      addLine(report, CheckLine::Status::Fail, std::string("dataPak: ") + ex.what());
    }
  } else if (settings.paksDir) {
    try {
      const auto derived = config.resolveDataPak(std::string(gameId));
      addLine(report, CheckLine::Status::Ok, "dataPak (derived): " + derived.string());
    } catch (const std::exception& ex) {
      addLine(report, CheckLine::Status::Warn, std::string("dataPak not configured: ") + ex.what());
    }
  } else {
    addLine(report, CheckLine::Status::Fail, "dataPak not configured");
  }

  if (settings.pakAesKey)
    addLine(report, CheckLine::Status::Ok, "pakAesKey configured (encrypted paks supported)");
  else
    addLine(report, CheckLine::Status::Info,
            "pakAesKey not set (only required for encrypted paks)");

  if (settings.mountPoint)
    addLine(report, CheckLine::Status::Info, "mountPoint: " + *settings.mountPoint);
  else if (config.defaultMountPoint)
    addLine(report, CheckLine::Status::Info, "mountPoint (default): " + *config.defaultMountPoint);

  checkAliasResolution(report, config, gameId);
  checkUnrealPak(report, config);

  addLine(report, CheckLine::Status::Info,
          "JSON table patching: supported when dataPak is readable");
  addLine(report, CheckLine::Status::Info,
          "Curve patching: supported when curve uassets are extractable from @paks");

  if (iequals(gameId, "Icarus"))
    addLine(report, CheckLine::Status::Info, "Reference target: fully tested on Icarus / UE 4.27");

  report.level = computeLevel(report);
  if (report.level != SupportLevel::Supported)
    appendConfiguredPartialDetails(report, gameId, settings, config);
  return report;
}

GameCheckReport checkGameTarget(const Config& config, std::string_view gameIdOrPath) {
  if (looksLikeFilesystemTarget(gameIdOrPath)) {
    GameCheckReport report;
    report.queriedPath = std::filesystem::path{std::string(gameIdOrPath)};

    const ProbedInstall probe = probeInstallPath(*report.queriedPath);
    const GameSettings* matchedSettings = nullptr;
    addLine(report, CheckLine::Status::Info, "Probing install path: " + probe.inputPath.string());

    if (probe.singlePakFile) {
      addLine(report, CheckLine::Status::Info, "Single .pak file detected");
      addLine(report, CheckLine::Status::Ok, "pak file: " + report.queriedPath->string());
    } else if (probe.paksDir) {
      addLine(report, CheckLine::Status::Ok,
              "Detected UE Content/Paks: " + probe.paksDir->string() + " (" +
                  std::to_string(probe.pakCount) + " .pak files)");
    } else {
      addLine(report, CheckLine::Status::Fail,
              "No Content/Paks directory with .pak files found under this path");
    }

    if (probe.dataPak)
      addLine(report, CheckLine::Status::Ok, "Detected data.pak: " + probe.dataPak->string());
    else
      addLine(report, CheckLine::Status::Warn,
              "data.pak not found (JSON table patching via @data may be unavailable)");

    report.matchedConfigId = findConfigGameIdForPaths(config, probe);
    if (report.matchedConfigId)
      addLine(report, CheckLine::Status::Ok,
              "Matches utool.json game entry: \"" + *report.matchedConfigId + "\"");
    else if (!config.games.empty())
      addLine(report, CheckLine::Status::Warn,
              "No matching entry in utool.json - add this game under \"games\" to use @data/@paks");
    else
      addLine(report, CheckLine::Status::Warn, "No utool.json games configured");

    checkUnrealPak(report, config);

    if (probe.paksDir && probe.pakCount > 0 && probe.dataPak)
      addLine(report, CheckLine::Status::Info, "Layout matches a typical UE4/UE5 moddable install");
    else if (probe.paksDir && probe.pakCount > 0)
      addLine(report, CheckLine::Status::Info,
              "Partial UE layout: pak pack/extract may work; JSON @data needs data.pak or config");

    if (report.matchedConfigId) {
      const auto& settings = config.games.at(*report.matchedConfigId);
      matchedSettings = &settings;
      const auto configured = checkConfiguredGame(config, *report.matchedConfigId, settings);
      for (const auto& line : configured.lines) {
        if (line.status == CheckLine::Status::Info &&
            line.message.rfind("Checking configured game", 0) == 0)
          continue;
        report.lines.push_back(line);
      }
      report.gameId = report.matchedConfigId;
    }

    report.level = computeLevel(report);
    if (report.level != SupportLevel::Supported)
      appendPartialDetails(report, probe, config, matchedSettings);
    return report;
  }

  const auto configId = findConfigGameIdByName(config, gameIdOrPath);
  if (!configId) {
    GameCheckReport report;
    report.gameId = std::string(gameIdOrPath);
    if (config.games.empty())
      addLine(report, CheckLine::Status::Fail,
              "No utool.json found and game \"" + *report.gameId +
                  "\" is not a filesystem path");
    else {
      addLine(report, CheckLine::Status::Fail,
              "Game \"" + *report.gameId + "\" not found in utool.json");
      std::string known = "Configured games:";
      for (const auto& [key, value] : config.games) {
        (void)value;
        known += " " + key;
      }
      addLine(report, CheckLine::Status::Info, known);
    }
    report.level = SupportLevel::Unsupported;
    return report;
  }

  return checkConfiguredGame(config, *configId, config.games.at(*configId));
}

std::vector<GameCheckReport> checkAllConfiguredGames(const Config& config) {
  std::vector<GameCheckReport> reports;
  if (config.games.empty()) {
    GameCheckReport report;
    addLine(report, CheckLine::Status::Fail,
            "No utool.json found or \"games\" section is empty");
    report.level = SupportLevel::Unsupported;
    reports.push_back(std::move(report));
    return reports;
  }

  std::vector<std::string> ids;
  ids.reserve(config.games.size());
  for (const auto& [key, value] : config.games) {
    (void)value;
    ids.push_back(key);
  }
  std::sort(ids.begin(), ids.end());

  for (const auto& id : ids)
    reports.push_back(checkConfiguredGame(config, id, config.games.at(id)));
  return reports;
}

void printGameCheckReport(const GameCheckReport& report, std::ostream& out) {
  if (report.gameId)
    out << "Game:     " << *report.gameId << '\n';
  if (report.queriedPath)
    out << "Path:     " << report.queriedPath->string() << '\n';
  out << "Status:   " << supportLabel(report.level) << "\n\n";

  for (const auto& line : report.lines)
    out << "  [" << statusLabel(line.status) << "]  " << line.message << '\n';

  if (!report.details.empty()) {
    out << '\n';
    for (const auto& line : report.details)
      out << "  [" << statusLabel(line.status) << "]  " << line.message << '\n';
  }
}

int exitCodeForSupport(SupportLevel level) {
  return level == SupportLevel::Supported ? 0 : 1;
}

}  // namespace UTool::Core
