#include "PakInternal.hpp"

#include "UTool/Pak/UnrealPak.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace UTool::Pak {
namespace {

std::string trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.remove_suffix(1);
  return std::string(s);
}

std::optional<std::string> envVar(const char* name) {
  if (const char* v = std::getenv(name); v && *v)
    return std::string(v);
  return std::nullopt;
}

bool isHexKey(std::string_view s) {
  if (s.size() != 64)
    return false;
  for (char c : s) {
    if (!std::isxdigit(static_cast<unsigned char>(c)))
      return false;
  }
  return true;
}

std::string hexToBase64(std::string_view hex) {
  auto hexVal = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return 10 + c - 'a';
    if (c >= 'A' && c <= 'F')
      return 10 + c - 'A';
    return -1;
  };

  std::string bytes;
  bytes.reserve(hex.size() / 2);
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    const int hi = hexVal(hex[i]);
    const int lo = hexVal(hex[i + 1]);
    if (hi < 0 || lo < 0)
      throw std::runtime_error("Invalid hex character in pakAesKey");
    bytes.push_back(static_cast<char>((hi << 4) | lo));
  }

  static const char* b64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const unsigned b0 = static_cast<unsigned char>(bytes[i]);
    const unsigned b1 = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
    const unsigned b2 = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
    out.push_back(b64[(b0 >> 2) & 0x3F]);
    out.push_back(b64[((b0 << 4) | (b1 >> 4)) & 0x3F]);
    out.push_back(i + 1 < bytes.size() ? b64[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=');
    out.push_back(i + 2 < bytes.size() ? b64[b2 & 0x3F] : '=');
  }
  return out;
}

std::string normalizeAesKeyToBase64(std::string_view keyMaterial) {
  const std::string trimmed = trim(keyMaterial);
  if (trimmed.empty())
    throw std::runtime_error("pakAesKey is empty");
  if (isHexKey(trimmed))
    return hexToBase64(trimmed);
  return trimmed;
}

std::filesystem::path cryptoTemplatePath() {
  if (auto repo = tryFindRepoRoot({})) {
    const auto path =
        *repo / "assets" / "UnrealPak" / "Engine" / "Binaries" / "Win64" / "Crypto.json";
    if (std::filesystem::is_regular_file(path))
      return path;
  }
  const auto paths = resolveToolchain({}, {}, {}, false);
  const auto path = paths.executable.parent_path() / "Crypto.json";
  if (std::filesystem::is_regular_file(path))
    return path;
  throw std::runtime_error("Crypto.json template not found in UnrealPak toolchain");
}

std::string sanitizeCacheId(std::string_view cacheId) {
  std::string out;
  for (char c : cacheId) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')
      out.push_back(c);
    else
      out.push_back('_');
  }
  if (out.empty())
    out = "default";
  return out;
}

}  // namespace

std::string utoolStoreRoot() {
  if (auto local = envVar("LOCALAPPDATA"))
    return *local + "\\utool";
  return std::filesystem::temp_directory_path().string() + "\\utool";
}

std::string unrealPakVersionFingerprint(const std::filesystem::path& executable) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(executable, ec))
    return "missing";
  const auto size = std::filesystem::file_size(executable, ec);
  const auto mtime = std::filesystem::last_write_time(executable, ec);
  std::ostringstream oss;
  oss << executable.filename().string() << ':' << size << ':'
      << mtime.time_since_epoch().count();
  return oss.str();
}

std::filesystem::path ensureCryptoJson(std::string_view aesKeyMaterial, std::string_view cacheId) {
  const std::string base64Key = normalizeAesKeyToBase64(aesKeyMaterial);
  const auto cryptoDir = std::filesystem::path(utoolStoreRoot()) / "crypto";
  std::filesystem::create_directories(cryptoDir);

  const auto finalPath = cryptoDir / (sanitizeCacheId(cacheId) + ".json");
  if (std::filesystem::is_regular_file(finalPath)) {
    std::ifstream existing(finalPath);
    std::string content((std::istreambuf_iterator<char>(existing)),
                        std::istreambuf_iterator<char>());
    if (content.find(base64Key) != std::string::npos)
      return finalPath;
  }

  std::ifstream tmpl(cryptoTemplatePath());
  if (!tmpl)
    throw std::runtime_error("Cannot read Crypto.json template");
  std::string content((std::istreambuf_iterator<char>(tmpl)), std::istreambuf_iterator<char>());

  const std::string placeholder = "Your Base64 key here";
  const auto pos = content.find(placeholder);
  if (pos == std::string::npos)
    throw std::runtime_error("Crypto.json template missing key placeholder");
  content.replace(pos, placeholder.size(), base64Key);

  const auto tempPath =
      cryptoDir /
      (sanitizeCacheId(cacheId) + ".tmp." +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");

  {
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out)
      throw std::runtime_error("Cannot write temporary Crypto.json");
    out << content;
    out.flush();
    if (!out)
      throw std::runtime_error("Failed writing temporary Crypto.json");
  }

  std::error_code ec;
  std::filesystem::rename(tempPath, finalPath, ec);
  if (ec) {
    std::filesystem::remove(finalPath, ec);
    ec.clear();
    std::filesystem::rename(tempPath, finalPath, ec);
    if (ec)
      throw std::runtime_error("Failed to finalize Crypto.json: " + ec.message());
  }
  return finalPath;
}

}  // namespace UTool::Pak
