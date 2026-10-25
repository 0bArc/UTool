#include "UTool/Core/Config.hpp"
#include "UTool/Core/StringUtil.hpp"
#include "UTool/Lua/Host.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <vector>

namespace UTool::Core {
namespace {

std::optional<std::string> optString(const nlohmann::json& j, const char* key) {
  if (!j.contains(key) || j[key].is_null())
    return std::nullopt;
  if (j[key].is_string())
    return j[key].get<std::string>();
  return std::nullopt;
}

std::vector<std::string> stringArray(const nlohmann::json& j, const char* key) {
  std::vector<std::string> out;
  if (!j.contains(key) || !j[key].is_array())
    return out;
  for (const auto& item : j[key]) {
    if (item.is_string())
      out.push_back(item.get<std::string>());
  }
  return out;
}

GameSettings parseGame(const nlohmann::json& j) {
  GameSettings g;
  g.paksDir = optString(j, "paksDir");
  g.dataPak = optString(j, "dataPak");
  g.playerDataDir = optString(j, "playerDataDir");
  g.mountPoint = optString(j, "mountPoint");
  g.pakAesKey = optString(j, "pakAesKey");
  return g;
}

Config parseConfig(const nlohmann::json& j, std::filesystem::path configDir,
                   std::filesystem::path configFile) {
  Config cfg;
  cfg.configDirectory = std::move(configDir);
  cfg.configFilePath = std::move(configFile);
  cfg.unrealPak = optString(j, "unrealPak");
  cfg.gamePaksDir = optString(j, "gamePaksDir");
  cfg.dataPak = optString(j, "dataPak");
  cfg.defaultMountPoint = optString(j, "defaultMountPoint");
  cfg.playerDataDir = optString(j, "playerDataDir");
  cfg.extractedDir = optString(j, "extractedDir");
  cfg.unrealEngineDir = optString(j, "unrealEngineDir");
  cfg.pakAesKey = optString(j, "pakAesKey");
  cfg.updateVersion = optString(j, "updateVersion");
  if (!cfg.updateVersion && j.contains("updateVersion") && j["updateVersion"].is_number_integer())
    cfg.updateVersion = std::to_string(j["updateVersion"].get<std::int64_t>());
  cfg.lastShippedWeek = optString(j, "lastShippedWeek");
  if (!cfg.lastShippedWeek && j.contains("lastShippedWeek") && j["lastShippedWeek"].is_number_integer())
    cfg.lastShippedWeek = std::to_string(j["lastShippedWeek"].get<std::int64_t>());
  cfg.legacyIcarusPaksDir = optString(j, "icarusPaksDir");
  cfg.legacyIcarusDataPak = optString(j, "icarusDataPak");
  cfg.legacyIcarusMountPoint = optString(j, "icarusMountPoint");
  cfg.legacyIcarusPlayerDataDir = optString(j, "icarusPlayerDataDir");
  cfg.legacyDemoExtractedDir = optString(j, "demoExtractedDir");

  if (j.contains("games") && j["games"].is_object()) {
    for (auto it = j["games"].begin(); it != j["games"].end(); ++it) {
      if (it.value().is_object())
        cfg.games[it.key()] = parseGame(it.value());
    }
  }

  if (j.contains("nexus") && j["nexus"].is_object()) {
    const auto& n = j["nexus"];
    NexusSettings nx;
    nx.gameDomain = optString(n, "gameDomain");
    if (n.contains("modId") && n["modId"].is_number_integer())
      nx.modId = n["modId"].get<std::int64_t>();
    if (auto env = optString(n, "apiKeyEnv"))
      nx.apiKeyEnv = *env;
    nx.apiBase = optString(n, "apiBase");
    if (n.contains("files") && n["files"].is_array()) {
      for (const auto& f : n["files"]) {
        if (!f.is_object())
          continue;
        NexusFileEntry e;
        e.id = f.value("id", "");
        if (f.contains("modId") && f["modId"].is_number_integer())
          e.modId = f["modId"].get<std::int64_t>();
        if (f.contains("fileId") && f["fileId"].is_number_integer())
          e.fileId = f["fileId"].get<std::int64_t>();
        e.match = f.value("match", "");
        e.displayName = optString(f, "displayName");
        e.category = f.value("category", "main");
        if (f.contains("archiveExisting") && f["archiveExisting"].is_boolean())
          e.archiveExisting = f["archiveExisting"].get<bool>();
        if (f.contains("updateModVersion") && f["updateModVersion"].is_boolean())
          e.updateModVersion = f["updateModVersion"].get<bool>();
        nx.files.push_back(std::move(e));
      }
    }
    // Optional grouped form: nexus.mods[{modId, files:[...]}]
    if (n.contains("mods") && n["mods"].is_array()) {
      for (const auto& m : n["mods"]) {
        if (!m.is_object() || !m.contains("modId") || !m["modId"].is_number_integer())
          continue;
        const auto mid = m["modId"].get<std::int64_t>();
        if (!m.contains("files") || !m["files"].is_array())
          continue;
        for (const auto& f : m["files"]) {
          if (!f.is_object())
            continue;
          NexusFileEntry e;
          e.id = f.value("id", "");
          e.modId = mid;
          if (f.contains("fileId") && f["fileId"].is_number_integer())
            e.fileId = f["fileId"].get<std::int64_t>();
          e.match = f.value("match", "");
          e.displayName = optString(f, "displayName");
          e.category = f.value("category", "main");
          if (f.contains("archiveExisting") && f["archiveExisting"].is_boolean())
            e.archiveExisting = f["archiveExisting"].get<bool>();
          if (f.contains("updateModVersion") && f["updateModVersion"].is_boolean())
            e.updateModVersion = f["updateModVersion"].get<bool>();
          nx.files.push_back(std::move(e));
        }
      }
    }
    cfg.nexus = std::move(nx);
  }

  if (j.contains("catalog") && j["catalog"].is_array()) {
    for (const auto& c : j["catalog"]) {
      if (!c.is_object())
        continue;
      CatalogEntry e;
      e.id = c.value("id", "");
      e.path = c.value("path", "");
      e.category = c.value("category", "");
      if (c.contains("ship") && c["ship"].is_boolean())
        e.ship = c["ship"].get<bool>();
      e.bannerTitle = optString(c, "bannerTitle");
      e.bannerTagline = optString(c, "bannerTagline");
      if (e.id.empty() || e.path.empty())
        continue;
      cfg.catalog.push_back(std::move(e));
    }
  }
  return cfg;
}

const GameSettings* findGame(const Config& cfg, const std::optional<std::string>& gameId) {
  if (!gameId || gameId->empty())
    return nullptr;
  for (const auto& [key, value] : cfg.games) {
    if (iequals(key, *gameId))
      return &value;
  }
  return nullptr;
}

ModPakSettings parsePak(const nlohmann::json& j) {
  ModPakSettings p;
  p.output = optString(j, "output");
  p.mountPoint = optString(j, "mountPoint");
  p.sourcePak = optString(j, "sourcePak");
  p.curveSourcePak = optString(j, "curveSourcePak");
  p.sourceFilter = optString(j, "sourceFilter");
  if (j.contains("useUnrealPak") && j["useUnrealPak"].is_boolean())
    p.useUnrealPak = j["useUnrealPak"].get<bool>();
  if (j.contains("keepCache") && j["keepCache"].is_boolean())
    p.keepCache = j["keepCache"].get<bool>();
  if (j.contains("zip")) {
    if (j["zip"].is_boolean())
      p.zip = j["zip"].get<bool>();
    else if (j["zip"].is_string()) {
      p.zip = true;
      p.zipTemplate = j["zip"].get<std::string>();
    }
  }
  return p;
}

Ue4Target parseTarget(const nlohmann::json& j) {
  Ue4Target t;
  t.gameId = optString(j, "gameId");
  t.engineVersion = optString(j, "engineVersion");
  t.minGameVersion = optString(j, "minGameVersion");
  t.maxGameVersion = optString(j, "maxGameVersion");
  return t;
}

ModManifest parseManifest(const nlohmann::json& j) {
  ModManifest m;
  m.id = j.value("id", "");
  m.name = j.value("name", "");
  m.version = j.value("version", "");
  m.description = optString(j, "description");
  m.author = optString(j, "author");
  if (j.contains("target") && j["target"].is_object())
    m.target = parseTarget(j["target"]);
  m.contentRoots = stringArray(j, "contentRoots");
  if (m.contentRoots.empty())
    m.contentRoots = {"content"};
  m.patchFiles = stringArray(j, "patchFiles");
  m.scripts = stringArray(j, "scripts");
  m.curvePatchesDir = optString(j, "curvePatchesDir");
  m.updateVersion = optString(j, "updateVersion");
  if (!m.updateVersion && j.contains("updateVersion") && j["updateVersion"].is_number_integer())
    m.updateVersion = std::to_string(j["updateVersion"].get<std::int64_t>());
  if (j.contains("pak") && j["pak"].is_object())
    m.pak = parsePak(j["pak"]);
  return m;
}

nlohmann::json readJsonFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("Failed to open " + path.string());
  nlohmann::json j;
  in >> j;
  return j;
}

std::optional<int> compareDottedVersions(std::string_view a, std::string_view b) {
  auto parse = [](std::string_view s) -> std::optional<std::vector<int>> {
    if (s.empty())
      return std::nullopt;
    std::vector<int> parts;
    int current = -1;
    for (char c : s) {
      if (c >= '0' && c <= '9') {
        if (current < 0)
          current = 0;
        if (current > 100000000)
          return std::nullopt;
        current = current * 10 + (c - '0');
      } else if (c == '.') {
        if (current < 0)
          return std::nullopt;
        parts.push_back(current);
        current = -1;
      } else {
        return std::nullopt;
      }
    }
    if (current < 0)
      return std::nullopt;
    parts.push_back(current);
    return parts;
  };

  const auto left = parse(a);
  const auto right = parse(b);
  if (!left || !right)
    return std::nullopt;
  const size_t n = std::max(left->size(), right->size());
  for (size_t i = 0; i < n; ++i) {
    const int av = i < left->size() ? (*left)[i] : 0;
    const int bv = i < right->size() ? (*right)[i] : 0;
    if (av < bv)
      return -1;
    if (av > bv)
      return 1;
  }
  return 0;
}

}  // namespace

bool Config::isDataPakAlias(std::string_view token) {
  return iequals(token, "@data") || iequals(token, "@game-data") ||
         iequals(token, "@config:data") || iequals(token, "@icarus-data");
}

bool Config::isPaksDirAlias(std::string_view token) {
  return iequals(token, "@paks") || iequals(token, "@game-paks") ||
         iequals(token, "@config:paks") || iequals(token, "@icarus");
}

Config Config::load(const std::filesystem::path& startDirectory) {
  namespace fs = std::filesystem;

  if (const char* env = std::getenv("UTOOL_CONFIG_DIR")) {
    if (*env) {
      const fs::path dir(env);
      for (const char* name : {"utool.json", "csstratware.json"}) {
        const auto path = dir / name;
        if (fs::is_regular_file(path))
          return parseConfig(readJsonFile(path), dir, path);
      }
    }
  }

  fs::path dir = startDirectory.empty() ? fs::current_path() : startDirectory;
  for (int i = 0; i < 8 && !dir.empty(); ++i) {
    for (const char* name : {"utool.json", "csstratware.json"}) {
      const auto path = dir / name;
      if (!fs::is_regular_file(path))
        continue;
      return parseConfig(readJsonFile(path), dir, path);
    }
    if (!dir.has_parent_path() || dir.parent_path() == dir)
      break;
    dir = dir.parent_path();
  }
  return Config{};
}

std::filesystem::path Config::resolvePath(const std::string& path) const {
  namespace fs = std::filesystem;
  const fs::path p(path);
  if (p.is_absolute())
    return fs::weakly_canonical(p);
  const auto base = configDirectory.empty() ? fs::current_path() : configDirectory;
  return fs::weakly_canonical(base / p);
}

std::optional<std::filesystem::path> Config::resolvePaksDir(
    const std::optional<std::string>& gameId) const {
  const auto* game = findGame(*this, gameId);
  const auto* dir = game && game->paksDir ? &*game->paksDir
                    : gamePaksDir         ? &*gamePaksDir
                    : legacyIcarusPaksDir ? &*legacyIcarusPaksDir
                                          : nullptr;
  if (!dir || dir->empty())
    return std::nullopt;
  return resolvePath(*dir);
}

std::filesystem::path Config::resolveDataPak(const std::optional<std::string>& gameId) const {
  const auto* game = findGame(*this, gameId);
  const auto* pak = game && game->dataPak ? &*game->dataPak
                    : dataPak             ? &*dataPak
                    : legacyIcarusDataPak ? &*legacyIcarusDataPak
                                          : nullptr;
  if (pak && !pak->empty())
    return resolvePath(*pak);

  if (auto paks = resolvePaksDir(gameId))
    return std::filesystem::weakly_canonical(*paks / ".." / "Data" / "data.pak");

  throw std::runtime_error(
      "dataPak not configured. Set dataPak or gamePaksDir in utool.json, "
      "or mod.json pak.sourcePak to a file path.");
}

std::optional<std::string> Config::resolveMountPoint(
    const std::optional<std::string>& gameId) const {
  const auto* game = findGame(*this, gameId);
  if (game && game->mountPoint)
    return game->mountPoint;
  if (defaultMountPoint)
    return defaultMountPoint;
  return legacyIcarusMountPoint;
}

std::optional<std::filesystem::path> Config::resolveExtractedDir() const {
  const auto* dir = extractedDir             ? &*extractedDir
                    : legacyDemoExtractedDir ? &*legacyDemoExtractedDir
                                             : nullptr;
  if (!dir || dir->empty())
    return std::nullopt;
  return resolvePath(*dir);
}

std::optional<std::filesystem::path> Config::resolveExistingExtractedDir() const {
  auto dir = resolveExtractedDir();
  if (!dir || !std::filesystem::is_directory(*dir))
    return std::nullopt;
  return dir;
}

std::optional<std::filesystem::path> Config::resolveSourcePak(
    const std::optional<std::string>& token,
    const std::optional<std::string>& gameId) const {
  if (!token || token->empty())
    return std::nullopt;
  if (isDataPakAlias(*token))
    return resolveDataPak(gameId);
  return resolvePath(*token);
}

std::vector<std::filesystem::path> Config::resolveSourcePakPaths(
    const std::optional<std::string>& token,
    const std::optional<std::string>& gameId) const {
  std::vector<std::filesystem::path> out;
  if (!token || token->empty())
    return out;

  if (isPaksDirAlias(*token)) {
    auto paksDir = resolvePaksDir(gameId);
    if (!paksDir)
      throw std::runtime_error("paksDir not configured for @paks alias.");
    for (const auto& entry : std::filesystem::directory_iterator(*paksDir)) {
      if (!entry.is_regular_file())
        continue;
      if (toLower(entry.path().extension().string()) == ".pak")
        out.push_back(entry.path());
    }
    std::sort(out.begin(), out.end());
    return out;
  }

  if (auto single = resolveSourcePak(token, gameId))
    out.push_back(*single);
  return out;
}

ModPackage loadModPackage(const std::filesystem::path& modDir) {
  const auto luaPath = modDir / ModManifest::LuaManifestFileName;
  const auto jsonPath = modDir / ModManifest::ManifestFileName;

  ModPackage pkg;
  pkg.rootPath = std::filesystem::weakly_canonical(modDir);

  if (std::filesystem::is_regular_file(luaPath)) {
    auto regs = Lua::loadModScripts({luaPath});
    if (!regs.modManifest)
      throw std::runtime_error("mod.lua must call utool.mod { ... }: " + luaPath.string());
    pkg.manifest = *regs.modManifest;
    if (pkg.manifest.id.empty() || pkg.manifest.name.empty())
      throw std::runtime_error("utool.mod requires id and name: " + luaPath.string());
    if (pkg.manifest.version.empty())
      pkg.manifest.version = "1.0.0";
    return pkg;
  }

  if (!std::filesystem::is_regular_file(jsonPath))
    throw std::runtime_error("No mod.lua or mod.json in " + modDir.string());

  pkg.manifest = parseManifest(readJsonFile(jsonPath));
  if (pkg.manifest.id.empty() || pkg.manifest.name.empty() || pkg.manifest.version.empty())
    throw std::runtime_error("mod.json requires id, name, and version: " + jsonPath.string());
  return pkg;
}

std::vector<ModPackage> discoverMods(const std::filesystem::path& modsDir) {
  std::vector<ModPackage> mods;
  if (!std::filesystem::is_directory(modsDir))
    return mods;

  const auto hasManifest = [](const std::filesystem::path& dir) {
    return std::filesystem::is_regular_file(dir / ModManifest::LuaManifestFileName) ||
           std::filesystem::is_regular_file(dir / ModManifest::ManifestFileName);
  };

  const auto skipDirName = [](const std::filesystem::path& name) {
    const auto s = name.string();
    return s == "dist" || s == ".cache" || s == "media" || s == "content" || s == "scripts" ||
           s == "patches" || s == ".extracted" || s == "extracted";
  };

  if (hasManifest(modsDir)) {
    mods.push_back(loadModPackage(modsDir));
    return mods;
  }

  for (auto it = std::filesystem::recursive_directory_iterator(
           modsDir, std::filesystem::directory_options::skip_permission_denied);
       it != std::filesystem::recursive_directory_iterator(); ++it) {
    if (!it->is_directory())
      continue;
    if (skipDirName(it->path().filename())) {
      it.disable_recursion_pending();
      continue;
    }
    if (!hasManifest(it->path()))
      continue;
    try {
      mods.push_back(loadModPackage(it->path()));
    } catch (const std::exception& ex) {
      std::cerr << "skip " << it->path().string() << ": " << ex.what() << '\n';
    }
  }
  std::sort(mods.begin(), mods.end(), [](const ModPackage& a, const ModPackage& b) {
    return a.manifest.id < b.manifest.id;
  });
  return mods;
}

std::vector<ModPackage> discoverShipMods(const std::filesystem::path& modsDir,
                                         const Config& config) {
  if (config.catalog.empty())
    return discoverMods(modsDir);

  std::vector<ModPackage> mods;
  for (const auto& entry : config.catalog) {
    if (!entry.ship)
      continue;
    const auto root = modsDir / entry.path;
    try {
      mods.push_back(loadModPackage(root));
    } catch (const std::exception& ex) {
      throw std::runtime_error("catalog ship entry '" + entry.id + "' (" + entry.path +
                               "): " + ex.what());
    }
  }
  return mods;
}

std::vector<std::string> validateMod(const ModPackage& package) {
  std::vector<std::string> issues;
  if (package.manifest.id.empty())
    issues.emplace_back("missing id");
  if (package.manifest.name.empty())
    issues.emplace_back("missing name");
  if (package.manifest.version.empty())
    issues.emplace_back("missing version");

  for (const auto& patch : package.manifest.patchFiles) {
    if (!std::filesystem::is_regular_file(package.rootPath / patch))
      issues.push_back("missing patchFile: " + patch);
  }
  for (const auto& script : package.manifest.scripts) {
    if (!std::filesystem::is_regular_file(package.rootPath / script))
      issues.push_back("missing script: " + script);
  }
  if (package.manifest.pak && package.manifest.pak->mountPoint &&
      package.manifest.pak->mountPoint->empty()) {
    issues.emplace_back("pak.mountPoint is empty");
  }
  if (package.manifest.target && package.manifest.target->minGameVersion &&
      package.manifest.target->maxGameVersion) {
    const auto order = compareDottedVersions(*package.manifest.target->minGameVersion,
                                              *package.manifest.target->maxGameVersion);
    if (!order)
      issues.emplace_back("target.minGameVersion and maxGameVersion must be dotted numbers");
    else if (*order > 0)
      issues.emplace_back("target.minGameVersion is greater than maxGameVersion");
  }
  return issues;
}

std::optional<std::filesystem::path> findRepoRoot(const std::filesystem::path& start) {
  namespace fs = std::filesystem;
  fs::path dir = start.empty() ? fs::current_path() : start;
  for (int i = 0; i < 14 && !dir.empty(); ++i) {
    if (fs::is_regular_file(dir / "CMakeLists.txt") &&
        (fs::is_directory(dir / "assets") || fs::is_directory(dir / "include" / "UTool")))
      return fs::weakly_canonical(dir);
    if (fs::is_regular_file(dir / "assets" / "UnrealPak.zip"))
      return fs::weakly_canonical(dir);
    if (!dir.has_parent_path() || dir.parent_path() == dir)
      break;
    dir = dir.parent_path();
  }
  return std::nullopt;
}

std::optional<std::string> resolveUpdateVersion(
    const Config& config,
    const ModManifest& manifest,
    const std::optional<std::string>& cliOverride) {
  if (cliOverride && !cliOverride->empty())
    return *cliOverride;
  if (manifest.updateVersion && !manifest.updateVersion->empty())
    return *manifest.updateVersion;
  if (config.updateVersion && !config.updateVersion->empty())
    return *config.updateVersion;
  return std::nullopt;
}

void writeConfigUpdateVersion(Config& config, const std::string& updateVersion) {
  if (config.configFilePath.empty())
    throw std::runtime_error("No utool.json found to write updateVersion");
  auto j = readJsonFile(config.configFilePath);
  if (updateVersion.find_first_not_of("0123456789") == std::string::npos)
    j["updateVersion"] = std::stoll(updateVersion);
  else
    j["updateVersion"] = updateVersion;
  std::ofstream out(config.configFilePath);
  if (!out)
    throw std::runtime_error("Failed to write " + config.configFilePath.string());
  out << j.dump(2) << '\n';
  config.updateVersion = updateVersion;
}

void writeConfigLastShippedWeek(Config& config, const std::string& week) {
  if (config.configFilePath.empty())
    throw std::runtime_error("No utool.json found to write lastShippedWeek");
  auto j = readJsonFile(config.configFilePath);
  if (week.find_first_not_of("0123456789") == std::string::npos)
    j["lastShippedWeek"] = std::stoll(week);
  else
    j["lastShippedWeek"] = week;
  std::ofstream out(config.configFilePath);
  if (!out)
    throw std::runtime_error("Failed to write " + config.configFilePath.string());
  out << j.dump(2) << '\n';
  config.lastShippedWeek = week;
}

bool rewriteModLuaUpdateVersion(
    const std::filesystem::path& modLuaPath,
    const std::string& updateVersion) {
  std::ifstream in(modLuaPath);
  if (!in)
    return false;
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();

  const std::string replacement = "updateVersion = " + updateVersion + ",";
  bool changed = false;

  // Replace existing updateVersion = <number|string>
  std::string out;
  out.reserve(content.size() + 16);
  std::size_t i = 0;
  while (i < content.size()) {
    const auto pos = content.find("updateVersion", i);
    if (pos == std::string::npos) {
      out.append(content, i, std::string::npos);
      break;
    }
    out.append(content, i, pos - i);
    std::size_t j = pos + std::string("updateVersion").size();
    while (j < content.size() && (content[j] == ' ' || content[j] == '\t'))
      ++j;
    if (j >= content.size() || content[j] != '=') {
      out.append(content, pos, std::string("updateVersion").size());
      i = pos + std::string("updateVersion").size();
      continue;
    }
    ++j;
    while (j < content.size() && (content[j] == ' ' || content[j] == '\t'))
      ++j;
    // skip value
    if (j < content.size() && (content[j] == '"' || content[j] == '\'')) {
      const char q = content[j++];
      while (j < content.size() && content[j] != q)
        ++j;
      if (j < content.size())
        ++j;
    } else {
      while (j < content.size() &&
             (std::isdigit(static_cast<unsigned char>(content[j])) || content[j] == '.' ||
              content[j] == '-' || content[j] == '+'))
        ++j;
    }
    while (j < content.size() && (content[j] == ' ' || content[j] == '\t'))
      ++j;
    if (j < content.size() && content[j] == ',')
      ++j;
    out += replacement;
    changed = true;
    i = j;
  }

  if (!changed) {
    // Insert after utool.mod { line if %updateversion% appears in file
    if (content.find("%updateversion%") == std::string::npos &&
        content.find("%updateVersion%") == std::string::npos)
      return false;
    const auto brace = content.find("utool.mod");
    if (brace == std::string::npos)
      return false;
    const auto open = content.find('{', brace);
    if (open == std::string::npos)
      return false;
    out = content;
    out.insert(open + 1, "\n  " + replacement + "\n");
    changed = true;
  }

  if (!changed)
    return false;
  std::ofstream file(modLuaPath);
  if (!file)
    return false;
  file << out;
  return true;
}

}  // namespace UTool::Core
