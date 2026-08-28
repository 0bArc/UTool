#include "Data.hpp"

#include "../HostInternal.hpp"
#include "../SolJson.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace UTool::Lua {
namespace {

void splitAssetPath(const std::string& input, std::string& fileName, std::string& relativeDir) {
  std::string normalized = input;
  for (char& c : normalized) {
    if (c == '\\')
      c = '/';
  }
  const auto slash = normalized.find_last_of('/');
  if (slash == std::string::npos) {
    fileName = normalized;
    relativeDir.clear();
    return;
  }
  relativeDir = normalized.substr(0, slash);
  fileName = normalized.substr(slash + 1);
}

std::vector<FieldCriterion> criteriaFromTable(const sol::table& table) {
  std::vector<FieldCriterion> out;
  table.for_each([&](const sol::object& key, const sol::object& value) {
    if (!key.is<std::string>())
      return;
    out.push_back(FieldCriterion{key.as<std::string>(), solToJson(value)});
  });
  if (out.empty())
    throw std::runtime_error("find criteria table must contain at least one field");
  return out;
}

struct AssetRef {
  ScriptRegistrations* regs = nullptr;
  std::string fileName;
  std::string relativeDirectory;
};

struct RowRef {
  AssetRef asset;
  std::string collection{"Rows"};
  std::vector<FieldCriterion> criteria;
};

struct FieldRef {
  RowRef row;
  std::string property;
};

struct PakBuilder {
  ScriptRegistrations* regs = nullptr;
  std::optional<FieldRef> field;
  std::optional<AssetRef> asset;
  sol::protected_function assetMapFn;
  bool zip = false;
  std::optional<std::string> zipTemplate;
  std::optional<std::size_t> lastVariantIndex;
};

FieldMutation mutationFromField(const FieldRef& field, const nlohmann::json& value) {
  FieldMutation m;
  m.assetFileName = field.row.asset.fileName;
  m.relativeDirectory = field.row.asset.relativeDirectory;
  m.collection = field.row.collection;
  m.criteria = field.row.criteria;
  m.property = field.property;
  m.value = value;
  return m;
}

RowRef rowFromAsset(AssetRef asset, std::string name) {
  RowRef row;
  row.asset = std::move(asset);
  row.collection = "Rows";
  row.criteria.push_back(FieldCriterion{"Name", name});
  return row;
}

}  // namespace

void registerData(sol::state& lua, ScriptRegistrations& regs) {
  auto mapFnKeep = detail::makeFunctionKeep(lua, "__utool_map_fns");

  lua.new_usertype<AssetRef>(
      "AssetRef",
      "row",
      [](AssetRef& self, const std::string& name) { return rowFromAsset(self, name); },
      "find",
      [](AssetRef& self, const std::string& collection, const sol::table& criteria) {
        RowRef row;
        row.asset = self;
        row.collection = collection;
        row.criteria = criteriaFromTable(criteria);
        return row;
      },
      "map",
      [&regs, mapFnKeep](AssetRef& self, const std::string& collection,
                         sol::protected_function fn) {
        mapFnKeep->push_back(fn);
        auto kept = mapFnKeep->back();
        auto luaKeep = regs.lua;
        AssetRegistration reg;
        reg.assetFileName = self.fileName;
        reg.relativeDirectory = self.relativeDirectory;
        const std::string arrayPointer = "/" + collection;
        reg.apply = [kept, luaKeep, arrayPointer](Mod::JsonAssetEditor& editor) {
          sol::state_view view(*luaKeep);
          editor.mapArray(arrayPointer, [&](nlohmann::json item) {
            auto call = kept(jsonToSol(view, item));
            if (!call.valid()) {
              sol::error err = call;
              throw std::runtime_error(std::string("asset:map callback failed: ") + err.what());
            }
            return solToJson(call.get<sol::object>());
          });
        };
        regs.assets.push_back(std::move(reg));
        return self;
      });

  lua.new_usertype<RowRef>(
      "RowRef",
      "field",
      [](RowRef& self, const std::string& property) {
        FieldRef f;
        f.row = self;
        f.property = property;
        return f;
      },
      "set",
      [&regs](RowRef& self, const std::string& property, const sol::object& value) {
        FieldRef f;
        f.row = self;
        f.property = property;
        regs.fieldSets.push_back(mutationFromField(f, solToJson(value)));
        return self;
      });

  lua.new_usertype<FieldRef>(
      "FieldRef",
      "set",
      [&regs](FieldRef& self, const sol::object& value) {
        regs.fieldSets.push_back(mutationFromField(self, solToJson(value)));
        return self;
      });

  lua.new_usertype<PakBuilder>(
      "PakBuilder",
      "Value",
      [&regs, mapFnKeep](PakBuilder& self, sol::object value) {
        std::int64_t number = 0;
        nlohmann::json jsonValue;
        if (value.is<std::int64_t>()) {
          number = value.as<std::int64_t>();
          jsonValue = number;
        } else if (value.is<int>()) {
          number = value.as<int>();
          jsonValue = static_cast<int>(number);
        } else if (value.is<double>()) {
          const double d = value.as<double>();
          number = static_cast<std::int64_t>(d * 10.0);
          jsonValue = d;
        } else {
          jsonValue = solToJson(value);
          if (jsonValue.is_number_integer())
            number = jsonValue.get<std::int64_t>();
          else if (jsonValue.is_number()) {
            const double d = jsonValue.get<double>();
            number = static_cast<std::int64_t>(d * 10.0);
          } else
            throw std::runtime_error("pak.create(...):Value expects a number");
        }

        PakVariant variant;
        variant.valueNumber = number;
        variant.zip = self.zip;
        variant.zipTemplate = self.zipTemplate;

        if (self.asset && self.assetMapFn.valid()) {
          const AssetRef asset = *self.asset;
          auto kept = self.assetMapFn;
          auto luaKeep = regs.lua;
          const std::string arrayPointer = "/Rows";
          variant.assetFileName = asset.fileName;
          variant.relativeDirectory = asset.relativeDirectory;
          variant.assetApply = [kept, luaKeep, arrayPointer](Mod::JsonAssetEditor& editor) {
            sol::state_view view(*luaKeep);
            editor.mapArray(arrayPointer, [&](nlohmann::json item) {
              auto call = kept(jsonToSol(view, item));
              if (!call.valid()) {
                sol::error err = call;
                throw std::runtime_error(std::string("pak.create map failed: ") + err.what());
              }
              return solToJson(call.get<sol::object>());
            });
          };
        } else if (self.field) {
          variant.mutation = mutationFromField(*self.field, jsonValue);
        } else {
          throw std::runtime_error("pak.create(...):Value called on invalid builder");
        }

        regs.pakVariants.push_back(std::move(variant));
        self.lastVariantIndex = regs.pakVariants.size() - 1;
        return self;
      },
      "zip",
      sol::overload(
          [&regs](PakBuilder& self) {
            self.zip = true;
            self.zipTemplate = std::nullopt;
            if (self.lastVariantIndex && *self.lastVariantIndex < regs.pakVariants.size()) {
              regs.pakVariants[*self.lastVariantIndex].zip = true;
              regs.pakVariants[*self.lastVariantIndex].zipTemplate = std::nullopt;
            }
            return self;
          },
          [&regs](PakBuilder& self, const std::string& templ) {
            self.zip = true;
            self.zipTemplate = templ;
            if (self.lastVariantIndex && *self.lastVariantIndex < regs.pakVariants.size()) {
              regs.pakVariants[*self.lastVariantIndex].zip = true;
              regs.pakVariants[*self.lastVariantIndex].zipTemplate = templ;
            }
            return self;
          }));

  sol::table utool = lua["utool"];
  utool.set_function("asset", [&regs](const std::string& path) {
    AssetRef ref;
    ref.regs = &regs;
    splitAssetPath(path, ref.fileName, ref.relativeDirectory);
    if (ref.fileName.empty())
      throw std::runtime_error("utool.asset requires a file name");
    return ref;
  });

  sol::table pak = lua.create_table();
  pak.set_function(
      "create",
      sol::overload(
          [&regs](const FieldRef& field) {
            PakBuilder builder;
            builder.regs = &regs;
            builder.field = field;
            return builder;
          },
          [&regs, mapFnKeep](AssetRef asset, sol::protected_function mapFn) {
            mapFnKeep->push_back(mapFn);
            PakBuilder builder;
            builder.regs = &regs;
            builder.asset = std::move(asset);
            builder.assetMapFn = mapFnKeep->back();
            return builder;
          }));
  utool["pak"] = pak;

  utool.set_function("mod", [&regs](const sol::table& table) {
    if (regs.modManifest)
      throw std::runtime_error("utool.mod may only be called once");

    auto optString = [](const sol::table& t, const char* key) -> std::optional<std::string> {
      sol::object obj = t[key];
      if (!obj.valid() || obj.get_type() == sol::type::nil)
        return std::nullopt;
      if (obj.is<std::string>())
        return obj.as<std::string>();
      return std::nullopt;
    };
    auto optBool = [](const sol::table& t, const char* key) -> std::optional<bool> {
      sol::object obj = t[key];
      if (!obj.valid() || obj.get_type() == sol::type::nil)
        return std::nullopt;
      if (obj.is<bool>())
        return obj.as<bool>();
      return std::nullopt;
    };

    Core::ModManifest m;
    m.id = table.get_or<std::string>("id", "");
    m.name = table.get_or<std::string>("name", "");
    m.version = table.get_or<std::string>("version", "1.0.0");
    m.description = optString(table, "description");
    m.author = optString(table, "author");

    {
      sol::object uv = table["updateVersion"];
      if (uv.valid() && uv.get_type() != sol::type::nil) {
        if (uv.is<std::string>())
          m.updateVersion = uv.as<std::string>();
        else if (uv.is<int>())
          m.updateVersion = std::to_string(uv.as<int>());
        else if (uv.is<double>())
          m.updateVersion = std::to_string(static_cast<std::int64_t>(uv.as<double>()));
      }
    }

    sol::object targetObj = table["target"];
    if (targetObj.valid() && targetObj.is<sol::table>()) {
      sol::table target = targetObj.as<sol::table>();
      Core::Ue4Target t;
      t.gameId = optString(target, "gameId");
      t.engineVersion = optString(target, "engineVersion");
      t.minGameVersion = optString(target, "minGameVersion");
      t.maxGameVersion = optString(target, "maxGameVersion");
      m.target = t;
    }

    sol::object scriptsObj = table["scripts"];
    if (scriptsObj.valid() && scriptsObj.is<sol::table>()) {
      scriptsObj.as<sol::table>().for_each([&](const sol::object&, const sol::object& value) {
        if (value.is<std::string>())
          m.scripts.push_back(value.as<std::string>());
      });
    }

    sol::object rootsObj = table["contentRoots"];
    if (rootsObj.valid() && rootsObj.is<sol::table>()) {
      m.contentRoots.clear();
      rootsObj.as<sol::table>().for_each([&](const sol::object&, const sol::object& value) {
        if (value.is<std::string>())
          m.contentRoots.push_back(value.as<std::string>());
      });
      if (m.contentRoots.empty())
        m.contentRoots = {"content"};
    }

    sol::object pakObj = table["pak"];
    if (pakObj.valid() && pakObj.is<sol::table>()) {
      sol::table pakTable = pakObj.as<sol::table>();
      Core::ModPakSettings pakSettings;
      pakSettings.output = optString(pakTable, "output");
      pakSettings.mountPoint = optString(pakTable, "mountPoint");
      pakSettings.sourcePak = optString(pakTable, "sourcePak");
      pakSettings.curveSourcePak = optString(pakTable, "curveSourcePak");
      pakSettings.sourceFilter = optString(pakTable, "sourceFilter");
      if (auto u = optBool(pakTable, "useUnrealPak"))
        pakSettings.useUnrealPak = *u;
      else
        pakSettings.useUnrealPak = true;
      if (auto k = optBool(pakTable, "keepCache"))
        pakSettings.keepCache = *k;
      sol::object zipObj = pakTable["zip"];
      if (zipObj.valid() && zipObj.get_type() != sol::type::nil) {
        if (zipObj.is<bool>()) {
          pakSettings.zip = zipObj.as<bool>();
        } else if (zipObj.is<std::string>()) {
          pakSettings.zip = true;
          pakSettings.zipTemplate = zipObj.as<std::string>();
        }
      }
      m.pak = pakSettings;
    }

    if (m.id.empty() || m.name.empty())
      throw std::runtime_error("utool.mod requires id and name");
    regs.modManifest = std::move(m);
  });
}

}  // namespace UTool::Lua
