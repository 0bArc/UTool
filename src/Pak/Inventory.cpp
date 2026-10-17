#include "UTool/Pak/Inventory.hpp"

#include "PakInternal.hpp"
#include "UTool/Core/GameCheck.hpp"
#include "UTool/Pak/StringIndex.hpp"
#include "UTool/Pak/UnrealPak.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace UTool::Pak {
namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

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

std::string cacheKeyForPak(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options) {
  std::error_code ec;
  const auto canonical = std::filesystem::weakly_canonical(pakPath, ec);
  const auto pathStr = ec ? pakPath.string() : canonical.string();
  const auto size = std::filesystem::file_size(pakPath, ec);
  const auto mtime = std::filesystem::last_write_time(pakPath, ec);
  const auto version = unrealPakVersionFingerprint(resolveExecutable(options));
  std::ostringstream oss;
  oss << pathStr << '|' << size << '|' << mtime.time_since_epoch().count() << '|' << version;
  return oss.str();
}

std::string hashCacheKey(std::string_view key) {
  std::size_t h = std::hash<std::string_view>{}(key);
  std::ostringstream oss;
  oss << std::hex << h;
  return oss.str();
}

std::filesystem::path pakIndexCacheDir() {
  return std::filesystem::path(utoolStoreRoot()) / "cache" / "pak-index";
}

std::optional<std::string> resolveAesKey(
    const Core::Config& config,
    const std::optional<std::string>& gameId) {
  if (gameId) {
    const auto it = config.games.find(*gameId);
    if (it != config.games.end() && it->second.pakAesKey)
      return *it->second.pakAesKey;
  }
  return config.pakAesKey;
}

UnrealPakOptions withCrypto(const Core::Config& config, UnrealPakOptions options,
                            const std::optional<std::string>& gameId) {
  if (const auto key = resolveAesKey(config, gameId)) {
    const std::string cacheId = gameId ? *gameId : "global";
    options.cryptoKeysPath = ensureCryptoJson(*key, cacheId);
  }
  return options;
}

PakFileEntry toEntry(const ParsedListEntry& parsed, std::string_view sourcePak) {
  PakFileEntry entry;
  entry.sourcePak = std::string(sourcePak);
  entry.virtualPath = parsed.virtualPath;
  entry.size = parsed.size;
  entry.offset = parsed.offset;
  entry.extension = fileExtensionLower(parsed.virtualPath);
  return entry;
}

nlohmann::json entryToJson(const PakFileEntry& e) {
  return nlohmann::json{
      {"sourcePak", e.sourcePak},
      {"virtualPath", e.virtualPath},
      {"size", e.size},
      {"offset", e.offset},
      {"extension", e.extension},
  };
}

PakFileEntry entryFromJson(const nlohmann::json& j) {
  PakFileEntry e;
  e.sourcePak = j.value("sourcePak", "");
  e.virtualPath = j.value("virtualPath", "");
  e.size = j.value("size", static_cast<std::uint64_t>(0));
  e.offset = j.value("offset", static_cast<std::uint64_t>(0));
  e.extension = j.value("extension", "");
  return e;
}

std::optional<std::vector<PakFileEntry>> loadPakCache(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options,
    std::string_view sourcePak) {
  const auto cacheFile = pakIndexCacheDir() / (hashCacheKey(cacheKeyForPak(pakPath, options)) + ".json");
  std::error_code ec;
  if (!std::filesystem::is_regular_file(cacheFile, ec))
    return std::nullopt;

  std::ifstream in(cacheFile);
  if (!in)
    return std::nullopt;

  nlohmann::json doc;
  try {
    in >> doc;
  } catch (...) {
    return std::nullopt;
  }

  if (doc.value("cacheKey", "") != cacheKeyForPak(pakPath, options))
    return std::nullopt;

  std::vector<PakFileEntry> entries;
  if (!doc.contains("entries") || !doc["entries"].is_array())
    return std::nullopt;
  for (const auto& item : doc["entries"])
    entries.push_back(entryFromJson(item));
  for (auto& e : entries) {
    if (e.sourcePak.empty())
      e.sourcePak = std::string(sourcePak);
  }
  return entries;
}

void savePakCache(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options,
    std::string_view sourcePak,
    const std::vector<PakFileEntry>& entries) {
  const auto cacheDir = pakIndexCacheDir();
  std::filesystem::create_directories(cacheDir);
  const auto cacheFile = cacheDir / (hashCacheKey(cacheKeyForPak(pakPath, options)) + ".json");

  nlohmann::json doc;
  doc["cacheKey"] = cacheKeyForPak(pakPath, options);
  doc["sourcePak"] = sourcePak;
  doc["pakPath"] = std::filesystem::weakly_canonical(pakPath).string();
  doc["entries"] = nlohmann::json::array();
  for (const auto& e : entries)
    doc["entries"].push_back(entryToJson(e));

  const auto temp = cacheDir / (cacheFile.filename().string() + ".tmp");
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << doc.dump();
    out.flush();
  }
  std::error_code ec;
  std::filesystem::rename(temp, cacheFile, ec);
  if (ec) {
    std::filesystem::remove(cacheFile, ec);
    ec.clear();
    std::filesystem::rename(temp, cacheFile, ec);
  }
}

std::vector<PakFileEntry> listPakEntriesOnDisk(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options,
    std::string_view sourcePak,
    bool& fromCache) {
  fromCache = false;
  if (auto cached = loadPakCache(pakPath, options, sourcePak)) {
    fromCache = true;
    return *cached;
  }

  const auto capture = listPakCapture(pakPath, options);
  if (capture.exitCode != 0) {
    std::string msg = "UnrealPak -List failed for " + pakPath.filename().string();
    if (!capture.stderrText.empty())
      msg += ": " + capture.stderrText.substr(0, 200);
    throw std::runtime_error(msg);
  }

  const auto parsed = parsePakListOutput(capture.stdoutText + capture.stderrText);
  std::vector<PakFileEntry> entries;
  entries.reserve(parsed.size());
  for (const auto& p : parsed)
    entries.push_back(toEntry(p, sourcePak));

  savePakCache(pakPath, options, sourcePak, entries);
  return entries;
}

std::vector<std::filesystem::path> collectPakFiles(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> paks;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec))
    return paks;

  const auto maybeAddPak = [&](const std::filesystem::path& path) {
    if (lower(path.extension().string()) == ".pak")
      paks.push_back(path);
  };

  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (entry.is_regular_file()) {
      maybeAddPak(entry.path());
      continue;
    }
    if (!entry.is_directory())
      continue;
    for (const auto& nested : std::filesystem::directory_iterator(entry.path(), ec)) {
      if (nested.is_regular_file())
        maybeAddPak(nested.path());
    }
  }

  std::sort(paks.begin(), paks.end());
  return paks;
}

const Core::GameSettings* findGameCaseInsensitive(
    const Core::Config& config,
    std::string_view gameId) {
  for (const auto& [id, settings] : config.games) {
    if (iequals(id, gameId))
      return &settings;
  }
  return nullptr;
}

std::optional<std::string> findGameIdCaseInsensitive(
    const Core::Config& config,
    std::string_view gameId) {
  for (const auto& [id, settings] : config.games) {
    (void)settings;
    if (iequals(id, gameId))
      return id;
  }
  return std::nullopt;
}

[[noreturn]] void throwMissingPaksDir(std::string_view gameId) {
  throw std::runtime_error(
      std::string("Game \"") + std::string(gameId) + "\" has no configured paksDir.\n\n"
      "Configure in utool.json:\n"
      "  \"games\": {\n"
      "    \"" + std::string(gameId) + "\": {\n"
      "      \"paksDir\": \"D:/Games/YourGame/Content/Paks\"\n"
      "    }\n"
      "  }");
}

struct ResolvedSource {
  std::string label;
  std::vector<std::filesystem::path> pakPaths;
  std::optional<std::string> gameId;
};

ResolvedSource resolveSourceSpec(
    std::string_view source,
    const ResolveContext& ctx) {
  const std::string spec(source);
  std::error_code ec;

  const auto asPath = std::filesystem::path(spec);
  if (std::filesystem::is_regular_file(asPath, ec) && lower(asPath.extension().string()) == ".pak") {
    return ResolvedSource{
        .label = asPath.filename().string(),
        .pakPaths = {std::filesystem::weakly_canonical(asPath, ec)},
    };
  }

  if (std::filesystem::is_directory(asPath, ec)) {
    auto paks = collectPakFiles(asPath);
    if (paks.empty())
      throw std::runtime_error("No .pak files found in directory: " + spec);
    return ResolvedSource{
        .label = spec,
        .pakPaths = std::move(paks),
    };
  }

  if (Core::Config::isDataPakAlias(spec)) {
    const auto gid = ctx.gameId;
    try {
      const auto dataPak = ctx.config.resolveDataPak(gid);
      return ResolvedSource{
          .label = spec,
          .pakPaths = {dataPak},
          .gameId = gid,
      };
    } catch (const std::exception&) {
      throw std::runtime_error(
          "dataPak not configured for @data.\n\n"
          "Configure dataPak or games.<id>.dataPak in utool.json.");
    }
  }

  if (Core::Config::isPaksDirAlias(spec)) {
    const auto paksDir = ctx.config.resolvePaksDir(ctx.gameId);
    if (!paksDir)
      throw std::runtime_error(
          "paksDir not configured for @paks.\n\n"
          "Configure paksDir or games.<id>.paksDir in utool.json.");
    auto paks = collectPakFiles(*paksDir);
    if (paks.empty())
      throw std::runtime_error("No .pak files found in paksDir: " + paksDir->string());
    return ResolvedSource{
        .label = spec,
        .pakPaths = std::move(paks),
        .gameId = ctx.gameId,
    };
  }

  if (const auto gameKey = findGameIdCaseInsensitive(ctx.config, spec)) {
    const auto* game = findGameCaseInsensitive(ctx.config, *gameKey);
    std::vector<std::filesystem::path> pakPaths;
    if (game && game->paksDir) {
      const auto dir = ctx.config.resolvePath(*game->paksDir);
      auto paks = collectPakFiles(dir);
      pakPaths.insert(pakPaths.end(), paks.begin(), paks.end());
    }
    if (game && game->dataPak) {
      const auto dataPath = ctx.config.resolvePath(*game->dataPak);
      if (std::filesystem::is_regular_file(dataPath, ec)) {
        const auto canonical = std::filesystem::weakly_canonical(dataPath, ec);
        if (std::find(pakPaths.begin(), pakPaths.end(), canonical) == pakPaths.end())
          pakPaths.push_back(canonical);
      }
    }
    if (!pakPaths.empty()) {
      return ResolvedSource{
          .label = *gameKey,
          .pakPaths = std::move(pakPaths),
          .gameId = *gameKey,
      };
    }
    throwMissingPaksDir(*gameKey);
  }

  if (Core::looksLikeFilesystemTarget(spec)) {
    throw std::runtime_error("Path not found or contains no .pak files: " + spec);
  }

  throw std::runtime_error(
      "Unknown pak source: " + spec + "\n\n"
      "Use a .pak path, directory, @paks, @data, or a gameId from utool.json.");
}

}  // namespace

ResolveContext makeResolveContext(
    const Core::Config& config,
    const std::optional<std::string>& gameId) {
  ResolveContext ctx;
  ctx.config = config;
  ctx.gameId = gameId;

  const auto paths = resolveToolchain(
      config.unrealPak,
      config.unrealEngineDir,
      config.configDirectory,
      true);
  ctx.unrealPak = withCrypto(config, toOptions(paths), gameId);
  return ctx;
}

PakInventory resolvePakSource(std::string_view source, const ResolveContext& ctx) {
  const auto resolved = resolveSourceSpec(source, ctx);
  ResolveContext pakCtx = ctx;
  if (resolved.gameId)
    pakCtx.gameId = resolved.gameId;
  if (resolved.gameId) {
    pakCtx.unrealPak = withCrypto(ctx.config, ctx.unrealPak, resolved.gameId);
  }

  PakInventory inventory;
  inventory.sourceLabel = resolved.label;
  bool anyFromCache = false;
  std::error_code ec;

  for (const auto& pakPath : resolved.pakPaths) {
    bool fromCache = false;
    const auto sourcePak = pakPath.filename().string();
    inventory.pakFiles.emplace_back(sourcePak, std::filesystem::weakly_canonical(pakPath, ec));
    auto entries = listPakEntriesOnDisk(pakPath, pakCtx.unrealPak, sourcePak, fromCache);
    anyFromCache = anyFromCache || fromCache;
    inventory.entries.insert(inventory.entries.end(), entries.begin(), entries.end());
  }

  inventory.fromCache = anyFromCache;
  return inventory;
}

std::vector<PakFileEntry> searchInventory(
    const PakInventory& inventory,
    std::string_view query,
    std::optional<std::string_view> extFilter) {
  const std::string q = lower(std::string(query));
  std::vector<PakFileEntry> out;
  for (const auto& entry : inventory.entries) {
    if (extFilter && !extFilter->empty() && entry.extension != lower(std::string(*extFilter)))
      continue;
    const std::string pathLower = lower(entry.virtualPath);
    const std::string pakLower = lower(entry.sourcePak);
    if (pathLower.find(q) != std::string::npos || pakLower.find(q) != std::string::npos)
      out.push_back(entry);
  }
  return out;
}

std::vector<PakFileEntry> searchInventoryInside(
    const PakInventory& inventory,
    std::string_view query,
    const UnrealPakOptions& options,
    const std::uint64_t maxFileBytes) {
  return searchStringIndexes(inventory, query, options, maxFileBytes);
}

std::vector<PakFileEntry> filterInventoryByExtension(
    const PakInventory& inventory,
    std::string_view extFilter) {
  const std::string ext = lower(std::string(extFilter));
  std::vector<PakFileEntry> out;
  for (const auto& entry : inventory.entries) {
    if (entry.extension == ext)
      out.push_back(entry);
  }
  return out;
}

std::vector<PakFileEntry> findEntries(
    const PakInventory& inventory,
    std::string_view virtualPath,
    std::optional<std::string_view> sourcePak) {
  std::vector<PakFileEntry> out;
  for (const auto& entry : inventory.entries) {
    if (entry.virtualPath != virtualPath)
      continue;
    if (sourcePak && !sourcePak->empty()) {
      if (!iequals(entry.sourcePak, *sourcePak) &&
          !iequals(std::filesystem::path(entry.sourcePak).filename().string(), *sourcePak))
        continue;
    }
    out.push_back(entry);
  }
  return out;
}

std::optional<PakFileEntry> resolveSingleEntry(
    const PakInventory& inventory,
    std::string_view virtualPath,
    std::optional<std::string_view> sourcePak) {
  const auto matches = findEntries(inventory, virtualPath, sourcePak);
  if (matches.empty())
    return std::nullopt;
  if (matches.size() == 1)
    return matches.front();
  if (sourcePak && !sourcePak->empty())
    return matches.front();
  return std::nullopt;
}

std::optional<std::filesystem::path> findPakPathOnDisk(
    const PakInventory& inventory,
    std::string_view sourcePak) {
  for (const auto& [name, path] : inventory.pakFiles) {
    if (iequals(name, sourcePak))
      return path;
    if (iequals(std::filesystem::path(name).filename().string(), sourcePak))
      return path;
  }
  return std::nullopt;
}

}  // namespace UTool::Pak
