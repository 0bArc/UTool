#pragma once

#include "UTool/Core/GameCheck.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UTool::Core {

struct ModSetupOptions {
  std::optional<std::string> modId;
  std::optional<std::string> modName;
};

struct ModSetupResult {
  std::string modLua;
  std::string gameId;
  SupportLevel level = SupportLevel::Unsupported;
  std::vector<std::string> notes;
  bool viable = false;
};

[[nodiscard]] ProbedInstall probeFromConfig(const Config& config, std::string_view gameId);

/// Resolve Content-root mount for a game (`@auto` / missing mountPoint).
[[nodiscard]] std::string resolveAutoMountPoint(
    const Config& config,
    const std::optional<std::string>& gameId = std::nullopt);

[[nodiscard]] ModSetupResult generateModSetup(
    const Config& config,
    std::string_view gameIdOrPath,
    const ModSetupOptions& options = {});

}  // namespace UTool::Core
