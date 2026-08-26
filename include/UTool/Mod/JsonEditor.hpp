#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace UTool::Mod {

/// Mutable JSON document for mod scripts / declarative patches.
class JsonAssetEditor {
 public:
  explicit JsonAssetEditor(std::string json);
  explicit JsonAssetEditor(nlohmann::json root);

  [[nodiscard]] nlohmann::json get(std::string_view jsonPointer) const;

  void replace(std::string_view jsonPointer, const nlohmann::json& value);
  void add(std::string_view jsonPointer, const nlohmann::json& value);
  void set(std::string_view jsonPointer, const nlohmann::json& value);
  void append(std::string_view arrayPointer, const nlohmann::json& value);
  void mergeInto(std::string_view jsonPointer, const nlohmann::json& value);
  void remove(std::string_view jsonPointer);
  void replaceAll(std::string_view propertyName, const nlohmann::json& value,
                  std::optional<std::string> underPointer = std::nullopt);

  int mapArray(std::string_view arrayPointer,
               const std::function<nlohmann::json(nlohmann::json)>& mapper);

  int removeArrayElementsWhere(
      std::string_view arrayPointer,
      std::string_view matchProperty,
      const nlohmann::json& matchValue);

  int setOnArrayElementsWhere(
      std::string_view arrayPointer,
      std::string_view matchProperty,
      const nlohmann::json& matchValue,
      std::string_view propertyPointer,
      const nlohmann::json& value);

  [[nodiscard]] std::string toJson(bool pretty = true) const;
  [[nodiscard]] const nlohmann::json& root() const { return root_; }
  nlohmann::json& root() { return root_; }

 private:
  nlohmann::json root_;
};

[[nodiscard]] std::string applyPatchOperations(std::string json, const nlohmann::json& operations);

}  // namespace UTool::Mod
