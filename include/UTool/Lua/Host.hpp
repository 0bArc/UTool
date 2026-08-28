#pragma once

#include "UTool/Core/Config.hpp"
#include "UTool/Mod/CurveFloat.hpp"
#include "UTool/Mod/JsonEditor.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace sol {
class state;
}

namespace UTool::Lua {

struct CurveRegistration {
  std::string assetName;
  std::string relativeDirectory;
  bool extendFromVanilla = true;
  std::function<void(Mod::CurveEditor&)> apply;
};

struct AssetRegistration {
  std::string assetFileName;
  std::string relativeDirectory;
  std::function<void(Mod::JsonAssetEditor&)> apply;
};

struct FieldCriterion {
  std::string property;
  nlohmann::json value;
};

struct FieldMutation {
  std::string assetFileName;
  std::string relativeDirectory;
  std::string collection{"Rows"};
  std::vector<FieldCriterion> criteria;
  std::string property;
  nlohmann::json value;
};

struct PakVariant {
  FieldMutation mutation;
  std::string assetFileName;
  std::string relativeDirectory;
  std::function<void(Mod::JsonAssetEditor&)> assetApply;
  std::int64_t valueNumber = 0;
  bool zip = false;
  /// Empty → `{pakStem}.zip` next to the pak. Otherwise expand like pak.output.
  std::optional<std::string> zipTemplate;
};

struct ScriptRegistrations {
  std::shared_ptr<sol::state> lua;
  std::vector<CurveRegistration> curves;
  std::vector<AssetRegistration> assets;
  std::vector<FieldMutation> fieldSets;
  std::vector<PakVariant> pakVariants;
  std::optional<Core::ModManifest> modManifest;
};

[[nodiscard]] ScriptRegistrations loadModScripts(const std::vector<std::filesystem::path>& scripts);

[[nodiscard]] std::string expandOutputTemplate(
    std::string templ,
    const std::optional<std::string>& updateVersion,
    const std::optional<std::int64_t>& value);

/// True when template contains %d or any %name% other than %updateversion%.
[[nodiscard]] bool outputTemplateNeedsVariant(std::string_view templ);

void applyFieldMutation(Mod::JsonAssetEditor& editor, const FieldMutation& mutation);

}  // namespace UTool::Lua
