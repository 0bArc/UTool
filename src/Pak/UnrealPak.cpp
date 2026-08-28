#include "UTool/Pak/UnrealPak.hpp"

#include "UTool/Core/Config.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace UTool::Pak {
namespace {

std::string quote(const std::filesystem::path& path) {
  return "\"" + path.string() + "\"";
}

std::string quote(const std::string& s) {
  return "\"" + s + "\"";
}

std::optional<std::filesystem::path> processDirectory() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH)
    return std::nullopt;
  return std::filesystem::path(buf).parent_path();
#else
  return std::nullopt;
#endif
}

std::optional<std::string> envVar(const char* name) {
  if (const char* v = std::getenv(name); v && *v)
    return std::string(v);
  return std::nullopt;
}

bool tryFromStore(const std::filesystem::path& store, ToolchainPaths& out) {
  const auto exe = store / "Engine" / "Binaries" / "Win64" / "UnrealPak.exe";
  if (!std::filesystem::is_regular_file(exe))
    return false;
  out.executable = std::filesystem::weakly_canonical(exe);
  out.engineDir = std::filesystem::weakly_canonical(store / "Engine");
  out.storeRoot = std::filesystem::weakly_canonical(store);
  return true;
}

std::vector<std::filesystem::path> enumerateStoreRoots(const std::filesystem::path& configDirectory) {
  std::vector<std::filesystem::path> roots;
  if (auto repo = tryFindRepoRoot(configDirectory))
    roots.push_back(*repo / "assets" / "UnrealPak");

  if (!configDirectory.empty()) {
    roots.push_back(configDirectory / "tools" / "UnrealPak");
    roots.push_back(configDirectory / "UnrealPak");
  }
  if (auto exeDir = processDirectory()) {
    roots.push_back(*exeDir / "tools" / "UnrealPak");
    roots.push_back(*exeDir / "UnrealPak");
  }
  if (auto local = envVar("LOCALAPPDATA"))
    roots.push_back(std::filesystem::path(*local) / "utool" / "UnrealPak");
  roots.emplace_back(R"(C:\software\UnrealPak)");
  return roots;
}

std::wstring q(const std::filesystem::path& path) {
  return L"\"" + path.wstring() + L"\"";
}

std::wstring q(const std::string& s) {
  return L"\"" + std::wstring(s.begin(), s.end()) + L"\"";
}

std::filesystem::path writeCreateResponseFile(
    const std::filesystem::path& contentDirectory,
    std::string mountPoint,
    const std::filesystem::path& responseFilePath) {
  for (char& c : mountPoint) {
    if (c == '\\')
      c = '/';
  }
  if (!mountPoint.empty() && mountPoint.back() != '/')
    mountPoint.push_back('/');

  std::ofstream out(responseFilePath, std::ios::binary);
  if (!out)
    throw std::runtime_error("Cannot write response file: " + responseFilePath.string());

  int count = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(contentDirectory)) {
    if (!entry.is_regular_file())
      continue;
    auto relative = std::filesystem::relative(entry.path(), contentDirectory).generic_string();
    const std::string pakPath = mountPoint + relative;
    out << quote(entry.path()) << ' ' << quote(pakPath) << '\n';
    ++count;
  }
  if (count == 0)
    throw std::runtime_error("No files under " + contentDirectory.string());
  return responseFilePath;
}

[[nodiscard]] UnrealPakCaptureResult runUnrealPakCaptureImpl(
    const std::vector<std::wstring>& args,
    const UnrealPakOptions& options,
    bool discardOutput) {
  UnrealPakCaptureResult result;
  const auto exe = resolveExecutable(options);
  std::wstring cmdLine = L"\"" + exe.wstring() + L"\"";
  for (const auto& a : args) {
    cmdLine.push_back(L' ');
    cmdLine += a;
  }
  if (options.engineDir) {
    cmdLine += L" -enginedir=\"";
    cmdLine += options.engineDir->wstring();
    cmdLine += L"\"";
  }
  if (options.cryptoKeysPath) {
    cmdLine += L" -cryptokeys=\"";
    cmdLine += options.cryptoKeysPath->wstring();
    cmdLine += L"\"";
  }

#ifdef _WIN32
  std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
  mutableCmd.push_back(L'\0');

  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE readOut = INVALID_HANDLE_VALUE;
  HANDLE writeOut = INVALID_HANDLE_VALUE;
  HANDLE readErr = INVALID_HANDLE_VALUE;
  HANDLE writeErr = INVALID_HANDLE_VALUE;

  if (!discardOutput) {
    CreatePipe(&readOut, &writeOut, &sa, 0);
    CreatePipe(&readErr, &writeErr, &sa, 0);
    SetHandleInformation(readOut, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(readErr, HANDLE_FLAG_INHERIT, 0);
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  HANDLE nullOut = INVALID_HANDLE_VALUE;
  if (discardOutput) {
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    nullOut = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullOut != INVALID_HANDLE_VALUE) {
      si.hStdOutput = nullOut;
      si.hStdError = nullOut;
    }
  } else {
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = writeOut;
    si.hStdError = writeErr;
  }

  PROCESS_INFORMATION pi{};
  const BOOL ok = CreateProcessW(
      exe.wstring().c_str(),
      mutableCmd.data(),
      nullptr,
      nullptr,
      TRUE,
      0,
      nullptr,
      nullptr,
      &si,
      &pi);
  if (!ok) {
    if (readOut != INVALID_HANDLE_VALUE)
      CloseHandle(readOut);
    if (writeOut != INVALID_HANDLE_VALUE)
      CloseHandle(writeOut);
    if (readErr != INVALID_HANDLE_VALUE)
      CloseHandle(readErr);
    if (writeErr != INVALID_HANDLE_VALUE)
      CloseHandle(writeErr);
    throw std::runtime_error(
        "CreateProcess UnrealPak failed (" + std::to_string(GetLastError()) + "): " +
        exe.string());
  }

  if (!discardOutput) {
    CloseHandle(writeOut);
    CloseHandle(writeErr);
  }

  auto drain = [](HANDLE pipe, std::string& out) {
    if (pipe == INVALID_HANDLE_VALUE)
      return;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
      out.append(buffer, buffer + read);
    CloseHandle(pipe);
  };

  if (!discardOutput) {
    drain(readOut, result.stdoutText);
    drain(readErr, result.stderrText);
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (nullOut != INVALID_HANDLE_VALUE)
    CloseHandle(nullOut);
  result.exitCode = static_cast<int>(code);
  return result;
#else
  (void)args;
  (void)options;
  (void)discardOutput;
  return result;
#endif
}

[[nodiscard]] int runUnrealPakExitCode(
    const std::vector<std::wstring>& args,
    const UnrealPakOptions& options,
    bool quiet = false) {
  return runUnrealPakCaptureImpl(args, options, quiet).exitCode;
}

void runUnrealPak(const std::vector<std::wstring>& args, const UnrealPakOptions& options) {
  if (runUnrealPakExitCode(args, options) != 0) {
    const auto exe = resolveExecutable(options);
    throw std::runtime_error("UnrealPak failed: " + exe.string());
  }
}

}  // namespace

std::optional<std::filesystem::path> tryFindRepoRoot(const std::filesystem::path& start) {
  return Core::findRepoRoot(start);
}

bool tryEnsureExtracted(const std::filesystem::path& configDirectory, bool force) {
  auto repo = tryFindRepoRoot(configDirectory.empty() ? std::filesystem::current_path()
                                                      : configDirectory);
  if (!repo)
    return false;

  const auto zipPath = *repo / "assets" / "UnrealPak.zip";
  if (!std::filesystem::is_regular_file(zipPath))
    return false;

  const auto storeRoot = *repo / "assets" / "UnrealPak";
  const auto exe = storeRoot / "Engine" / "Binaries" / "Win64" / "UnrealPak.exe";
  if (!force && std::filesystem::is_regular_file(exe) &&
      std::filesystem::last_write_time(exe) >= std::filesystem::last_write_time(zipPath))
    return true;

  std::filesystem::create_directories(*repo / "assets");
  const std::string cmd =
      "powershell -NoProfile -Command \"Expand-Archive -LiteralPath '" + zipPath.string() +
      "' -DestinationPath '" + (*repo / "assets").string() + "' -Force\"";
  const int code = std::system(cmd.c_str());
  return code == 0 && std::filesystem::is_regular_file(exe);
}

std::filesystem::path resolveExecutable(const UnrealPakOptions& options) {
  if (options.executablePath && std::filesystem::is_regular_file(*options.executablePath))
    return std::filesystem::weakly_canonical(*options.executablePath);

  if (auto fromEnv = envVar("UTOOL_UNREALPAK"); fromEnv && std::filesystem::is_regular_file(*fromEnv))
    return std::filesystem::weakly_canonical(*fromEnv);

  for (const auto& store : enumerateStoreRoots({})) {
    ToolchainPaths paths;
    if (tryFromStore(store, paths))
      return paths.executable;
  }

  const std::vector<std::filesystem::path> common = {
      R"(C:\software\UnrealPak\Engine\Binaries\Win64\UnrealPak.exe)",
      R"(C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UnrealPak.exe)",
      R"(C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealPak.exe)",
  };
  for (const auto& path : common) {
    if (std::filesystem::is_regular_file(path))
      return path;
  }

  throw std::runtime_error(
      "UnrealPak.exe not found. Ensure assets/UnrealPak.zip exists "
      "(auto-extracts on first use), or set UTOOL_UNREALPAK.");
}

ToolchainPaths resolveToolchain(
    const std::optional<std::string>& configExecutable,
    const std::optional<std::string>& configEngineDir,
    const std::filesystem::path& configDirectory,
    bool ensureLocalCopy) {
  if (ensureLocalCopy)
    tryEnsureExtracted(configDirectory);

  for (const auto& store : enumerateStoreRoots(configDirectory)) {
    ToolchainPaths paths;
    if (tryFromStore(store, paths))
      return paths;
  }

  UnrealPakOptions opt;
  if (configExecutable)
    opt.executablePath = *configExecutable;
  if (configEngineDir)
    opt.engineDir = *configEngineDir;

  ToolchainPaths paths;
  paths.executable = resolveExecutable(opt);
  if (configEngineDir)
    paths.engineDir = std::filesystem::weakly_canonical(*configEngineDir);
  else
    paths.engineDir = paths.executable.parent_path().parent_path().parent_path();  // Win64 -> Binaries -> Engine
  paths.storeRoot = paths.executable.parent_path();
  return paths;
}

UnrealPakOptions toOptions(const ToolchainPaths& paths) {
  UnrealPakOptions o;
  o.executablePath = paths.executable;
  o.engineDir = paths.engineDir;
  return o;
}

void extract(
    const std::filesystem::path& pakPath,
    const std::filesystem::path& outputDirectory,
    const std::optional<std::string>& filter,
    const UnrealPakOptions& options) {
  std::filesystem::create_directories(outputDirectory);
  std::vector<std::wstring> args = {
      q(std::filesystem::weakly_canonical(pakPath)),
      L"-Extract",
      q(std::filesystem::weakly_canonical(outputDirectory)),
  };
  if (filter && !filter->empty())
    args.push_back(L"-Filter=" + std::wstring(filter->begin(), filter->end()));
  runUnrealPak(args, options);
}

void packDirectory(
    const std::filesystem::path& contentDirectory,
    const std::filesystem::path& outputPakPath,
    const std::string& mountPoint,
    bool compress,
    const UnrealPakOptions& options) {
  const auto out = std::filesystem::weakly_canonical(outputPakPath);
  std::filesystem::create_directories(out.parent_path());

  const auto response =
      std::filesystem::temp_directory_path() /
      ("utool-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".txt");
  try {
    writeCreateResponseFile(contentDirectory, mountPoint, response);
    std::vector<std::wstring> args = {q(out), L"-Create=" + q(response)};
    if (compress)
      args.emplace_back(L"-compress");
    runUnrealPak(args, options);
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove(response, ec);
    throw;
  }
  std::error_code ec;
  std::filesystem::remove(response, ec);
}

bool tryListPak(const std::filesystem::path& pakPath, const UnrealPakOptions& options) {
  return listPakCapture(pakPath, options).exitCode == 0;
}

UnrealPakCaptureResult runUnrealPakCapture(
    const std::vector<std::wstring>& args,
    const UnrealPakOptions& options) {
  return runUnrealPakCaptureImpl(args, options, false);
}

UnrealPakCaptureResult listPakCapture(
    const std::filesystem::path& pakPath,
    const UnrealPakOptions& options) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(pakPath, ec))
    return UnrealPakCaptureResult{.exitCode = 1};
  const std::vector<std::wstring> args = {
      L"\"" + std::filesystem::weakly_canonical(pakPath).wstring() + L"\"",
      L"-List",
  };
  return runUnrealPakCapture(args, options);
}

}  // namespace UTool::Pak
