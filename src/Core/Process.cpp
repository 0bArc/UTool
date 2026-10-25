#include "UTool/Core/Process.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace UTool::Core {
namespace {

std::wstring quoteArg(const std::wstring& arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    return arg;

  std::wstring out = L"\"";
  size_t i = 0;
  while (i < arg.size()) {
    size_t slashes = 0;
    while (i < arg.size() && arg[i] == L'\\') {
      ++slashes;
      ++i;
    }
    if (i == arg.size()) {
      out.append(slashes * 2, L'\\');
      break;
    }
    if (arg[i] == L'"')
      out.append(slashes * 2 + 1, L'\\');
    else
      out.append(slashes, L'\\');
    out.push_back(arg[i]);
    ++i;
  }
  out.push_back(L'"');
  return out;
}

}  // namespace

ProcessResult runProcess(const std::filesystem::path& executable,
                         const std::vector<std::wstring>& args) {
  ProcessResult result;
#ifdef _WIN32
  std::wstring command = quoteArg(executable.wstring());
  for (const auto& arg : args) {
    command.push_back(L' ');
    command += quoteArg(arg);
  }

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                      &startup, &process))
    return result;

  result.launched = true;
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD code = 1;
  if (!GetExitCodeProcess(process.hProcess, &code))
    code = 1;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  result.exitCode = static_cast<int>(code);
  return result;
#else
  (void)executable;
  (void)args;
  return result;
#endif
}

}  // namespace UTool::Core
