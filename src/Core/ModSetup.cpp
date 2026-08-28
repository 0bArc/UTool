#include "UTool/Core/ModSetup.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
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

std::string slugToken(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  bool prevUnderscore = false;
  for (char c : input) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      prevUnderscore = false;
    } else if (!prevUnderscore) {
      out += '_';
      prevUnderscore = true;
    }
  }
  while (!out.empty() && out.back() == '_')
    out.pop_back();
  return out.empty() ? "mod" : out;
}

std::string defaultModId(std::string_view gameId) {
  return slugToken(gameId) + ".example_mod";
}

std::string defaultModName(std::string_view gameId) {
  return std::string("Example ") + std::string(gameId) + " Mod";
}

std::string projectNameFromPaksDir(const std::filesystem::path& paksDir) {
  const auto content = paksDir.parent_path();
  if (iequals(content.filename().string(), "Content"))
    return content.parent_path().filename().string();
  return paksDir.filename().string();
}

std::string resolveAutoMountPointImpl(const Config& config, const std::optional<std::string>& gameId) {
  if (auto mount = config.resolveMountPoint(gameId))
    return *mount;

  if (gameId && !gameId->empty()) {
    const ProbedInstall probe = probeFromConfig(config, *gameId);
    if (probe.paksDir) {
      const std::string project = projectNameFromPaksDir(*probe.paksDir);
      return "../../../" + project + "/Content/";
    }
    if (iequals(*gameId, "Icarus"))
      return "../../../Icarus/Content/";
  }

  throw std::runtime_error(
      "Could not resolve mountPoint @auto: set games.*.mountPoint or defaultMountPoint in "
      "utool.json, or configure paksDir for the target game.");
}

std::string pakOutputFile(std::string_view modId, std::string_view gameId) {
  const std::string base = slugToken(modId);
  if (iequals(gameId, "Icarus"))
    return "dist/" + base + "_P.pak";
  return "dist/" + base + ".pak";
}

std::string luaString(std::string_view value) {
  std::string out = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"')
      out += '\\';
    out += c;
  }
  out += '"';
  return out;
}

bool probeHasViablePaks(const ProbedInstall& probe) {
  return probe.singlePakFile || (probe.paksDir && probe.pakCount > 0);
}

std::string suggestGameIdFromProbe(const ProbedInstall& probe) {
  if (probe.paksDir) {
    std::string name = projectNameFromPaksDir(*probe.paksDir);
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

std::optional<std::string> resolveGameId(
    const Config& config,
    std::string_view target,
    const GameCheckReport& report,
    const ProbedInstall& probe) {
  if (report.matchedConfigId)
    return *report.matchedConfigId;
  if (report.gameId)
    return *report.gameId;
  if (looksLikeFilesystemTarget(target))
    return suggestGameIdFromProbe(probe);
  return findConfigGameIdByName(config, target);
}

std::string defaultEngineVersion(std::string_view gameId) {
  if (iequals(gameId, "Icarus"))
    return "4.27";
  return "5.4";
}

void appendExamplePatch(std::ostringstream& out, bool hasDataPak, bool hasPaks) {
  out << "\n";
  if (hasDataPak) {
    out << "-- JSON table patch (requires @data in utool.json):\n";
    out << "-- utool.asset(\"D_Example.json\")\n";
    out << "--   :row(\"RowName\")\n";
    out << "--   :field(\"Property\")\n";
    out << "--   :set(0)\n";
  } else if (hasPaks) {
    out << "-- No data.pak: use content/ files or curve patches instead of utool.asset.\n";
    out << "-- utool.patch_curve(\"C_ExampleCurve\", \"Data/Path\", function()\n";
    out << "--   local last = utool.curve:LastKey()\n";
    out << "--   utool.curve:AddKey(last.Time + 1, last.Value + 100)\n";
    out << "-- end)\n";
  } else {
    out << "-- Add patches under content/ or scripts/ once pak sources are configured.\n";
  }
}

}  // namespace

ProbedInstall probeFromConfig(const Config& config, std::string_view gameId) {
  ProbedInstall probe;
  const auto id = findConfigGameIdByName(config, gameId);
  if (!id)
    return probe;

  const GameSettings& settings = config.games.at(*id);
  std::error_code ec;

  if (settings.paksDir) {
    try {
      probe.paksDir = config.resolvePath(*settings.paksDir);
      if (probe.paksDir && std::filesystem::is_directory(*probe.paksDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(*probe.paksDir, ec)) {
          if (ec || !entry.is_regular_file())
            continue;
          if (iequals(entry.path().extension().string(), ".pak"))
            ++probe.pakCount;
        }
        probe.installRoot = probe.paksDir->parent_path().parent_path().parent_path();
      }
    } catch (...) {
    }
  }

  if (settings.dataPak) {
    try {
      const auto path = config.resolvePath(*settings.dataPak);
      if (std::filesystem::is_regular_file(path, ec))
        probe.dataPak = path;
    } catch (...) {
    }
  } else if (probe.paksDir) {
    const auto derived = probe.paksDir->parent_path().parent_path() / "Data" / "data.pak";
    if (std::filesystem::is_regular_file(derived, ec))
      probe.dataPak = derived;
  }

  return probe;
}

ModSetupResult generateModSetup(
    const Config& config,
    std::string_view gameIdOrPath,
    const ModSetupOptions& options) {
  ModSetupResult result;

  const GameCheckReport report = checkGameTarget(config, gameIdOrPath);

  ProbedInstall probe =
      looksLikeFilesystemTarget(gameIdOrPath)
          ? probeInstallPath(std::filesystem::path{std::string(gameIdOrPath)})
          : probeFromConfig(config, gameIdOrPath);

  const auto gameIdOpt = resolveGameId(config, gameIdOrPath, report, probe);
  if (!gameIdOpt) {
    result.notes.push_back("Could not determine gameId from check results.");
    result.level = SupportLevel::Unsupported;
    return result;
  }

  result.gameId = *gameIdOpt;
  result.level = report.level;

  if (!probeHasViablePaks(probe)) {
    result.notes.push_back("No .pak files found; mod.lua not generated.");
    return result;
  }

  result.viable = report.level != SupportLevel::Unsupported;

  const std::string modId = options.modId ? *options.modId : defaultModId(result.gameId);
  const std::string modName = options.modName ? *options.modName : defaultModName(result.gameId);
  const bool hasDataPak = probe.dataPak.has_value();
  const bool hasPaks = probe.paksDir && probe.pakCount > 0;
  const std::string mount = "@auto";
  const std::string output = pakOutputFile(modId, result.gameId);
  const std::string engine = defaultEngineVersion(result.gameId);

  if (result.level == SupportLevel::Partial)
    result.notes.push_back("Game is partial support; review pak.sourcePak and utool.json before building.");
  if (config.configDirectory.empty())
    result.notes.push_back("No utool.json found nearby; add a games entry before utool pak build-mod.");
  if (!report.matchedConfigId && looksLikeFilesystemTarget(gameIdOrPath))
    result.notes.push_back("Add the suggested games entry from `utool check` to utool.json.");

  std::ostringstream out;
  out << "utool.mod {\n";
  out << "  id = " << luaString(modId) << ",\n";
  out << "  name = " << luaString(modName) << ",\n";
  out << "  version = \"1.0.0\",\n";
  out << "  description = \"Generated by utool auto setup\",\n";
  out << "  author = \"utool\",\n";
  out << "  target = {\n";
  out << "    gameId = " << luaString(result.gameId) << ",\n";
  out << "    engineVersion = " << luaString(engine) << ",\n";
  out << "    minGameVersion = \"1.0.0\",\n";
  out << "  },\n";

  if (hasDataPak && hasPaks) {
    out << "  scripts = {\n";
    out << "    -- \"scripts/patches.lua\",\n";
    out << "  },\n";
  }

  out << "  pak = {\n";
  out << "    output = " << luaString(output) << ",\n";
  out << "    mountPoint = " << luaString(mount) << ",\n";

  if (hasDataPak) {
    out << "    sourcePak = \"@data\",\n";
    if (hasPaks)
      out << "    curveSourcePak = \"@paks\",\n";
  } else if (hasPaks) {
    out << "    sourcePak = \"@paks\",\n";
  }

  out << "    useUnrealPak = true,\n";
  out << "  },\n";
  out << "}\n";

  appendExamplePatch(out, hasDataPak, hasPaks);

  result.modLua = out.str();
  return result;
}

std::string resolveAutoMountPoint(const Config& config, const std::optional<std::string>& gameId) {
  return resolveAutoMountPointImpl(config, gameId);
}

}  // namespace UTool::Core
