#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace UTool::Pak {

struct ToolchainPaths {
  std::filesystem::path executable;
  std::filesystem::path engineDir;
  std::filesystem::path storeRoot;
};

struct UnrealPakOptions {
  std::optional<std::filesystem::path> executablePath;
  std::optional<std::filesystem::path> engineDir;
  std::optional<std::filesystem::path> cryptoKeysPath;
};

struct UnrealPakCaptureResult {
  int exitCode = -1;
  std::string stdoutText;
  std::string stderrText;
};

[[nodiscard]] std::optional<std::filesystem::path> tryFindRepoRoot(
    const std::filesystem::path& start = {});

[[nodiscard]] bool tryEnsureExtracted(
    const std::filesystem::path& configDirectory = {},
    bool force = false);

[[nodiscard]] ToolchainPaths resolveToolchain(
    const std::optional<std::string>& configExecutable = std::nullopt,
    const std::optional<std::string>& configEngineDir = std::nullopt,
    const std::filesystem::path& configDirectory = {},
    bool ensureLocalCopy = true);

[[nodiscard]] std::filesystem::path resolveExecutable(const UnrealPakOptions& options = {});

[[nodiscard]] UnrealPakCaptureResult runUnrealPakCapture(
    const std::vector<std::wstring>& args,
    const UnrealPakOptions& options = {});

void extract(
    const std::filesystem::path& pakPath,
    const std::filesystem::path& outputDirectory,
    const std::optional<std::string>& filter = std::nullopt,
    const UnrealPakOptions& options = {});

void packDirectory(
    const std::filesystem::path& contentDirectory,
    const std::filesystem::path& outputPakPath,
    const std::string& mountPoint,
    bool compress = false,
    const UnrealPakOptions& options = {});

[[nodiscard]] UnrealPakOptions toOptions(const ToolchainPaths& paths);

[[nodiscard]] bool tryListPak(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options = {});

[[nodiscard]] UnrealPakCaptureResult listPakCapture(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options = {});

}  // namespace UTool::Pak
