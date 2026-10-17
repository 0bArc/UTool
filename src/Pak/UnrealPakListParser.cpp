#include "PakInternal.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace UTool::Pak {

namespace {

std::string trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.remove_suffix(1);
  return std::string(s);
}

}  // namespace

std::vector<ParsedListEntry> parsePakListOutput(std::string_view output) {
  static const std::regex lineRe(
      R"(LogPakFile:\s*Display:\s*\"([^\"]+)\"\s+offset:\s*(\d+),\s*size:\s*(\d+)\s*bytes)");

  std::vector<ParsedListEntry> entries;
  const std::string text(output);
  auto begin = std::sregex_iterator(text.begin(), text.end(), lineRe);
  const auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    ParsedListEntry entry;
    entry.virtualPath = (*it)[1].str();
    entry.offset = static_cast<std::uint64_t>(std::stoull((*it)[2].str()));
    entry.size = static_cast<std::uint64_t>(std::stoull((*it)[3].str()));
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::string fileExtensionLower(std::string_view virtualPath) {
  const auto pos = virtualPath.rfind('.');
  if (pos == std::string_view::npos || pos + 1 >= virtualPath.size())
    return {};
  std::string ext(virtualPath.substr(pos + 1));
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

}  // namespace UTool::Pak
