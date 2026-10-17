#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Pak {

struct ParsedListEntry {
  std::string virtualPath;
  std::uint64_t size = 0;
  std::uint64_t offset = 0;
};

[[nodiscard]] std::vector<ParsedListEntry> parsePakListOutput(std::string_view output);

[[nodiscard]] std::string fileExtensionLower(std::string_view virtualPath);

[[nodiscard]] std::string utoolStoreRoot();

[[nodiscard]] std::string unrealPakVersionFingerprint(const std::filesystem::path& executable);

[[nodiscard]] std::filesystem::path ensureCryptoJson(
    std::string_view aesKeyMaterial,
    std::string_view cacheId);

}  // namespace UTool::Pak
