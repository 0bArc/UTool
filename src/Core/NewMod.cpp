#include "UTool/Core/NewMod.hpp"
#include "UTool/Core/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace UTool::Core {
namespace {

std::string luaString(std::string_view value) {
  std::string out = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"')
      out += '\\';
    out += c;
  }
  out += '"';
  return out;
}

std::string pascalZipStem(std::string_view displayName) {
  std::string out;
  bool cap = true;
  for (char c : displayName) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      if (cap) {
        out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        cap = false;
      } else {
        out += c;
      }
    } else {
      cap = true;
    }
  }
  return out.empty() ? "Mod" : out;
}

std::string engineForGame(std::string_view gameId) {
  if (iequals(gameId, "Icarus"))
    return "4.27";
  return "5.4";
}

nlohmann::json readJsonFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("Failed to read " + path.string());
  nlohmann::json j;
  in >> j;
  return j;
}

void writeJsonFile(const std::filesystem::path& path, const nlohmann::json& j) {
  std::ofstream out(path);
  if (!out)
    throw std::runtime_error("Failed to write " + path.string());
  out << j.dump(2) << '\n';
}

std::string folderSlug(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (char c : input) {
    if (std::isalnum(static_cast<unsigned char>(c)))
      out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out.empty() ? "mod" : out;
}

std::string blankModLua(const NewModRequest& req, const std::string& modId, const std::string& week,
                        const std::string& zipStem) {
  std::ostringstream out;
  out << "utool.mod {\n";
  out << "  id = " << luaString(modId) << ",\n";
  out << "  name = " << luaString(req.displayName) << ",\n";
  out << "  version = \"1.0.0\",\n";
  out << "  description = " << luaString(req.description) << ",\n";
  out << "  author = " << luaString(req.author) << ",\n";
  out << "  updateVersion = " << week << ",\n";
  out << "  target = {\n";
  out << "    gameId = " << luaString(req.gameId) << ",\n";
  out << "    engineVersion = " << luaString(engineForGame(req.gameId)) << ",\n";
  out << "    minGameVersion = \"1.0.0\",\n";
  out << "  },\n";
  out << "  pak = {\n";
  out << "    mountPoint = \"@auto\",\n";
  out << "    sourcePak = \"@data\",\n";
  out << "    useUnrealPak = true,\n";
  out << "    output = \"dist/" << req.catalogId << "_%updateversion%_P.pak\",\n";
  out << "    zip = \"" << zipStem << "-Week-%updateversion%.zip\",\n";
  out << "  },\n";
  out << "}\n\n";
  out << "-- Patch assets here, e.g.:\n";
  out << "-- local table = utool.asset(\"D_Example.json\")\n";
  out << "-- table:map(\"Rows\", function(row)\n";
  out << "--   return row\n";
  out << "-- end)\n";
  return out.str();
}

std::string scaleFieldModLua(const NewModRequest& req, const std::string& modId,
                             const std::string& week, const std::string& zipStem) {
  std::vector<std::string> files = req.assetFiles;
  if (files.empty())
    files.push_back("D_Example.json");
  const std::string field = req.scaleField ? *req.scaleField : "Value";

  std::ostringstream out;
  out << "utool.mod {\n";
  out << "  id = " << luaString(modId) << ",\n";
  out << "  name = " << luaString(req.displayName) << ",\n";
  out << "  version = \"1.0.0\",\n";
  out << "  description = " << luaString(req.description) << ",\n";
  out << "  author = " << luaString(req.author) << ",\n";
  out << "  updateVersion = " << week << ",\n";
  out << "  target = {\n";
  out << "    gameId = " << luaString(req.gameId) << ",\n";
  out << "    engineVersion = " << luaString(engineForGame(req.gameId)) << ",\n";
  out << "    minGameVersion = \"1.0.0\",\n";
  out << "  },\n";
  out << "  pak = {\n";
  out << "    mountPoint = \"@auto\",\n";
  out << "    sourcePak = \"@data\",\n";
  out << "    useUnrealPak = true,\n";
  out << "    output = \"dist/" << req.catalogId << "_%updateversion%_%d_P.pak\",\n";
  out << "  },\n";
  out << "}\n\n";

  out << "local assets = {\n";
  for (const auto& f : files)
    out << "  utool.asset(" << luaString(f) << "),\n";
  out << "}\n\n";

  out << "local function scaleRow(row, mult)\n";
  out << "  local v = row." << field << "\n";
  out << "  if type(v) ~= \"number\" or v <= 0 then\n";
  out << "    return row\n";
  out << "  end\n";
  out << "  -- Divide this row only so relative values stay different.\n";
  out << "  row." << field << " = math.max(1, math.floor(v / mult + 0.5))\n";
  out << "  return row\n";
  out << "end\n\n";
  out << "local tiers = {\n";
  out << "  { mult = 2, tag = \"2x\", key = 2 },\n";
  out << "  { mult = 5, tag = \"5x\", key = 5 },\n";
  out << "  { mult = 10, tag = \"10x\", key = 10 },\n";
  out << "}\n\n";
  out << "for _, tier in ipairs(tiers) do\n";
  out << "  local mult = tier.mult\n";
  out << "  utool.pak.create(assets, function(row)\n";
  out << "    return scaleRow(row, mult)\n";
  out << "  end):Value(tier.key)\n";
  out << "    :zip(\"" << zipStem << "-Week-%updateversion%_\" .. tier.tag .. \".zip\")\n";
  out << "end\n";
  return out.str();
}

bool catalogIdMatchesFileEntry(std::string_view catalogId, std::string_view fileEntryId) {
  if (fileEntryId == catalogId)
    return true;
  const std::string prefix = std::string(catalogId) + "-";
  return fileEntryId.size() > prefix.size() &&
         fileEntryId.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

const std::vector<std::string>& knownModCategories() {
  static const std::vector<std::string> cats = {
      "survival", "inventory", "creatures", "progression", "workshop", "dev", "miscellaneous",
  };
  return cats;
}

bool isKnownModCategory(std::string_view category) {
  if (category.empty())
    return false;
  for (const auto& c : knownModCategories()) {
    if (iequals(c, category))
      return true;
  }
  // Freeform categories allowed (folder slug).
  return !folderSlug(category).empty();
}

std::vector<std::string> splitAssetFileList(std::string_view text) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&]() {
    while (!cur.empty() && (cur.front() == ' ' || cur.front() == '\t'))
      cur.erase(cur.begin());
    while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
      cur.pop_back();
    if (!cur.empty())
      out.push_back(cur);
    cur.clear();
  };
  for (char c : text) {
    if (c == ',' || c == ';' || c == '\n')
      flush();
    else
      cur += c;
  }
  flush();
  return out;
}

std::string slugifyModId(std::string_view name) {
  return folderSlug(name);
}

std::string luaModId(std::string_view gameId, std::string_view catalogId) {
  return folderSlug(gameId) + "." + std::string(catalogId);
}

std::string zipStemFromDisplayName(std::string_view displayName) {
  return pascalZipStem(displayName);
}

void writeConfigCatalogEntry(Config& config, const CatalogEntry& entry) {
  if (config.configFilePath.empty())
    throw std::runtime_error("No utool.json found to write catalog entry");

  for (const auto& existing : config.catalog) {
    if (existing.id == entry.id)
      throw std::runtime_error("Catalog already has id '" + entry.id + "'");
  }

  auto j = readJsonFile(config.configFilePath);
  if (!j.contains("catalog") || !j["catalog"].is_array())
    j["catalog"] = nlohmann::json::array();

  nlohmann::json e = {
      {"category", entry.category},
      {"id", entry.id},
      {"path", entry.path},
      {"ship", entry.ship},
  };
  if (entry.bannerTitle)
    e["bannerTitle"] = *entry.bannerTitle;
  if (entry.bannerTagline)
    e["bannerTagline"] = *entry.bannerTagline;
  j["catalog"].push_back(e);

  writeJsonFile(config.configFilePath, j);
  config.catalog.push_back(entry);
}

void writeConfigNexusFileStub(Config& config, const std::string& fileEntryId,
                              const std::string& matchGlob,
                              const std::string& displayNameTemplate,
                              std::optional<std::int64_t> nexusModId) {
  if (config.configFilePath.empty())
    throw std::runtime_error("No utool.json found to write nexus.files");

  auto j = readJsonFile(config.configFilePath);
  if (!j.contains("nexus") || !j["nexus"].is_object())
    j["nexus"] = nlohmann::json::object();
  if (!j["nexus"].contains("files") || !j["nexus"]["files"].is_array())
    j["nexus"]["files"] = nlohmann::json::array();

  for (const auto& f : j["nexus"]["files"]) {
    if (f.contains("id") && f["id"].is_string() && f["id"] == fileEntryId)
      return;
  }

  nlohmann::json entry = {
      {"archiveExisting", true},
      {"category", "main"},
      {"displayName", displayNameTemplate},
      {"fileId", 0},
      {"id", fileEntryId},
      {"match", matchGlob},
  };
  if (nexusModId && *nexusModId > 0)
    entry["modId"] = *nexusModId;

  j["nexus"]["files"].push_back(entry);
  writeJsonFile(config.configFilePath, j);

  if (!config.nexus)
    config.nexus = NexusSettings{};
  NexusFileEntry stub;
  stub.id = fileEntryId;
  stub.fileId = 0;
  stub.match = matchGlob;
  stub.displayName = displayNameTemplate;
  stub.category = "main";
  stub.archiveExisting = true;
  stub.modId = nexusModId;
  config.nexus->files.push_back(std::move(stub));
}

LinkModResult linkModToNexus(Config& config, const std::string& catalogId, std::int64_t nexusModId) {
  LinkModResult result;
  result.catalogId = catalogId;
  result.nexusModId = nexusModId;

  if (catalogId.empty() || nexusModId <= 0) {
    result.message = "Need catalog id and positive Nexus mod id";
    return result;
  }
  if (config.configFilePath.empty()) {
    result.message = "No utool.json found";
    return result;
  }

  bool inCatalog = false;
  for (const auto& e : config.catalog) {
    if (e.id == catalogId) {
      inCatalog = true;
      break;
    }
  }
  if (!inCatalog) {
    result.message = "Catalog has no id '" + catalogId + "' (check examples/utool.json)";
    return result;
  }

  auto j = readJsonFile(config.configFilePath);
  if (!j.contains("nexus") || !j["nexus"].is_object())
    j["nexus"] = nlohmann::json::object();
  if (!j["nexus"].contains("files") || !j["nexus"]["files"].is_array())
    j["nexus"]["files"] = nlohmann::json::array();

  int updated = 0;
  for (auto& f : j["nexus"]["files"]) {
    if (!f.contains("id") || !f["id"].is_string())
      continue;
    const std::string id = f["id"].get<std::string>();
    if (!catalogIdMatchesFileEntry(catalogId, id))
      continue;
    f["modId"] = nexusModId;
    ++updated;
  }

  if (updated == 0) {
    result.message =
        "No nexus.files match '" + catalogId +
        "'. Add stubs first (utool new mod / ship once), or create files with id '" + catalogId +
        "-*'.";
    return result;
  }

  writeJsonFile(config.configFilePath, j);

  if (config.nexus) {
    for (auto& f : config.nexus->files) {
      if (catalogIdMatchesFileEntry(catalogId, f.id))
        f.modId = nexusModId;
    }
  }

  result.ok = true;
  result.filesUpdated = updated;
  result.message = "Linked " + std::to_string(updated) + " nexus file(s) -> mods/" +
                   std::to_string(nexusModId);
  return result;
}

NewModResult createNewMod(Config& config, const std::filesystem::path& modsDir,
                          const NewModRequest& reqIn) {
  NewModResult result;
  NewModRequest req = reqIn;
  req.catalogId = folderSlug(req.catalogId);
  req.category = folderSlug(req.category);
  result.catalogId = req.catalogId;

  if (req.displayName.empty() || req.catalogId.empty() || req.category.empty()) {
    result.message = "name, id, and category are required";
    return result;
  }
  if (!isKnownModCategory(req.category)) {
    result.message = "Bad category '" + req.category + "'";
    return result;
  }
  for (const auto& existing : config.catalog) {
    if (existing.id == req.catalogId) {
      result.message = "Catalog id already exists: " + req.catalogId;
      return result;
    }
  }

  const auto modDir = modsDir / req.category / req.catalogId;
  std::error_code ec;
  if (std::filesystem::exists(modDir, ec)) {
    result.message = "Mod folder already exists: " + modDir.string();
    return result;
  }

  std::filesystem::create_directories(modDir / "media", ec);
  if (ec) {
    result.message = "Failed to create " + modDir.string() + ": " + ec.message();
    return result;
  }

  const std::string modId = luaModId(req.gameId, req.catalogId);
  result.modId = modId;
  const std::string week =
      config.updateVersion && !config.updateVersion->empty() ? *config.updateVersion : "0";
  const std::string zipStem = zipStemFromDisplayName(req.displayName);

  const std::string lua =
      req.tmpl == NewModTemplate::ScaleField
          ? scaleFieldModLua(req, modId, week, zipStem)
          : blankModLua(req, modId, week, zipStem);

  const auto modLuaPath = modDir / ModManifest::LuaManifestFileName;
  {
    std::ofstream out(modLuaPath);
    if (!out) {
      result.message = "Failed to write " + modLuaPath.string();
      return result;
    }
    out << lua;
  }

  const auto siblingMedia = modsDir / "survival" / "farminggrowth" / "media";
  if (std::filesystem::is_directory(siblingMedia, ec)) {
    for (const auto& name : {"banner_1300x372.png", "header_1400x400.png"}) {
      const auto src = siblingMedia / name;
      if (std::filesystem::is_regular_file(src, ec))
        std::filesystem::copy_file(src, modDir / "media" / name, ec);
    }
  }

  CatalogEntry entry;
  entry.id = req.catalogId;
  entry.path = req.category + "/" + req.catalogId;
  entry.category = req.category;
  entry.ship = req.ship;
  try {
    writeConfigCatalogEntry(config, entry);
  } catch (const std::exception& ex) {
    result.message = ex.what();
    return result;
  }

  try {
    if (req.tmpl == NewModTemplate::ScaleField) {
      for (const char* tag : {"2x", "5x", "10x"}) {
        const std::string fid = req.catalogId + "-" + tag;
        const std::string match = zipStem + "-Week-*_" + tag + ".zip";
        const std::string display = zipStem + "-Week-%updateversion%_" + tag + ".zip";
        writeConfigNexusFileStub(config, fid, match, display, req.nexusModId);
      }
    } else {
      writeConfigNexusFileStub(config, req.catalogId, zipStem + "-Week-*.zip",
                               zipStem + "-Week-%updateversion%.zip", req.nexusModId);
    }
  } catch (const std::exception& ex) {
    result.message = std::string("Mod created but nexus stub failed: ") + ex.what();
    result.ok = true;
    result.modDir = modDir;
    return result;
  }

  result.ok = true;
  result.modDir = modDir;
  result.message = "Created " + modDir.string();
  return result;
}

}  // namespace UTool::Core
