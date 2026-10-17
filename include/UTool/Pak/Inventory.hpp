#pragma once

#include "UTool/Pak/UnrealPak.hpp"

#include "UTool/Core/Config.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Pak {

struct PakFileEntry {
  std::string sourcePak;
  std::string virtualPath;
  std::uint64_t size = 0;
  std::uint64_t offset = 0;
  std::string extension;
};

struct PakInventory {
  std::string sourceLabel;
  std::vector<PakFileEntry> entries;
  bool fromCache = false;
  /// sourcePak filename -> absolute path on disk
  std::vector<std::pair<std::string, std::filesystem::path>> pakFiles;
};

struct ResolveContext {
  Core::Config config;
  UnrealPakOptions unrealPak;
  std::optional<std::string> gameId;
};

[[nodiscard]] ResolveContext makeResolveContext(
    const Core::Config& config,
    const std::optional<std::string>& gameId = std::nullopt);

[[nodiscard]] PakInventory resolvePakSource(
    std::string_view source,
    const ResolveContext& ctx);

[[nodiscard]] std::vector<PakFileEntry> searchInventory(
    const PakInventory& inventory,
    std::string_view query,
    std::optional<std::string_view> extFilter = std::nullopt);

/// Search durable string indexes (builds per-pak indexes via single-file extract).
[[nodiscard]] std::vector<PakFileEntry> searchInventoryInside(
    const PakInventory& inventory,
    std::string_view query,
    const UnrealPakOptions& options,
    std::uint64_t maxFileBytes = 8ull * 1024ull * 1024ull);

[[nodiscard]] std::vector<PakFileEntry> filterInventoryByExtension(
    const PakInventory& inventory,
    std::string_view extFilter);

[[nodiscard]] std::vector<PakFileEntry> findEntries(
    const PakInventory& inventory,
    std::string_view virtualPath,
    std::optional<std::string_view> sourcePak = std::nullopt);

[[nodiscard]] std::optional<PakFileEntry> resolveSingleEntry(
    const PakInventory& inventory,
    std::string_view virtualPath,
    std::optional<std::string_view> sourcePak = std::nullopt);

[[nodiscard]] std::optional<std::filesystem::path> findPakPathOnDisk(
    const PakInventory& inventory,
    std::string_view sourcePak);

}  // namespace UTool::Pak
