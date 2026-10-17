#include "UTool/Lua/Host.hpp"

#include "Modules/Registry.hpp"

#include <sol/sol.hpp>

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace UTool::Lua {
namespace {

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

bool hasVariantPlaceholder(std::string_view templ) {
  if (templ.find("%d") != std::string::npos)
    return true;
  for (std::size_t i = 0; i < templ.size(); ++i) {
    if (templ[i] != '%')
      continue;
    const auto end = templ.find('%', i + 1);
    if (end == std::string::npos)
      continue;
    const auto name = templ.substr(i + 1, end - i - 1);
    if (!name.empty() && !iequals(name, "updateversion"))
      return true;
  }
  return false;
}

std::string formatNamedPlaceholder(std::string_view name, std::int64_t value) {
  if (iequals(name, "percent"))
    return std::to_string(value) + "%";
  return std::to_string(value);
}

}  // namespace

std::string expandOutputTemplate(
    std::string templ,
    const std::optional<std::string>& updateVersion,
    const std::optional<std::int64_t>& value) {
  constexpr std::string_view kUpdate = "%updateversion%";
  constexpr std::string_view kValue = "%d";

  if (templ.find(kUpdate) != std::string::npos) {
    if (!updateVersion || updateVersion->empty())
      throw std::runtime_error("template uses %updateversion% but updateVersion is not set");
    for (std::size_t pos = 0; (pos = templ.find(kUpdate, pos)) != std::string::npos;) {
      templ.replace(pos, kUpdate.size(), *updateVersion);
      pos += updateVersion->size();
    }
  }

  if (templ.find(kValue) != std::string::npos) {
    if (!value)
      throw std::runtime_error("template uses %d but no pak.create(...):Value was provided");
    const std::string replacement = std::to_string(*value);
    for (std::size_t pos = 0; (pos = templ.find(kValue, pos)) != std::string::npos;) {
      templ.replace(pos, kValue.size(), replacement);
      pos += replacement.size();
    }
  }

  if (!value)
    return templ;

  for (std::size_t i = 0; i < templ.size();) {
    if (templ[i] != '%') {
      ++i;
      continue;
    }
    const auto end = templ.find('%', i + 1);
    if (end == std::string::npos) {
      ++i;
      continue;
    }
    const auto name = templ.substr(i + 1, end - i - 1);
    if (name.empty() || iequals(name, "updateversion")) {
      i = end + 1;
      continue;
    }
    const std::string replacement = formatNamedPlaceholder(name, *value);
    templ.replace(i, end - i + 1, replacement);
    i += replacement.size();
  }

  return templ;
}

bool outputTemplateNeedsVariant(std::string_view templ) {
  return hasVariantPlaceholder(templ);
}

void applyFieldMutation(Mod::JsonAssetEditor& editor, const FieldMutation& mutation) {
  if (mutation.criteria.empty())
    throw std::runtime_error("Field mutation requires criteria");
  if (mutation.property.empty())
    throw std::runtime_error("Field mutation requires a property");

  const std::string arrayPointer = "/" + mutation.collection;
  std::string propertyPointer = mutation.property;
  if (propertyPointer.empty() || propertyPointer.front() != '/')
    propertyPointer.insert(propertyPointer.begin(), '/');

  if (mutation.criteria.size() == 1) {
    editor.setOnArrayElementsWhere(
        arrayPointer,
        mutation.criteria[0].property,
        mutation.criteria[0].value,
        propertyPointer,
        mutation.value);
    return;
  }

  editor.mapArray(arrayPointer, [&](nlohmann::json item) {
    if (!item.is_object())
      return item;
    for (const auto& c : mutation.criteria) {
      auto it = item.find(c.property);
      if (it == item.end() || *it != c.value)
        return item;
    }
    item[mutation.property] = mutation.value;
    return item;
  });
}

ScriptRegistrations loadModScripts(const std::vector<std::filesystem::path>& scripts) {
  ScriptRegistrations regs;
  if (scripts.empty())
    return regs;

  regs.lua = std::make_shared<sol::state>();
  sol::state& lua = *regs.lua;
  lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                     sol::lib::package);

  lua.create_named_table("utool");
  registerModules(lua, regs);

  for (const auto& script : scripts) {
    auto result = lua.safe_script_file(script.string());
    if (!result.valid()) {
      sol::error err = result;
      throw std::runtime_error("Failed to load " + script.string() + ": " + err.what());
    }
  }

  return regs;
}

}  // namespace UTool::Lua
