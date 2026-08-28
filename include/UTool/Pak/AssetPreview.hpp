#pragma once

#include "UTool/Pak/Inventory.hpp"

#include <nlohmann/json.hpp>

#include <optional>

namespace UTool::Pak {

enum class PreviewKind { Json, Curve, Text, Binary, Unsupported };

struct AssetPreview {
  PreviewKind kind = PreviewKind::Unsupported;
  std::string sourcePak;
  std::string virtualPath;
  nlohmann::json payload;
};

struct SnippetRequest {
  std::optional<std::string> row;
  std::optional<std::string> field;
  std::optional<nlohmann::json> value;
};

[[nodiscard]] AssetPreview previewEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::filesystem::path& tempDir);

/// Open entry via LRU extract cache and return preview (+ extractedPath in payload).
[[nodiscard]] AssetPreview openEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options);

[[nodiscard]] std::filesystem::path extractEntryToDir(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::filesystem::path& outputDirectory);

[[nodiscard]] std::string generateSnippet(
    const AssetPreview& preview,
    const SnippetRequest& request = {});

[[nodiscard]] const char* previewKindName(PreviewKind kind);

}  // namespace UTool::Pak
