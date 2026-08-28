#pragma once

#include "UTool/Pak/Inventory.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Pak {

constexpr std::uint64_t kOpenCacheMaxBytes = 256ull * 1024ull * 1024ull;

/// Extract a single pak entry with a tight basename filter into outputDirectory.
[[nodiscard]] std::filesystem::path extractExactEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::filesystem::path& outputDirectory);

/// Open (or reuse) an extracted entry under the LRU cache. Evicts until under maxBytes.
[[nodiscard]] std::filesystem::path openCachedEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    std::uint64_t maxBytes = kOpenCacheMaxBytes);

[[nodiscard]] std::filesystem::path openCacheRoot();

[[nodiscard]] std::vector<std::string> extractAsciiStrings(
    const std::filesystem::path& filePath,
    std::size_t minLen = 4,
    std::size_t maxStrings = 200,
    std::size_t maxLen = 256);

}  // namespace UTool::Pak
