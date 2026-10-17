#include "UTool/Pak/AssetPreview.hpp"

#include "UTool/Mod/CurveFloat.hpp"
#include "UTool/Mod/JsonEditor.hpp"
#include "UTool/Pak/OpenCache.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace UTool::Pak {
namespace {

constexpr std::size_t kMaxTextPreviewBytes = 256 * 1024;

std::string readFileLimited(const std::filesystem::path& path, std::size_t maxBytes) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("Cannot read " + path.string());
  std::string data;
  data.resize(maxBytes);
  in.read(data.data(), static_cast<std::streamsize>(maxBytes));
  data.resize(static_cast<std::size_t>(in.gcount()));
  return data;
}

std::string basenameFromVirtualPath(std::string_view virtualPath) {
  const auto pos = virtualPath.find_last_of("/\\");
  if (pos == std::string_view::npos)
    return std::string(virtualPath);
  return std::string(virtualPath.substr(pos + 1));
}

nlohmann::json summarizeJsonRows(const nlohmann::json& root) {
  nlohmann::json summary = nlohmann::json::object();
  if (!root.contains("Rows") || !root["Rows"].is_array())
    return summary;

  for (const auto& row : root["Rows"]) {
    if (!row.is_object())
      continue;
    const std::string name = row.value("Name", "");
    if (name.empty())
      continue;
    nlohmann::json fields = nlohmann::json::object();
    int count = 0;
    for (auto it = row.begin(); it != row.end() && count < 12; ++it) {
      if (it.key() == "Name")
        continue;
      fields[it.key()] = it.value();
      ++count;
    }
    summary[name] = fields;
  }
  return summary;
}

AssetPreview previewJsonFile(const std::filesystem::path& path, const PakFileEntry& entry) {
  AssetPreview preview;
  preview.kind = PreviewKind::Json;
  preview.sourcePak = entry.sourcePak;
  preview.virtualPath = entry.virtualPath;

  const auto text = readFileLimited(path, kMaxTextPreviewBytes);
  const auto root = nlohmann::json::parse(text);
  preview.payload = {
      {"pretty", root.dump(2)},
      {"rowSummary", summarizeJsonRows(root)},
  };
  if (root.is_object()) {
    nlohmann::json keys = nlohmann::json::array();
    for (auto it = root.begin(); it != root.end(); ++it)
      keys.push_back(it.key());
    preview.payload["topLevelKeys"] = std::move(keys);
  }
  return preview;
}

AssetPreview previewCurveFile(const std::filesystem::path& path, const PakFileEntry& entry) {
  AssetPreview preview;
  preview.kind = PreviewKind::Curve;
  preview.sourcePak = entry.sourcePak;
  preview.virtualPath = entry.virtualPath;

  const auto keys = Mod::readCurveKeys(path);
  nlohmann::json keyArray = nlohmann::json::array();
  for (const auto& k : keys)
    keyArray.push_back({{"time", k.time}, {"value", k.value}});
  preview.payload = {
      {"assetName", basenameFromVirtualPath(entry.virtualPath)},
      {"keys", std::move(keyArray)},
  };
  return preview;
}

AssetPreview previewTextFile(const std::filesystem::path& path, const PakFileEntry& entry) {
  AssetPreview preview;
  preview.kind = PreviewKind::Text;
  preview.sourcePak = entry.sourcePak;
  preview.virtualPath = entry.virtualPath;
  preview.payload = {
      {"text", readFileLimited(path, kMaxTextPreviewBytes)},
      {"truncated", std::filesystem::file_size(path) > kMaxTextPreviewBytes},
  };
  return preview;
}

AssetPreview previewBinaryFile(const std::filesystem::path& path, const PakFileEntry& entry) {
  AssetPreview preview;
  preview.kind = PreviewKind::Binary;
  preview.sourcePak = entry.sourcePak;
  preview.virtualPath = entry.virtualPath;

  const auto bytes = readFileLimited(path, 64);
  std::ostringstream hex;
  hex << std::hex;
  for (unsigned char b : bytes)
    hex << (b >> 4) << (b & 0xF);

  nlohmann::json interesting = nlohmann::json::array();
  nlohmann::json all = nlohmann::json::array();
  for (const auto& s : extractAsciiStrings(path, 4, 200, 256)) {
    all.push_back(s);
    if (s.find("/Game/") != std::string::npos || s.rfind("BP_", 0) == 0 ||
        s.rfind("SM_", 0) == 0 || s.rfind("DTR_", 0) == 0 ||
        s.find("Hazard") != std::string::npos)
      interesting.push_back(s);
  }

  preview.payload = {
      {"size", std::filesystem::file_size(path)},
      {"hexHead", hex.str()},
      {"note", "Binary asset; printable strings unboxed below."},
      {"strings", std::move(all)},
      {"paths", std::move(interesting)},
  };
  return preview;
}

AssetPreview previewFromExtracted(
    const std::filesystem::path& extracted,
    const PakFileEntry& entry) {
  if (entry.extension == "json")
    return previewJsonFile(extracted, entry);

  if (entry.extension == "curve" || extracted.extension() == ".curve.json")
    return previewCurveFile(extracted, entry);

  if (entry.extension == "uasset" || entry.extension == "uexp") {
    try {
      const auto keys = Mod::readCurveKeys(extracted);
      if (!keys.empty())
        return previewCurveFile(extracted, entry);
    } catch (...) {
    }
    return previewBinaryFile(extracted, entry);
  }

  static const std::vector<std::string> textExts = {"ini", "txt", "csv", "lua", "xml", "cfg"};
  if (std::find(textExts.begin(), textExts.end(), entry.extension) != textExts.end())
    return previewTextFile(extracted, entry);

  return previewBinaryFile(extracted, entry);
}

}  // namespace

std::filesystem::path extractEntryToDir(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::filesystem::path& outputDirectory) {
  return extractExactEntry(entry, pakPathOnDisk, options, outputDirectory);
}

AssetPreview previewEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options,
    const std::filesystem::path& /*tempDir*/) {
  const auto extracted = openCachedEntry(entry, pakPathOnDisk, options);
  return previewFromExtracted(extracted, entry);
}

AssetPreview openEntry(
    const PakFileEntry& entry,
    const std::filesystem::path& pakPathOnDisk,
    const UnrealPakOptions& options) {
  const auto extracted = openCachedEntry(entry, pakPathOnDisk, options);
  auto preview = previewFromExtracted(extracted, entry);
  preview.payload["extractedPath"] = extracted.string();
  preview.payload["cached"] = true;
  return preview;
}

const char* previewKindName(PreviewKind kind) {
  switch (kind) {
    case PreviewKind::Json:
      return "json";
    case PreviewKind::Curve:
      return "curve";
    case PreviewKind::Text:
      return "text";
    case PreviewKind::Binary:
      return "binary";
    default:
      return "unsupported";
  }
}

std::string generateSnippet(const AssetPreview& preview, const SnippetRequest& request) {
  const std::string assetName = basenameFromVirtualPath(preview.virtualPath);

  if (preview.kind == PreviewKind::Json) {
    std::string row = request.row.value_or("");
    std::string field = request.field.value_or("");

    if (row.empty() && preview.payload.contains("rowSummary") &&
        preview.payload["rowSummary"].is_object() && !preview.payload["rowSummary"].empty()) {
      row = preview.payload["rowSummary"].begin().key();
      if (field.empty()) {
        const auto& rowObj = preview.payload["rowSummary"][row];
        if (rowObj.is_object() && !rowObj.empty())
          field = rowObj.begin().key();
      }
    }

    std::ostringstream out;
    out << "utool.asset(\"" << assetName << "\")";
    if (!row.empty())
      out << "\n  :row(\"" << row << "\")";
    if (!field.empty())
      out << "\n  :field(\"" << field << "\")";
    if (request.value)
      out << "\n  :set(" << request.value->dump() << ")";
    else
      out << "\n  :set(0)";
    return out.str();
  }

  if (preview.kind == PreviewKind::Curve) {
    const std::string curveName =
        preview.payload.value("assetName", std::filesystem::path(assetName).stem().string());
    std::ostringstream out;
    out << "utool.patch_curve(\"" << curveName << "\", {\n";
    if (preview.payload.contains("keys") && preview.payload["keys"].is_array()) {
      for (const auto& k : preview.payload["keys"]) {
        out << "  { time = " << k.value("time", 0.f) << ", value = "
            << k.value("value", 0.f) << " },\n";
      }
    }
    out << "})";
    return out.str();
  }

  return "-- No snippet available for " + std::string(previewKindName(preview.kind)) +
         " preview of " + assetName;
}

}  // namespace UTool::Pak
