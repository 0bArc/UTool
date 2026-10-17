#include "UTool/Core/Env.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace UTool::Core {
namespace {

std::string trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return std::string(s);
}

std::string unquote(std::string s) {
  if (s.size() >= 2) {
    const char a = s.front();
    const char b = s.back();
    if ((a == '"' && b == '"') || (a == '\'' && b == '\''))
      return s.substr(1, s.size() - 2);
  }
  return s;
}

void setProcessEnv(const std::string& key, const std::string& value) {
#ifdef _WIN32
  SetEnvironmentVariableA(key.c_str(), value.c_str());
#else
  setenv(key.c_str(), value.c_str(), 0);
#endif
}

std::optional<std::filesystem::path> findConfigDir(std::filesystem::path dir) {
  namespace fs = std::filesystem;
  for (int i = 0; i < 8 && !dir.empty(); ++i) {
    for (const char* name : {"utool.json", "csstratware.json"}) {
      if (fs::is_regular_file(dir / name))
        return dir;
    }
    if (!dir.has_parent_path() || dir.parent_path() == dir)
      break;
    dir = dir.parent_path();
  }
  return std::nullopt;
}

}  // namespace

void loadEnvFile(const std::filesystem::path& path, EnvMap& into) {
  std::ifstream in(path);
  if (!in)
    return;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '#')
      continue;
    if (trimmed.rfind("export ", 0) == 0)
      trimmed = trim(trimmed.substr(7));
    const auto eq = trimmed.find('=');
    if (eq == std::string::npos)
      continue;
    auto key = trim(trimmed.substr(0, eq));
    auto value = unquote(trim(trimmed.substr(eq + 1)));
    if (key.empty())
      continue;
    if (into.contains(key))
      continue;
    into.emplace(std::move(key), std::move(value));
  }
}

EnvMap loadDotEnv(const std::filesystem::path& startDirectory, bool applyToProcess) {
  namespace fs = std::filesystem;
  EnvMap map;
  const fs::path start = startDirectory.empty() ? fs::current_path() : startDirectory;

  // cwd first; config-dir .env only fills missing keys. Process env wins in getEnv.
  loadEnvFile(fs::current_path() / ".env", map);
  if (auto cfgDir = findConfigDir(start))
    loadEnvFile(*cfgDir / ".env", map);
  if (start != fs::current_path())
    loadEnvFile(start / ".env", map);

  if (applyToProcess) {
    for (const auto& [k, v] : map) {
#ifdef _WIN32
      char buf[2];
      if (GetEnvironmentVariableA(k.c_str(), buf, 2) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        setProcessEnv(k, v);
#else
      if (!std::getenv(k.c_str()))
        setProcessEnv(k, v);
#endif
    }
  }
  return map;
}

std::optional<std::string> getEnv(std::string_view key, const EnvMap& loaded) {
  const std::string k(key);
  if (const char* v = std::getenv(k.c_str())) {
    if (*v)
      return std::string(v);
  }
  if (auto it = loaded.find(k); it != loaded.end() && !it->second.empty())
    return it->second;
  return std::nullopt;
}

}  // namespace UTool::Core
