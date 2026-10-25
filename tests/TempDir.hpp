#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

struct TempDir {
  std::filesystem::path path;

  explicit TempDir(std::string name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path = std::filesystem::temp_directory_path() / (name + "-" + std::to_string(stamp));
    std::filesystem::create_directories(path);
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
};

inline void writeFile(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
}
