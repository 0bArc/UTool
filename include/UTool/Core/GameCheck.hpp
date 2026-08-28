#pragma once

#include "UTool/Core/Config.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Core {

enum class SupportLevel { Supported, Partial, Unsupported };

struct CheckLine {
  enum class Status { Ok, Warn, Fail, Info };
  Status status = Status::Info;
  std::string message;
};

struct ProbedInstall {
  std::filesystem::path inputPath;
  std::filesystem::path installRoot;
  std::optional<std::filesystem::path> paksDir;
  std::optional<std::filesystem::path> dataPak;
  std::size_t pakCount = 0;
  bool singlePakFile = false;
};

struct GameCheckReport {
  std::optional<std::string> gameId;
  std::optional<std::string> matchedConfigId;
  std::optional<std::filesystem::path> queriedPath;
  SupportLevel level = SupportLevel::Unsupported;
  std::vector<CheckLine> lines;
  std::vector<CheckLine> details;
};

[[nodiscard]] bool looksLikeFilesystemTarget(std::string_view target);

[[nodiscard]] ProbedInstall probeInstallPath(const std::filesystem::path& path);

[[nodiscard]] std::optional<std::string> findConfigGameIdForPaths(
    const Config& config,
    const ProbedInstall& probe);

[[nodiscard]] std::optional<std::string> findConfigGameIdByName(
    const Config& config,
    std::string_view gameId);

[[nodiscard]] GameCheckReport checkConfiguredGame(
    const Config& config,
    std::string_view gameId,
    const GameSettings& settings);

[[nodiscard]] GameCheckReport checkGameTarget(
    const Config& config,
    std::string_view gameIdOrPath);

[[nodiscard]] std::vector<GameCheckReport> checkAllConfiguredGames(const Config& config);

void printGameCheckReport(const GameCheckReport& report, std::ostream& out);

[[nodiscard]] int exitCodeForSupport(SupportLevel level);

}  // namespace UTool::Core
