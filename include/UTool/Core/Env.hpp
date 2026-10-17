#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace UTool::Core {

/// KEY=VALUE map loaded from .env files (comments and blank lines ignored).
using EnvMap = std::unordered_map<std::string, std::string>;

/// Parse a .env file. Does not overwrite keys already present in `into`.
void loadEnvFile(const std::filesystem::path& path, EnvMap& into);

/// Load .env from cwd, then from next to utool.json (walk-up). Existing process
/// env vars win: this only fills missing keys into the process environment when
/// `applyToProcess` is true.
///
/// Lookup order for getEnv (first wins): process env -> map from files.
[[nodiscard]] EnvMap loadDotEnv(
    const std::filesystem::path& startDirectory = {},
    bool applyToProcess = true);

[[nodiscard]] std::optional<std::string> getEnv(
    std::string_view key,
    const EnvMap& loaded = {});

}  // namespace UTool::Core
