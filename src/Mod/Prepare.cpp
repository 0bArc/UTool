#include "UTool/Mod/Prepare.hpp"

#include "UTool/Core/Config.hpp"
#include "UTool/Core/ModSetup.hpp"
#include "UTool/Lua/Host.hpp"
#include "UTool/Mod/JsonEditor.hpp"
#include "UTool/Pak/Inventory.hpp"
#include "UTool/Pak/UnrealPak.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace UTool::Mod {
namespace {

std::string readText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("Cannot read " + path.string());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void writeText(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out)
    throw std::runtime_error("Cannot write " + path.string());
  out << text;
}

void copyFile(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::filesystem::create_directories(to.parent_path());
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
}

void copyTree(const std::filesystem::path& source, const std::filesystem::path& target) {
  for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
    if (!entry.is_regular_file())
      continue;
    const auto name = entry.path().filename().string();
    if (name.find(".utool-curve-note.txt") != std::string::npos)
      continue;
    const auto rel = std::filesystem::relative(entry.path(), source);
    copyFile(entry.path(), target / rel);
  }
}

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

void zipPakFile(const std::filesystem::path& zipPath, const std::filesystem::path& pakPath) {
  if (!std::filesystem::is_regular_file(pakPath))
    throw std::runtime_error("Cannot zip missing pak: " + pakPath.string());
  std::filesystem::create_directories(zipPath.parent_path());
  if (std::filesystem::exists(zipPath))
    std::filesystem::remove(zipPath);

  // tar is far faster than spinning up PowerShell Compress-Archive per file.
  const std::string cmd = "tar -a -cf \"" + zipPath.string() + "\" -C \"" +
                          pakPath.parent_path().string() + "\" \"" +
                          pakPath.filename().string() + "\"";
  const int code = std::system(cmd.c_str());
  if (code != 0 || !std::filesystem::is_regular_file(zipPath))
    throw std::runtime_error("Failed to create zip: " + zipPath.string());
}

std::filesystem::path resolveZipPath(
    const std::filesystem::path& pakPath,
    const Lua::PakVariant& variant,
    const Core::ModPackage& package,
    const std::optional<std::string>& updateVersion) {
  if (!variant.zipTemplate || variant.zipTemplate->empty()) {
    auto zip = pakPath;
    zip.replace_extension(".zip");
    return zip;
  }

  std::string templ = *variant.zipTemplate;
  std::string expanded = Lua::expandOutputTemplate(templ, updateVersion, variant.valueNumber);
  for (char& c : expanded) {
    if (c == '\\')
      c = '/';
  }

  std::filesystem::path zip(expanded);
  if (!zip.is_absolute()) {
    if (expanded.find('/') == std::string::npos)
      zip = pakPath.parent_path() / zip.filename();
    else
      zip = package.rootPath / zip;
  }
  if (!iequals(zip.extension().string(), ".zip"))
    zip += ".zip";
  std::filesystem::create_directories(zip.parent_path());
  return zip.lexically_normal();
}

std::string sanitizePakName(std::string id) {
  for (char& c : id) {
    if (c == '.' || c == ' ')
      c = '-';
  }
  return id;
}

bool shouldPreserveSourcePaths(std::string mount) {
  for (char& c : mount) {
    if (c == '\\')
      c = '/';
  }
  while (!mount.empty() && mount.back() == '/')
    mount.pop_back();
  // Only the data-pak root keeps Character/... subfolders. Curve mounts such as
  // .../Content/Data/Character/ must stay flat so UnrealPak mount matches Icarus.
  return mount.size() >= 13 && iequals(mount.substr(mount.size() - 13), "/Content/Data");
}

std::filesystem::path ensurePatchPakName(std::filesystem::path output,
                                         const std::optional<std::string>& gameId) {
  if (!gameId || !iequals(*gameId, "Icarus"))
    return output;
  if (!iequals(output.extension().string(), ".pak"))
    return output;
  auto name = output.stem().string();
  if (name.size() >= 2 && iequals(name.substr(name.size() - 2), "_P"))
    return output;
  return output.parent_path() / (name + "_P.pak");
}

std::optional<std::filesystem::path> findExtractedAsset(
    const std::filesystem::path& extractedDir,
    const std::string& fileName) {
  if (!std::filesystem::is_directory(extractedDir))
    return std::nullopt;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(extractedDir)) {
    if (!entry.is_regular_file())
      continue;
    if (iequals(entry.path().filename().string(), fileName))
      return entry.path();
  }
  return std::nullopt;
}

std::filesystem::path extractAssetViaUnrealPak(
    const std::filesystem::path& pakPath,
    const std::string& assetFileName,
    const std::filesystem::path& pullDir,
    const Pak::UnrealPakOptions& options) {
  if (std::filesystem::exists(pullDir))
    std::filesystem::remove_all(pullDir);
  std::filesystem::create_directories(pullDir);
  const std::string filter = "*" + std::filesystem::path(assetFileName).stem().string() + "*";
  Pak::extract(pakPath, pullDir, filter, options);

  if (auto found = findExtractedAsset(pullDir, assetFileName))
    return *found;

  const auto stem = std::filesystem::path(assetFileName).stem().string();
  throw std::runtime_error(
      "UnrealPak extract did not produce " + assetFileName + " from " + pakPath.string() +
      ". Verify the asset exists (`utool pak search " + stem +
      " --from @data`), confirm pak.sourcePak / games.*.dataPak, and that AES crypto is "
      "configured if the pak is encrypted.");
}

using PatchMap = std::unordered_map<std::string, nlohmann::json>;

PatchMap loadJsonPatches(const Core::ModPackage& package) {
  PatchMap byAsset;
  for (const auto& patchRel : package.manifest.patchFiles) {
    const auto path = package.rootPath / patchRel;
    auto doc = nlohmann::json::parse(readText(path));
    if (!doc.contains("patches") || !doc["patches"].is_array())
      continue;
    for (const auto& patch : doc["patches"]) {
      std::string asset = patch.value("assetPath", "");
      if (asset.empty())
        asset = patch.value("asset", "");
      // Use filename for matching
      const auto base = std::filesystem::path(asset).filename().string();
      std::string key = base.empty() ? asset : base;
      if (key.size() > 5 && key.substr(key.size() - 2) == "_C") {
        // keep as-is
      }
      if (!key.empty() && key.find(".json") == std::string::npos &&
          key.find('.') == std::string::npos)
        key += ".json";

      auto ops = patch.contains("operations") ? patch["operations"] : nlohmann::json::array();
      if (!byAsset.contains(key))
        byAsset[key] = nlohmann::json::array();
      for (const auto& op : ops)
        byAsset[key].push_back(op);
    }
  }
  return byAsset;
}

std::vector<std::filesystem::path> collectScripts(const Core::ModPackage& package) {
  std::vector<std::filesystem::path> scripts;
  const auto luaManifest = package.rootPath / Core::ModManifest::LuaManifestFileName;
  if (std::filesystem::is_regular_file(luaManifest))
    scripts.push_back(luaManifest);

  for (const auto& rel : package.manifest.scripts) {
    const auto path = package.rootPath / rel;
    if (!scripts.empty() && path.lexically_normal() == scripts.front().lexically_normal())
      continue;
    scripts.push_back(path);
  }

  if (!scripts.empty())
    return scripts;

  const auto scriptsDir = package.rootPath / "scripts";
  if (!std::filesystem::is_directory(scriptsDir))
    return scripts;
  for (const auto& entry : std::filesystem::directory_iterator(scriptsDir)) {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() == ".lua")
      scripts.push_back(entry.path());
  }
  std::sort(scripts.begin(), scripts.end());
  return scripts;
}

bool isContentRootMount(std::string mount) {
  for (char& c : mount) {
    if (c == '\\')
      c = '/';
  }
  while (!mount.empty() && mount.back() == '/')
    mount.pop_back();
  if (mount.size() < 7)
    return false;
  return iequals(mount.substr(mount.size() - 7), "Content");
}

std::string relativeDirFromExtractedPath(
    const std::filesystem::path& found,
    const std::filesystem::path& pullRoot) {
  auto parent = std::filesystem::relative(found.parent_path(), pullRoot).generic_string();
  for (char& c : parent) {
    if (c == '\\')
      c = '/';
  }
  if (parent == "." || parent.empty())
    return {};
  if (parent.rfind("data/", 0) == 0 || parent.rfind("Data/", 0) == 0)
    return parent;
  return "data/" + parent;
}

std::string resolveAssetRelativeDirectory(
    const std::string& assetFile,
    const std::string& hintedRelative,
    const Core::ModPackage& package,
    const PrepareOptions& options) {
  if (!hintedRelative.empty())
    return hintedRelative;

  // Mount already under Content/.../Character (etc.): keep prepared files flat.
  if (!isContentRootMount(options.mountPoint))
    return {};

  if (options.extractedDir) {
    if (auto found = findExtractedAsset(*options.extractedDir, assetFile)) {
      auto rel = relativeDirFromExtractedPath(*found, *options.extractedDir);
      if (!rel.empty())
        return rel;
    }
  }

  if (options.sourcePak) {
    const auto pullDir =
        package.rootPath / ".cache" / "ue-extract" / std::filesystem::path(assetFile).stem();
    const auto found =
        extractAssetViaUnrealPak(*options.sourcePak, assetFile, pullDir, options.unrealPak);
    return relativeDirFromExtractedPath(found, pullDir);
  }

  return {};
}


}  // namespace

std::filesystem::path mergeForPack(
    const Core::ModPackage& package,
    const std::filesystem::path& preparedDir) {
  std::vector<std::filesystem::path> roots;
  for (const auto& r : package.manifest.contentRoots) {
    const auto p = package.rootPath / r;
    if (std::filesystem::is_directory(p))
      roots.push_back(p);
  }

  const bool hasPrepared =
      std::filesystem::is_directory(preparedDir) &&
      std::filesystem::directory_iterator(preparedDir) != std::filesystem::directory_iterator{};

  if (roots.empty() && !hasPrepared)
    throw std::runtime_error("No packable content for mod '" + package.manifest.id + "'.");
  if (roots.empty())
    return preparedDir;
  if (!hasPrepared)
    return roots.size() == 1 ? roots[0] : [&] {
      const auto merged = package.rootPath / ".cache" / "pack-content";
      if (std::filesystem::exists(merged))
        std::filesystem::remove_all(merged);
      std::filesystem::create_directories(merged);
      for (const auto& root : roots)
        copyTree(root, merged);
      return merged;
    }();

  const auto merged = package.rootPath / ".cache" / "pack-content";
  if (std::filesystem::exists(merged))
    std::filesystem::remove_all(merged);
  std::filesystem::create_directories(merged);
  copyTree(preparedDir, merged);
  for (const auto& root : roots)
    copyTree(root, merged);
  return merged;
}

PrepareResult prepareMod(
    const Core::ModPackage& package,
    const Core::Config& config,
    const PrepareOptions& options) {
  (void)config;
  PrepareResult result;
  const auto preparedRoot = package.rootPath / ".cache" / "prepared";
  if (std::filesystem::exists(preparedRoot))
    std::filesystem::remove_all(preparedRoot);
  std::filesystem::create_directories(preparedRoot);

  try {
    auto scripts = collectScripts(package);
    Lua::ScriptRegistrations regs;
    if (!scripts.empty())
      regs = Lua::loadModScripts(scripts);

    auto jsonPatches = loadJsonPatches(package);

    // Merge lua asset registrations into patch list keys
    for (const auto& assetReg : regs.assets) {
      if (!jsonPatches.contains(assetReg.assetFileName))
        jsonPatches[assetReg.assetFileName] = nlohmann::json::array();
    }
    for (const auto& fieldSet : regs.fieldSets) {
      if (!jsonPatches.contains(fieldSet.assetFileName))
        jsonPatches[fieldSet.assetFileName] = nlohmann::json::array();
    }
    if (options.pakVariantIndex) {
      if (*options.pakVariantIndex >= regs.pakVariants.size())
        throw std::runtime_error("Invalid pak variant index");
      const auto& variant = regs.pakVariants[*options.pakVariantIndex];
      if (variant.assetApply) {
        if (!jsonPatches.contains(variant.assetFileName))
          jsonPatches[variant.assetFileName] = nlohmann::json::array();
      } else if (!variant.mutation.assetFileName.empty()) {
        if (!jsonPatches.contains(variant.mutation.assetFileName))
          jsonPatches[variant.mutation.assetFileName] = nlohmann::json::array();
      }
    }

    for (const auto& [assetFile, ops] : jsonPatches) {
      std::optional<std::filesystem::path> sourcePath;
      if (options.extractedDir)
        sourcePath = findExtractedAsset(*options.extractedDir, assetFile);

      if (!sourcePath && options.sourcePak) {
        const auto pullDir =
            package.rootPath / ".cache" / "ue-extract" / std::filesystem::path(assetFile).stem();
        sourcePath =
            extractAssetViaUnrealPak(*options.sourcePak, assetFile, pullDir, options.unrealPak);
      }

      if (!sourcePath) {
        throw std::runtime_error(
            "Cannot locate source JSON for " + assetFile +
            ". Set pak.sourcePak / extractedDir in utool.json.");
      }

      auto current = readText(*sourcePath);
      if (!ops.empty())
        current = applyPatchOperations(current, ops);

      for (const auto& assetReg : regs.assets) {
        if (!iequals(assetReg.assetFileName, assetFile))
          continue;
        JsonAssetEditor editor(current);
        assetReg.apply(editor);
        current = editor.toJson(true);
      }

      for (const auto& fieldSet : regs.fieldSets) {
        if (!iequals(fieldSet.assetFileName, assetFile))
          continue;
        JsonAssetEditor editor(current);
        Lua::applyFieldMutation(editor, fieldSet);
        current = editor.toJson(true);
      }

      if (options.pakVariantIndex) {
        const auto& variant = regs.pakVariants[*options.pakVariantIndex];
        if (variant.assetApply && iequals(variant.assetFileName, assetFile)) {
          JsonAssetEditor editor(current);
          variant.assetApply(editor);
          current = editor.toJson(true);
        } else if (!variant.mutation.assetFileName.empty() &&
                   iequals(variant.mutation.assetFileName, assetFile)) {
          JsonAssetEditor editor(current);
          Lua::applyFieldMutation(editor, variant.mutation);
          current = editor.toJson(true);
        }
      }

      std::string relativeDir;
      for (const auto& assetReg : regs.assets) {
        if (!iequals(assetReg.assetFileName, assetFile))
          continue;
        if (!assetReg.relativeDirectory.empty()) {
          relativeDir = assetReg.relativeDirectory;
          break;
        }
      }
      if (relativeDir.empty()) {
        for (const auto& fieldSet : regs.fieldSets) {
          if (!iequals(fieldSet.assetFileName, assetFile))
            continue;
          if (!fieldSet.relativeDirectory.empty()) {
            relativeDir = fieldSet.relativeDirectory;
            break;
          }
        }
      }
      if (relativeDir.empty() && options.pakVariantIndex) {
        const auto& variant = regs.pakVariants[*options.pakVariantIndex];
        if (variant.assetApply && iequals(variant.assetFileName, assetFile))
          relativeDir = variant.relativeDirectory;
        else if (iequals(variant.mutation.assetFileName, assetFile))
          relativeDir = variant.mutation.relativeDirectory;
      }
      relativeDir =
          resolveAssetRelativeDirectory(assetFile, relativeDir, package, options);

      std::filesystem::path target = preparedRoot / assetFile;
      if (!relativeDir.empty())
        target = preparedRoot / relativeDir / assetFile;
      writeText(target, current);
      result.preparedFiles.push_back(target);
      std::cout << "prepared: " << target.string() << '\n';
    }

    // Curves from *.curve.json
    const auto curvesDir =
        package.rootPath / (package.manifest.curvePatchesDir.value_or("curves"));
    auto curveSpecs = readCurveJsonDirectory(curvesDir);

    // Curves from Lua
    for (const auto& curveReg : regs.curves) {
      if (options.curveSourcePaks.empty())
        throw std::runtime_error(
            "Mod has Lua curve patches but no curve source pak. Set pak.curveSourcePak (e.g. @paks).");

      std::string assetFile = curveReg.assetName;
      if (assetFile.size() < 7 || !iequals(assetFile.substr(assetFile.size() - 7), ".uasset"))
        assetFile += ".uasset";

      const auto cacheDir = package.rootPath / ".cache" / "curve-source" / curveReg.relativeDirectory;
      const auto cachedUasset = cacheDir / assetFile;
      if (options.forceExtract || !std::filesystem::is_regular_file(cachedUasset)) {
        std::filesystem::path found;
        for (const auto& pak : options.curveSourcePaks) {
          try {
            const auto pullDir =
                package.rootPath / ".cache" / "curve-source" / ".pull" /
                std::filesystem::path(assetFile).stem();
            found = extractAssetViaUnrealPak(pak, assetFile, pullDir, options.unrealPak);
            break;
          } catch (...) {
            // try next pak
          }
        }
        if (found.empty())
          throw std::runtime_error("Failed to extract curve asset " + assetFile);
        std::filesystem::create_directories(cacheDir);
        copyFile(found, cachedUasset);
        const auto foundUexp = found.replace_extension(".uexp");
        if (std::filesystem::is_regular_file(foundUexp))
          copyFile(foundUexp, cachedUasset.parent_path() / (cachedUasset.stem().string() + ".uexp"));
      }

      auto vanillaKeys = readCurveKeys(cachedUasset);
      CurveEditor editor(curveReg.assetName, vanillaKeys);
      curveReg.apply(editor);

      CurveFloatPatchSpec spec;
      spec.assetName = curveReg.assetName;
      spec.relativeDirectory = curveReg.relativeDirectory;
      spec.extendFromVanilla = curveReg.extendFromVanilla;

      auto findValueAt = [](const std::vector<CurveKey>& keys, float time) -> std::optional<float> {
        for (const auto& k : keys) {
          if (std::fabs(k.time - time) < 1e-4f)
            return k.value;
        }
        return std::nullopt;
      };

      bool mutatedVanilla = false;
      for (const auto& vk : vanillaKeys) {
        const auto edited = findValueAt(editor.keys(), vk.time);
        if (!edited || std::fabs(*edited - vk.value) > 1e-3f) {
          mutatedVanilla = true;
          break;
        }
      }

      if (mutatedVanilla || !curveReg.extendFromVanilla || vanillaKeys.empty()) {
        spec.extendFromVanilla = false;
        spec.minPatchTime = std::nullopt;
        spec.keys = editor.keys();
      } else {
        const float vanillaMax =
            std::max_element(vanillaKeys.begin(), vanillaKeys.end(),
                             [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; })
                ->time;
        spec.minPatchTime = vanillaMax;
        for (const auto& k : editor.keys()) {
          if (k.time > vanillaMax + 1e-4f)
            spec.keys.push_back(k);
        }
      }

      const bool nestUnderRelative =
          !curveReg.relativeDirectory.empty() &&
          (options.preserveSourcePaths || isContentRootMount(options.mountPoint));
      const auto outRoot =
          nestUnderRelative ? preparedRoot / std::filesystem::path(curveReg.relativeDirectory)
                            : preparedRoot;
      std::filesystem::create_directories(outRoot);
      const auto outUasset = outRoot / assetFile;
      copyFile(cachedUasset, outUasset);
      const auto cachedUexp = cachedUasset.parent_path() / (cachedUasset.stem().string() + ".uexp");
      const auto outUexp = outUasset.parent_path() / (outUasset.stem().string() + ".uexp");
      if (std::filesystem::is_regular_file(cachedUexp))
        copyFile(cachedUexp, outUexp);

      applyCurveKeys(outUasset, spec);
      result.preparedFiles.push_back(outUasset);
      if (std::filesystem::is_regular_file(outUexp))
        result.preparedFiles.push_back(outUexp);
      std::cout << "curve prepared: " << outUasset.string() << '\n';
    }

    for (const auto& spec : curveSpecs) {
      if (options.curveSourcePaks.empty())
        throw std::runtime_error("curve json patches require pak.curveSourcePak");

      std::string assetFile = spec.assetName;
      if (assetFile.size() < 7 || !iequals(assetFile.substr(assetFile.size() - 7), ".uasset"))
        assetFile += ".uasset";

      const auto cacheDir = package.rootPath / ".cache" / "curve-source" / spec.relativeDirectory;
      const auto cachedUasset = cacheDir / assetFile;
      if (!std::filesystem::is_regular_file(cachedUasset)) {
        std::filesystem::path found;
        for (const auto& pak : options.curveSourcePaks) {
          try {
            const auto pullDir =
                package.rootPath / ".cache" / "curve-source" / ".pull" /
                std::filesystem::path(assetFile).stem();
            found = extractAssetViaUnrealPak(pak, assetFile, pullDir, options.unrealPak);
            break;
          } catch (...) {
          }
        }
        if (found.empty())
          throw std::runtime_error("Failed to extract " + assetFile);
        std::filesystem::create_directories(cacheDir);
        copyFile(found, cachedUasset);
        auto foundUexp = found;
        foundUexp.replace_extension(".uexp");
        if (std::filesystem::is_regular_file(foundUexp))
          copyFile(foundUexp, cacheDir / (std::filesystem::path(assetFile).stem().string() + ".uexp"));
      }

      const bool nestUnderRelative =
          !spec.relativeDirectory.empty() &&
          (options.preserveSourcePaths || isContentRootMount(options.mountPoint));
      const auto outRoot =
          nestUnderRelative ? preparedRoot / std::filesystem::path(spec.relativeDirectory)
                            : preparedRoot;
      std::filesystem::create_directories(outRoot);
      const auto outUasset = outRoot / assetFile;
      copyFile(cachedUasset, outUasset);
      const auto cachedUexp = cacheDir / (std::filesystem::path(assetFile).stem().string() + ".uexp");
      const auto outUexp = outRoot / (std::filesystem::path(assetFile).stem().string() + ".uexp");
      if (std::filesystem::is_regular_file(cachedUexp))
        copyFile(cachedUexp, outUexp);
      applyCurveKeys(outUasset, spec);
      result.preparedFiles.push_back(outUasset);
      std::cout << "curve prepared: " << outUasset.string() << '\n';
    }


    result.ok = true;
    result.preparedContentDir = preparedRoot;
    result.message = "prepared " + std::to_string(result.preparedFiles.size()) + " file(s)";
  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

BuildModResult buildMod(
    const Core::ModPackage& package,
    const Core::Config& config,
    const std::optional<std::filesystem::path>& outputOverride,
    const std::optional<std::string>& mountOverride,
    bool compress,
    bool forceExtract) {
  BuildModResult result;
  try {
    std::optional<std::string> gameId =
        package.manifest.target ? package.manifest.target->gameId : std::nullopt;

    auto isAutoMount = [](std::string_view value) {
      return value.empty() || value == "@auto";
    };

    std::string mount;
    if (mountOverride && !isAutoMount(*mountOverride))
      mount = *mountOverride;
    else if (package.manifest.pak && package.manifest.pak->mountPoint &&
             !isAutoMount(*package.manifest.pak->mountPoint) && !mountOverride)
      mount = *package.manifest.pak->mountPoint;
    else
      mount = Core::resolveAutoMountPoint(config, gameId);

    const bool useUePack = (package.manifest.pak && package.manifest.pak->useUnrealPak) ||
                           (package.manifest.pak && package.manifest.pak->sourcePak) || true;

    const auto scripts = collectScripts(package);
    const auto curvesDir =
        package.rootPath / (package.manifest.curvePatchesDir.value_or("curves"));
    const bool hasJsonCurves =
        std::filesystem::is_directory(curvesDir) &&
        std::filesystem::directory_iterator(curvesDir) != std::filesystem::directory_iterator{};

    Lua::ScriptRegistrations regs;
    if (!scripts.empty())
      regs = Lua::loadModScripts(scripts);

    const bool shouldPrepare =
        !package.manifest.patchFiles.empty() || !scripts.empty() || hasJsonCurves;

    const auto updateVersion = package.manifest.updateVersion;
    const bool multiVariant = !regs.pakVariants.empty() && !outputOverride;

    const auto pakCtx = Pak::makeResolveContext(config, gameId);

    auto buildPrepareOptions = [&](std::optional<std::size_t> variantIndex) {
      PrepareOptions opts;
      opts.forceExtract = forceExtract;
      opts.preserveSourcePaths = shouldPreserveSourcePaths(mount);
      opts.extractedDir = config.resolveExistingExtractedDir();
      opts.pakVariantIndex = variantIndex;
      opts.mountPoint = mount;
      opts.unrealPak = pakCtx.unrealPak;

      std::optional<std::string> sourceToken =
          package.manifest.pak ? package.manifest.pak->sourcePak : std::nullopt;
      if ((!sourceToken || sourceToken->empty()) &&
          (!package.manifest.patchFiles.empty() || !scripts.empty()))
        sourceToken = "@data";
      if (sourceToken)
        opts.sourcePak = config.resolveSourcePak(sourceToken, gameId);

      std::optional<std::string> curveToken =
          package.manifest.pak && package.manifest.pak->curveSourcePak
              ? package.manifest.pak->curveSourcePak
              : std::string("@paks");
      if (!scripts.empty() || hasJsonCurves)
        opts.curveSourcePaks = config.resolveSourcePakPaths(curveToken, gameId);
      return opts;
    };

    auto resolveOutputPath = [&](std::optional<std::int64_t> valueNumber) {
      std::filesystem::path output;
      if (outputOverride) {
        output = *outputOverride;
      } else if (package.manifest.pak && package.manifest.pak->output) {
        const auto expanded =
            Lua::expandOutputTemplate(*package.manifest.pak->output, updateVersion, valueNumber);
        output = package.rootPath / expanded;
      } else {
        output = package.rootPath / "dist" / (sanitizePakName(package.manifest.id) + "_P.pak");
      }
      if (!output.is_absolute())
        output = package.rootPath / output;
      return std::filesystem::weakly_canonical(ensurePatchPakName(output, gameId));
    };

    auto packOnce = [&](std::optional<std::size_t> variantIndex,
                        std::optional<std::int64_t> valueNumber) {
      std::filesystem::path contentRoot;
      if (shouldPrepare) {
        auto prepared = prepareMod(package, config, buildPrepareOptions(variantIndex));
        if (!prepared.ok)
          throw std::runtime_error(prepared.message);
        contentRoot = mergeForPack(package, prepared.preparedContentDir);
      } else {
        contentRoot = mergeForPack(package, package.rootPath / ".cache" / "prepared");
      }

      auto output = resolveOutputPath(valueNumber);
      std::filesystem::create_directories(output.parent_path());

      if (useUePack) {
        Pak::packDirectory(contentRoot, output, mount, compress, pakCtx.unrealPak);
        std::cout << "Built mod pak (UnrealPak): " << output.string() << '\n';
      }
      return output;
    };

    std::filesystem::path lastOutput;

    auto zipAndRemovePak = [&](const Lua::PakVariant& variant, const std::filesystem::path& pakPath) {
      if (!variant.zip)
        return;
      const auto zipPath = resolveZipPath(pakPath, variant, package, updateVersion);
      zipPakFile(zipPath, pakPath);
      std::error_code ec;
      std::filesystem::remove(pakPath, ec);
      std::cout << "Zipped: " << zipPath.string() << '\n';
    };

    if (multiVariant) {
      for (std::size_t i = 0; i < regs.pakVariants.size(); ++i) {
        lastOutput = packOnce(i, regs.pakVariants[i].valueNumber);
        zipAndRemovePak(regs.pakVariants[i], lastOutput);
        if (regs.pakVariants[i].zip) {
          auto z = resolveZipPath(lastOutput, regs.pakVariants[i], package, updateVersion);
          lastOutput = z;
        }
      }
    } else {
      lastOutput = packOnce(std::nullopt, std::nullopt);
      if (package.manifest.pak && package.manifest.pak->zip) {
        Lua::PakVariant variant;
        variant.zip = true;
        variant.zipTemplate = package.manifest.pak->zipTemplate;
        zipAndRemovePak(variant, lastOutput);
        lastOutput = resolveZipPath(lastOutput, variant, package, updateVersion);
      }
    }

    const bool keepCache = package.manifest.pak && package.manifest.pak->keepCache;
    if (!keepCache) {
      const auto cache = package.rootPath / ".cache";
      if (std::filesystem::exists(cache))
        std::filesystem::remove_all(cache);
    }

    result.ok = true;
    result.outputPak = lastOutput;
    result.message = "ok";

  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

DeployModResult deployMod(const Core::ModPackage& package, const Core::Config& config) {
  DeployModResult result;
  try {
    std::optional<std::string> gameId =
        package.manifest.target ? package.manifest.target->gameId : std::nullopt;

    auto built = buildMod(package, config);
    if (!built.ok)
      throw std::runtime_error(built.message);

    auto paksOpt = config.resolvePaksDir(gameId);
    if (!paksOpt)
      throw std::runtime_error("paksDir not configured for deploy");
    const auto modsPakDir = *paksOpt / "mods";
    std::filesystem::create_directories(modsPakDir);
    const auto pakDest = modsPakDir / built.outputPak.filename();
    copyFile(built.outputPak, pakDest);
    result.pakDest = pakDest;
    std::cout << "Deployed pak: " << pakDest.string() << '\n';


    result.ok = true;
    result.message = "ok";
  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

}  // namespace UTool::Mod
