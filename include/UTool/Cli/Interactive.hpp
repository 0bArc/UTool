#pragma once

#include <filesystem>
#include <string>

namespace UTool::Cli {

/// Interactive menu session (TTY only). Returns 0 on normal exit.
[[nodiscard]] int runInteractiveSession();

/// Per-mod CLI: build / deploy / banner / ship for one mod. Returns 0 on exit.
[[nodiscard]] int runModSession(const std::filesystem::path& modDir,
                                const std::string& catalogId = {});

/// Prompt for a new mod, scaffold it, then open runModSession. TTY only.
[[nodiscard]] int runNewModWizard(const std::filesystem::path& modsDir);

[[nodiscard]] bool isInteractiveTty();

}  // namespace UTool::Cli
