#pragma once

#include "UTool/Pak/Inventory.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Pak {

constexpr std::uint64_t kStringIndexMaxFileBytes = 8ull * 1024ull * 1024ull;

/// Ensure durable string indexes exist for all paks in inventory (build/update if stale).
void ensureStringIndexes(
    const PakInventory& inventory,
    const UnrealPakOptions& options,
    std::uint64_t maxFileBytes = kStringIndexMaxFileBytes);

/// Search durable string indexes for query (case-insensitive). Does not extract whole paks.
[[nodiscard]] std::vector<PakFileEntry> searchStringIndexes(
    const PakInventory& inventory,
    std::string_view query,
    const UnrealPakOptions& options,
    std::uint64_t maxFileBytes = kStringIndexMaxFileBytes);

[[nodiscard]] std::filesystem::path stringIndexRoot();

}  // namespace UTool::Pak
