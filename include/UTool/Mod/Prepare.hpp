#pragma once

#include "UTool/Core/Config.hpp"
#include "UTool/Mod/CurveFloat.hpp"
#include "UTool/Pak/UnrealPak.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace UTool::Mod {

struct PrepareOptions {
  std::optional<std::filesystem::path> sourcePak;
  std::vector<std::filesystem::path> curveSourcePaks;
  std::optional<std::filesystem::path> extractedDir;
  bool preserveSourcePaths = false;
  bool forceExtract = false;
  Pak::UnrealPakOptions unrealPak;
  /// When set, apply that pak.create variant on top of base field sets.
  std::optional<std::size_t> pakVariantIndex;
  /// Mount point used for packing; bare asset paths stay flat unless this is Content root.
  std::string mountPoint;
};

struct PrepareResult {
  bool ok = false;
  std::string message;
  std::filesystem::path preparedContentDir;
  std::vector<std::filesystem::path> preparedFiles;
};

struct BuildModResult {
  bool ok = false;
  std::string message;
  std::filesystem::path outputPak;
};

struct DeployModResult {
  bool ok = false;
  std::string message;
  std::filesystem::path pakDest;
};

[[nodiscard]] PrepareResult prepareMod(
    const Core::ModPackage& package,
    const Core::Config& config,
    const PrepareOptions& options);

[[nodiscard]] BuildModResult buildMod(
    const Core::ModPackage& package,
    const Core::Config& config,
    const std::optional<std::filesystem::path>& outputOverride = std::nullopt,
    const std::optional<std::string>& mountOverride = std::nullopt,
    bool compress = false,
    bool forceExtract = false);

[[nodiscard]] DeployModResult deployMod(
    const Core::ModPackage& package,
    const Core::Config& config);

[[nodiscard]] std::filesystem::path mergeForPack(
    const Core::ModPackage& package,
    const std::filesystem::path& preparedDir);

}  // namespace UTool::Mod