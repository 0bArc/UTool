#pragma once

#include "UTool/Mod/CurveFloat.hpp"
#include "UTool/Mod/JsonEditor.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

struct ScriptRegistrations {
  /// Keeps the Lua VM alive for the duration of the returned callbacks.
  std::shared_ptr<sol::state> lua;
  std::vector<CurveRegistration> curves;
  std::vector<AssetRegistration> assets;
};

[[nodiscard]] ScriptRegistrations loadModScripts(const std::vector<std::filesystem::path>& scripts);

}  // namespace UTool::Lua
