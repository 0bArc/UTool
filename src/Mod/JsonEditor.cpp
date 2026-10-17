#include "UTool/Mod/JsonEditor.hpp"

#include <cctype>
#include <functional>
#include <stdexcept>
#include <vector>

namespace UTool::Mod {
namespace {

std::string unescape(std::string segment) {
  std::string out;
  out.reserve(segment.size());
  for (size_t i = 0; i < segment.size(); ++i) {
    if (segment[i] == '~' && i + 1 < segment.size()) {
      if (segment[i + 1] == '1') {
        out.push_back('/');
        ++i;
        continue;
      }
      if (segment[i + 1] == '0') {
        out.push_back('~');
        ++i;
        continue;
      }
    }
    out.push_back(segment[i]);
  }
  return out;
}

std::vector<std::string> splitPointer(std::string_view pointer) {
  if (pointer.empty() || pointer[0] != '/')
    throw std::runtime_error("JSON pointer must start with '/'");
  std::vector<std::string> segments;
  size_t i = 1;
  while (i <= pointer.size()) {
    size_t j = i;
    while (j < pointer.size() && pointer[j] != '/')
      ++j;
    if (j > i)
      segments.push_back(unescape(std::string(pointer.substr(i, j - i))));
    i = j + 1;
  }
  return segments;
}

nlohmann::json* navigate(nlohmann::json& node, const std::string& segment) {
  if (node.is_object()) {
    auto it = node.find(segment);
    if (it == node.end())
      return nullptr;
    return &(*it);
  }
  if (node.is_array()) {
    size_t index = 0;
    try {
      index = static_cast<size_t>(std::stoul(segment));
    } catch (...) {
      return nullptr;
    }
    if (index >= node.size())
      return nullptr;
    return &node.at(index);
  }
  return nullptr;
}

void assignChild(nlohmann::json& parent, const std::string& segment, nlohmann::json child) {
  if (parent.is_object()) {
    parent[segment] = std::move(child);
    return;
  }
  if (parent.is_array()) {
    const size_t index = static_cast<size_t>(std::stoul(segment));
    if (index == parent.size())
      parent.push_back(std::move(child));
    else
      parent.at(index) = std::move(child);
    return;
  }
  throw std::runtime_error("Cannot assign child under non-container");
}

std::pair<nlohmann::json*, std::string> resolveParent(
    nlohmann::json& root,
    std::string_view pointer,
    bool createMissing) {
  auto segments = splitPointer(pointer);
  if (segments.empty())
    throw std::runtime_error("Empty JSON pointer");

  nlohmann::json* current = &root;
  for (size_t i = 0; i + 1 < segments.size(); ++i) {
    auto* next = navigate(*current, segments[i]);
    if (!next) {
      if (!createMissing)
        throw std::runtime_error("Missing segment in " + std::string(pointer));
      assignChild(*current, segments[i], nlohmann::json::object());
      next = navigate(*current, segments[i]);
    }
    current = next;
  }
  return {current, segments.back()};
}

void setAtPointer(
    nlohmann::json& root,
    std::string_view pointer,
    const nlohmann::json& value,
    bool /*replace*/,
    bool createMissing) {
  auto [parent, key] = resolveParent(root, pointer, createMissing);
  if (parent->is_object()) {
    (*parent)[key] = value;
    return;
  }
  if (parent->is_array()) {
    if (key == "-") {
      parent->push_back(value);
      return;
    }
    const size_t index = static_cast<size_t>(std::stoul(key));
    parent->at(index) = value;
    return;
  }
  throw std::runtime_error("Cannot set value at pointer parent");
}

void removeAtPointer(nlohmann::json& root, std::string_view pointer) {
  auto [parent, key] = resolveParent(root, pointer, false);
  if (parent->is_object()) {
    parent->erase(key);
    return;
  }
  if (parent->is_array()) {
    parent->erase(parent->begin() + static_cast<std::ptrdiff_t>(std::stoul(key)));
    return;
  }
  throw std::runtime_error("Cannot remove at " + std::string(pointer));
}

void walk(nlohmann::json& node, const std::function<void(nlohmann::json&)>& visit) {
  visit(node);
  if (node.is_object()) {
    for (auto& [_, child] : node.items())
      walk(child, visit);
  } else if (node.is_array()) {
    for (auto& child : node)
      walk(child, visit);
  }
}

bool valuesEqual(const nlohmann::json& a, const nlohmann::json& b) {
  return a == b;
}

bool propertyMatches(const nlohmann::json& obj, std::string_view propertyName,
                     const nlohmann::json& expected) {
  if (!obj.is_object())
    return false;
  auto it = obj.find(std::string(propertyName));
  if (it == obj.end())
    return expected.is_null();
  return valuesEqual(*it, expected);
}

nlohmann::json& resolveArray(nlohmann::json& root, std::string_view pointer) {
  if (pointer.empty() || pointer == "/") {
    if (!root.is_array())
      throw std::runtime_error("Root is not an array");
    return root;
  }
  auto segments = splitPointer(pointer);
  nlohmann::json* current = &root;
  for (const auto& seg : segments) {
    current = navigate(*current, seg);
    if (!current)
      throw std::runtime_error("Array not found: " + std::string(pointer));
  }
  if (!current->is_array())
    throw std::runtime_error("JSON node is not an array: " + std::string(pointer));
  return *current;
}

void deepMerge(nlohmann::json& target, const nlohmann::json& overlay) {
  if (!target.is_object() || !overlay.is_object()) {
    target = overlay;
    return;
  }
  for (auto it = overlay.begin(); it != overlay.end(); ++it) {
    if (it.value().is_null()) {
      target.erase(it.key());
      continue;
    }
    if (target.contains(it.key()) && target[it.key()].is_object() && it.value().is_object())
      deepMerge(target[it.key()], it.value());
    else
      target[it.key()] = it.value();
  }
}

std::string normalizeRelative(std::string_view pointer) {
  if (pointer.empty())
    throw std::runtime_error("Property pointer is required");
  if (pointer.front() == '/')
    return std::string(pointer);
  return "/" + std::string(pointer);
}

}  // namespace

JsonAssetEditor::JsonAssetEditor(std::string json)
    : root_(nlohmann::json::parse(json)) {}

JsonAssetEditor::JsonAssetEditor(nlohmann::json root) : root_(std::move(root)) {}

nlohmann::json JsonAssetEditor::get(std::string_view jsonPointer) const {
  if (jsonPointer.empty() || jsonPointer == "/")
    return root_;
  auto segments = splitPointer(jsonPointer);
  const nlohmann::json* current = &root_;
  for (const auto& seg : segments) {
    if (current->is_object()) {
      auto it = current->find(seg);
      if (it == current->end())
        throw std::runtime_error("Missing path: " + std::string(jsonPointer));
      current = &(*it);
    } else if (current->is_array()) {
      const size_t index = static_cast<size_t>(std::stoul(seg));
      if (index >= current->size())
        throw std::runtime_error("Missing path: " + std::string(jsonPointer));
      current = &current->at(index);
    } else {
      throw std::runtime_error("Missing path: " + std::string(jsonPointer));
    }
  }
  return *current;
}

void JsonAssetEditor::replace(std::string_view jsonPointer, const nlohmann::json& value) {
  setAtPointer(root_, jsonPointer, value, true, false);
}

void JsonAssetEditor::add(std::string_view jsonPointer, const nlohmann::json& value) {
  setAtPointer(root_, jsonPointer, value, false, false);
}

void JsonAssetEditor::set(std::string_view jsonPointer, const nlohmann::json& value) {
  setAtPointer(root_, jsonPointer, value, true, true);
}

void JsonAssetEditor::append(std::string_view arrayPointer, const nlohmann::json& value) {
  resolveArray(root_, arrayPointer).push_back(value);
}

void JsonAssetEditor::mergeInto(std::string_view jsonPointer, const nlohmann::json& value) {
  if (!value.is_object())
    throw std::runtime_error("Merge value must be a JSON object");
  auto segments = splitPointer(jsonPointer);
  nlohmann::json* current = &root_;
  for (const auto& seg : segments) {
    auto* next = navigate(*current, seg);
    if (!next) {
      assignChild(*current, seg, nlohmann::json::object());
      next = navigate(*current, seg);
    }
    current = next;
  }
  if (!current->is_object())
    throw std::runtime_error("Merge target is not an object");
  deepMerge(*current, value);
}

void JsonAssetEditor::remove(std::string_view jsonPointer) {
  removeAtPointer(root_, jsonPointer);
}

void JsonAssetEditor::replaceAll(
    std::string_view propertyName,
    const nlohmann::json& value,
    std::optional<std::string> underPointer) {
  nlohmann::json* subtree = &root_;
  if (underPointer && !underPointer->empty()) {
    auto segments = splitPointer(*underPointer);
    for (const auto& seg : segments) {
      subtree = navigate(*subtree, seg);
      if (!subtree)
        throw std::runtime_error("Subtree not found: " + *underPointer);
    }
  }
  const std::string name(propertyName);
  walk(*subtree, [&](nlohmann::json& node) {
    if (node.is_object() && node.contains(name))
      node[name] = value;
  });
}

int JsonAssetEditor::mapArray(
    std::string_view arrayPointer,
    const std::function<nlohmann::json(nlohmann::json)>& mapper) {
  if (!mapper)
    throw std::runtime_error("MapArray mapper is required");
  auto& arr = resolveArray(root_, arrayPointer);
  int updated = 0;
  for (auto& item : arr) {
    item = mapper(item);
    ++updated;
  }
  return updated;
}

int JsonAssetEditor::removeArrayElementsWhere(
    std::string_view arrayPointer,
    std::string_view matchProperty,
    const nlohmann::json& matchValue) {
  auto& arr = resolveArray(root_, arrayPointer);
  int removed = 0;
  for (int i = static_cast<int>(arr.size()) - 1; i >= 0; --i) {
    if (arr[i].is_object() && propertyMatches(arr[i], matchProperty, matchValue)) {
      arr.erase(arr.begin() + i);
      ++removed;
    }
  }
  return removed;
}

int JsonAssetEditor::setOnArrayElementsWhere(
    std::string_view arrayPointer,
    std::string_view matchProperty,
    const nlohmann::json& matchValue,
    std::string_view propertyPointer,
    const nlohmann::json& value) {
  auto& arr = resolveArray(root_, arrayPointer);
  const auto pointer = normalizeRelative(propertyPointer);
  int updated = 0;
  for (auto& item : arr) {
    if (!item.is_object() || !propertyMatches(item, matchProperty, matchValue))
      continue;
    setAtPointer(item, pointer, value, true, true);
    ++updated;
  }
  return updated;
}

std::string JsonAssetEditor::toJson(bool pretty) const {
  return root_.dump(pretty ? 2 : -1);
}

std::string applyPatchOperations(std::string json, const nlohmann::json& operations) {
  JsonAssetEditor editor(std::move(json));
  if (!operations.is_array())
    throw std::runtime_error("patch operations must be an array");

  for (const auto& op : operations) {
    const std::string kind = op.value("op", "");
    const std::string path = op.value("path", "");
    std::string lower = kind;
    for (char& c : lower)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower == "replace") {
      editor.replace(path, op.contains("value") ? op["value"] : nlohmann::json());
    } else if (lower == "add") {
      editor.add(path, op.contains("value") ? op["value"] : nlohmann::json());
    } else if (lower == "append") {
      editor.append(path, op.contains("value") ? op["value"] : nlohmann::json());
    } else if (lower == "merge") {
      editor.mergeInto(path, op.at("value"));
    } else if (lower == "remove") {
      editor.remove(path);
    } else if (lower == "replaceall") {
      std::string prop = path;
      while (!prop.empty() && prop.front() == '/')
        prop.erase(prop.begin());
      const auto slash = prop.find_last_of('/');
      if (slash != std::string::npos)
        prop = prop.substr(slash + 1);
      editor.replaceAll(prop, op.contains("value") ? op["value"] : nlohmann::json());
    } else if (lower == "removewhere") {
      editor.removeArrayElementsWhere(path, op.at("matchProperty").get<std::string>(),
                                      op.contains("matchValue") ? op["matchValue"] : nlohmann::json());
    } else if (lower == "setwhere") {
      editor.setOnArrayElementsWhere(
          path,
          op.at("matchProperty").get<std::string>(),
          op.contains("matchValue") ? op["matchValue"] : nlohmann::json(),
          op.at("targetPath").get<std::string>(),
          op.contains("value") ? op["value"] : nlohmann::json());
    } else {
      throw std::runtime_error("Unknown patch op: " + kind);
    }
  }
  return editor.toJson(true);
}

}  // namespace UTool::Mod
