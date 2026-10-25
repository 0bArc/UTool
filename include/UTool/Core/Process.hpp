#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace UTool::Core {

struct ProcessResult {
  int exitCode = -1;
  bool launched = false;
};

/// Start `executable` with `args`. No shell. The executable name is searched on PATH
/// when it has no directory.
[[nodiscard]] ProcessResult runProcess(const std::filesystem::path& executable,
                                       const std::vector<std::wstring>& args);

}  // namespace UTool::Core
