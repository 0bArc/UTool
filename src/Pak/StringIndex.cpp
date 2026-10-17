#include "UTool/Pak/StringIndex.hpp"

#include "UTool/Pak/OpenCache.hpp"
#include "PakInternal.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>

namespace UTool::Pak {
namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string hashKey(std::string_view key) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string_view>{}(key);
  return oss.str();
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

bool isIndexExtension(std::string_view ext) {
  return ext == "uasset" || ext == "uexp";
}

std::filesystem::path indexFileForPak(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options) {
  return stringIndexRoot() / (hashKey(cacheKeyForPak(pakPath, options)) + ".json");
}

nlohmann::json loadIndex(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec))
    return {};
  std::ifstream in(path);
  if (!in)
    return {};
  try {
    nlohmann::json doc;
    in >> doc;
    return doc;
  } catch (...) {
    return {};
  }
}

void saveIndex(const std::filesystem::path& path, const nlohmann::json& doc) {
  std::filesystem::create_directories(path.parent_path());
  const auto temp = path.string() + ".tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << doc.dump();
  }
  std::error_code ec;
  std::filesystem::rename(temp, path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp, path, ec);
  }
}

void buildIndexForPak(
    const std::string& sourcePak,
    const std::filesystem::path& pakPath,
    const std::vector<PakFileEntry>& entries,
    const UnrealPakOptions& options,
    const std::uint64_t maxFileBytes) {
  const auto cacheKey = cacheKeyForPak(pakPath, options);
  const auto indexPath = indexFileForPak(pakPath, options);
  auto existing = loadIndex(indexPath);
  if (existing.value("cacheKey", "") == cacheKey && existing.contains("files") &&
      existing["files"].is_object())
    return;

  nlohmann::json files = nlohmann::json::object();
  const auto scratch =
      stringIndexRoot() / "scratch" /
      hashKey(pakPath.string() + std::to_string(
                                     std::chrono::steady_clock::now().time_since_epoch().count()));

  for (const auto& entry : entries) {
    if (entry.sourcePak != sourcePak)
      continue;
    if (!isIndexExtension(entry.extension))
      continue;
    if (entry.size == 0 || entry.size > maxFileBytes)
      continue;

    try {
      std::error_code ec;
      std::filesystem::remove_all(scratch, ec);
      const auto extracted = extractExactEntry(entry, pakPath, options, scratch);
      auto strings = extractAsciiStrings(extracted, 4, 120, 200);
      nlohmann::json arr = nlohmann::json::array();
      for (auto& s : strings)
        arr.push_back(std::move(s));
      files[entry.virtualPath] = std::move(arr);
      std::filesystem::remove_all(scratch, ec);
    } catch (...) {
      std::error_code ec;
      std::filesystem::remove_all(scratch, ec);
    }
  }

  nlohmann::json doc;
  doc["cacheKey"] = cacheKey;
  doc["sourcePak"] = sourcePak;
  doc["pakPath"] = pakPath.string();
  doc["files"] = std::move(files);
  saveIndex(indexPath, doc);

  std::error_code ec;
  std::filesystem::remove_all(scratch, ec);
}

}  // namespace

std::filesystem::path stringIndexRoot() {
  return std::filesystem::path(utoolStoreRoot()) / "cache" / "pak-strings";
}

void ensureStringIndexes(
    const PakInventory& inventory,
    const UnrealPakOptions& options,
    const std::uint64_t maxFileBytes) {
  for (const auto& [sourcePak, pakPath] : inventory.pakFiles)
    buildIndexForPak(sourcePak, pakPath, inventory.entries, options, maxFileBytes);
}

std::vector<PakFileEntry> searchStringIndexes(
    const PakInventory& inventory,
    std::string_view query,
    const UnrealPakOptions& options,
    const std::uint64_t maxFileBytes) {
  const std::string q = lower(std::string(query));
  if (q.empty())
    return {};

  ensureStringIndexes(inventory, options, maxFileBytes);

  std::vector<PakFileEntry> out;
  for (const auto& [sourcePak, pakPath] : inventory.pakFiles) {
    const auto doc = loadIndex(indexFileForPak(pakPath, options));
    if (!doc.contains("files") || !doc["files"].is_object())
      continue;

    for (auto it = doc["files"].begin(); it != doc["files"].end(); ++it) {
      bool hit = false;
      const std::string pathLower = lower(it.key());
      if (pathLower.find(q) != std::string::npos)
        hit = true;
      if (!hit && it.value().is_array()) {
        for (const auto& s : it.value()) {
          if (!s.is_string())
            continue;
          if (lower(s.get<std::string>()).find(q) != std::string::npos) {
            hit = true;
            break;
          }
        }
      }
      if (!hit)
        continue;

      for (const auto& entry : inventory.entries) {
        if (entry.sourcePak == sourcePak && entry.virtualPath == it.key()) {
          out.push_back(entry);
          break;
        }
      }
    }
  }
  return out;
}

}  // namespace UTool::Pak
