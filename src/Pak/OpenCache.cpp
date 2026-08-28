#include "UTool/Pak/OpenCache.hpp"

#include "PakInternal.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace UTool::Pak {
namespace {

std::string basenameFromVirtualPath(std::string_view virtualPath) {
  const auto pos = virtualPath.find_last_of("/\\");
  if (pos == std::string_view::npos)
    return std::string(virtualPath);
  return std::string(virtualPath.substr(pos + 1));
}

std::string hashKey(std::string_view key) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string_view>{}(key);
  return oss.str();
}

std::filesystem::path findExtractedFile(
    const std::filesystem::path& root,
    std::string_view targetName) {
  std::error_code ec;
  if (!std::filesystem::exists(root, ec))
    return {};
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().filename().string() == targetName)
      return entry.path();
  }
  return {};
}

std::string entryCacheKey(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(pakPathOnDisk, ec);
  const auto mtime = std::filesystem::last_write_time(pakPathOnDisk, ec);
  const auto version = unrealPakVersionFingerprint(resolveExecutable(options));
  std::ostringstream oss;
  oss << pakPathOnDisk.string() << '|' << entry.virtualPath << '|' << entry.size << '|'
      << size << '|' << mtime.time_since_epoch().count() << '|' << version;
  return oss.str();
}

std::filesystem::path metaPath() {
  return openCacheRoot() / "index.json";
}

nlohmann::json loadMeta() {
  const auto path = metaPath();
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec))
    return nlohmann::json{{"entries", nlohmann::json::array()}, {"totalSize", 0}};
  std::ifstream in(path);
  if (!in)
    return nlohmann::json{{"entries", nlohmann::json::array()}, {"totalSize", 0}};
  try {
    nlohmann::json doc;
    in >> doc;
    if (!doc.contains("entries") || !doc["entries"].is_array())
      doc["entries"] = nlohmann::json::array();
    if (!doc.contains("totalSize"))
      doc["totalSize"] = 0;
    return doc;
  } catch (...) {
    return nlohmann::json{{"entries", nlohmann::json::array()}, {"totalSize", 0}};
  }
}

void saveMeta(const nlohmann::json& doc) {
  const auto root = openCacheRoot();
  std::filesystem::create_directories(root);
  const auto path = metaPath();
  const auto temp = root / "index.json.tmp";
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

std::uint64_t recomputeTotal(nlohmann::json& doc) {
  std::uint64_t total = 0;
  for (const auto& e : doc["entries"])
    total += e.value("size", static_cast<std::uint64_t>(0));
  doc["totalSize"] = total;
  return total;
}

void evictUntilUnder(nlohmann::json& doc, std::uint64_t maxBytes) {
  auto& entries = doc["entries"];
  while (recomputeTotal(doc) > maxBytes && !entries.empty()) {
    auto oldest = entries.begin();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
      if (it->value("lastAccess", 0ull) < oldest->value("lastAccess", 0ull))
        oldest = it;
    }
    const auto rel = oldest->value("relPath", "");
    if (!rel.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(openCacheRoot() / rel, ec);
    }
    entries.erase(oldest);
  }
  recomputeTotal(doc);
}

std::uint64_t nowMs() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

}  // namespace

std::filesystem::path openCacheRoot() {
  return std::filesystem::path(utoolStoreRoot()) / "cache" / "pak-open";
}

std::filesystem::path extractExactEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::filesystem::path& outputDirectory) {
  const std::string fileName = basenameFromVirtualPath(entry.virtualPath);
  std::error_code ec;
  if (std::filesystem::exists(outputDirectory, ec))
    std::filesystem::remove_all(outputDirectory, ec);
  std::filesystem::create_directories(outputDirectory);

  // Tight filter: match exact filename, not the loose *stem* sibling pull.
  const std::string filter = "*" + fileName;
  extract(pakPathOnDisk, outputDirectory, filter, options);

  const auto found = findExtractedFile(outputDirectory, fileName);
  if (found.empty())
    throw std::runtime_error(
        "UnrealPak extract did not produce " + fileName + " from " + pakPathOnDisk.string());
  return found;
}

std::filesystem::path openCachedEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::uint64_t maxBytes) {
  const auto key = entryCacheKey(entry, pakPathOnDisk, options);
  const auto keyHash = hashKey(key);
  auto meta = loadMeta();

  for (auto& e : meta["entries"]) {
    if (e.value("keyHash", "") != keyHash)
      continue;
    const auto rel = e.value("relPath", "");
    const auto cached = openCacheRoot() / rel / basenameFromVirtualPath(entry.virtualPath);
    std::error_code ec;
    if (std::filesystem::is_regular_file(cached, ec)) {
      e["lastAccess"] = nowMs();
      saveMeta(meta);
      return cached;
    }
  }

  const auto relDir = std::filesystem::path("files") / keyHash;
  const auto outDir = openCacheRoot() / relDir;
  const auto extracted = extractExactEntry(entry, pakPathOnDisk, options, outDir);
  std::error_code ec;
  const auto size = std::filesystem::file_size(extracted, ec);

  nlohmann::json record = {
      {"keyHash", keyHash},
      {"relPath", relDir.generic_string()},
      {"virtualPath", entry.virtualPath},
      {"sourcePak", entry.sourcePak},
      {"size", size},
      {"lastAccess", nowMs()},
  };
  meta["entries"].push_back(std::move(record));
  evictUntilUnder(meta, maxBytes);
  saveMeta(meta);
  return extracted;
}

std::vector<std::string> extractAsciiStrings(
    const std::filesystem::path& filePath,
    const std::size_t minLen,
    const std::size_t maxStrings,
    const std::size_t maxLen) {
  std::ifstream in(filePath, std::ios::binary);
  if (!in)
    return {};

  std::string data(
      (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<std::string> out;
  std::string current;
  current.reserve(64);

  auto flush = [&]() {
    if (current.size() >= minLen && out.size() < maxStrings) {
      if (current.size() > maxLen)
        current.resize(maxLen);
      out.push_back(current);
    }
    current.clear();
  };

  for (unsigned char b : data) {
    if (b >= 32 && b < 127) {
      current.push_back(static_cast<char>(b));
      if (current.size() > maxLen)
        flush();
    } else {
      flush();
    }
    if (out.size() >= maxStrings)
      break;
  }
  flush();
  return out;
}

}  // namespace UTool::Pak
