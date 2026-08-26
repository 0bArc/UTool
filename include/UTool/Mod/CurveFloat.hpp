#pragma once

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace UTool::Mod {

struct CurveKey {
  float time = 0.f;
  float value = 0.f;
};

class CurveEditor {
 public:
  CurveEditor() = default;
  explicit CurveEditor(std::string assetName, std::vector<CurveKey> vanillaKeys = {});

  [[nodiscard]] const std::string& assetName() const { return assetName_; }
  [[nodiscard]] const std::vector<CurveKey>& keys() const { return keys_; }

  [[nodiscard]] CurveKey lastKey() const;
  void addKey(float time, float value);
  void setKey(float time, float value) { addKey(time, value); }

 private:
  std::string assetName_;
  std::vector<CurveKey> keys_;
};

struct CurveFloatPatchSpec {
  std::string assetName;
  std::string relativeDirectory{"Data/Character"};
  std::vector<CurveKey> keys;
  bool extendFromVanilla = true;
  std::optional<float> minPatchTime;
};

[[nodiscard]] std::vector<CurveKey> readCurveKeysFromJson(const nlohmann::json& root);
[[nodiscard]] nlohmann::json patchRichCurveKeysJson(nlohmann::json root, const CurveFloatPatchSpec& spec);

/// Read keys from UAssetAPI-style JSON text, or from a `.curve.json` / sidecar.
[[nodiscard]] std::vector<CurveKey> readCurveKeys(const std::filesystem::path& assetOrJsonPath);

/// Apply keys to a JSON uasset export (writes JSON) or binary `.uasset` (+ `.uexp`) via
/// in-place RichCurve key array rebuild when possible.
void applyCurveKeys(const std::filesystem::path& uassetPath, const CurveFloatPatchSpec& spec);

[[nodiscard]] std::vector<CurveFloatPatchSpec> readCurveJsonDirectory(
    const std::filesystem::path& curvesDir);

}  // namespace UTool::Mod
